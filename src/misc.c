/*
 * misc.c
 */

#include "misc.h"
#include "delta.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <zip.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "utlist.h"
#include "debug.h"
#ifdef SUPPORT_LOGGING_INFO
#include "diagnostic.h"
#endif

static int libzip_archive_add_file(struct zip* za, const char* path, const char* base);
static int libzip_archive_add_dir(struct zip* za, const char* path, const char* base);
static char* libzip_get_error(int ze);

size_t ua_rw_buff_size = 16 * 1024;

struct sha256_list {
	struct sha256_list* next;
	unsigned char sha256[SHA256_DIGEST_LENGTH];
};

int sha256cmp(struct sha256_list* a, struct sha256_list* b)
{
	return memcmp(a->sha256, b->sha256, SHA256_DIGEST_LENGTH);
}

uint64_t currentms(void)
{
	struct timespec tp;

	clock_gettime(CLOCK_MONOTONIC_RAW, &tp);

	return ((uint64_t)tp.tv_sec) * 1000ULL +
	       tp.tv_nsec / 1000000ULL;

}


int mkdirp(const char* path, int umask)
{
	char* dpath = f_strdup(path);
	char* aux   = dpath;
	int rc;
	struct stat statb;

	if (*aux != '/') {
		errno = EINVAL;
		free(dpath);
		return 1;
	}

	while (*(aux+1) == '/') { aux++; }

	if (!*(aux+1)) { free(dpath); return 0; }

	while (1) {
		*aux = '/';
		aux++;
		if (!*aux) { break; }
		aux = strchr(aux, '/');

		if (aux) {
			while (*(aux+1) == '/') { aux++; }
			*aux = 0;
		}

		rc = stat(dpath, &statb);

		if (rc || !S_ISDIR(statb.st_mode)) {
			rc = mkdir(dpath, umask);
			if (rc) { free(dpath); return rc; }
		}

		if (!aux) { break; }

	}

	free(dpath);
	return 0;

}


int rmdirp(const char* path)
{
	int err = E_UA_OK;
	DIR* dir;
	struct dirent* entry;
	char* filepath;

	A_INFO_MSG("removing dir %s", path);

	do {
		BOLT_SYS(!(dir = opendir(path)), "failed to open directory %s", path);

		// $TODO: this whole QNX business doesn't belong here, and must be moved
		// out to the porting layer, giving some API for how to traverse
		// directories

		while ((entry = readdir(dir)) != NULL) {
			filepath = JOIN(path, entry->d_name);
		#ifdef __QNX__
			struct stat stat_buf;
			stat(filepath, &stat_buf);
			if (!S_ISDIR(stat_buf.st_mode)) {
		#else
			if (entry->d_type != DT_DIR) {
		#endif
				if (remove(filepath) < 0) {
					err = E_UA_SYS;
					A_ERROR_MSG("error removing file: %s", filepath);
				}
			} else if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				err = rmdirp(filepath);
			}

			free(filepath);
			if (err) break;
		}

		closedir(dir);
		BOLT_SYS(remove(path), "error removing directory: %s", path);

	} while (0);

	return err;
}


int chkdirp(const char* path)
{
	int rc;
	struct stat statb;
	char* dir = f_dirname(path);

	rc = stat(dir, &statb);

	if (rc || !S_ISDIR(statb.st_mode)) {
		rc = mkdirp(dir, 0755);
	}

	free(dir);
	return rc;
}


int newdirp(const char* path, int umask)
{
	int rc;
	struct stat statb;

	rc = stat(path, &statb);

	if (!rc) {
		rmdirp(path);
	}

	rc = mkdirp(path, umask);

	return rc;
}

#ifndef SHELL_COMMAND_DISABLE
int run_cmd(const char* cmd, char* const argv[])
{
	int rc     = E_UA_OK;
	int status = 0;

	if (cmd && !is_cmd_runnable(cmd) && argv) {
		pid_t pid=fork();

		if ( pid == -1) {
			A_ERROR_MSG("fork failed");
			rc = E_UA_SYS;

		}else if ( pid == 0) {
			execvp(cmd, argv);
			A_ERROR_MSG("execv %s failed", cmd);
			rc = E_UA_SYS;

		}else {
			if (waitpid(pid, &status, 0) == -1) {
				rc = E_UA_SYS;
			}else {
				rc = WEXITSTATUS(status);
			}
		}
	}else{
		rc = E_UA_SYS;
	}

	return rc;
}
#endif

static char* libzip_get_error(int ze)
{
	int se = errno;
	char buf[256];
	char* buf2 = 0;
	int x      = zip_error_to_str(buf, 256, ze, se);

	if (x >= 255) {
		buf2 = f_malloc(x+1);
		zip_error_to_str(buf2, x+1, ze, se);
	}

	if (buf2) { return buf2; }
	return f_strdup(buf);

}

static int libzip_unzip(const char* archive, const char* path)
{
	int i, len, fd, zerr, err = E_UA_OK;
	char* buf = 0;
	long sum;
	char* aux   = 0;
	char* fpath = 0;
	struct zip* za;
	struct zip_file* zf;
	struct zip_stat sb;

	A_INFO_MSG("unzipping(libzip) archive %s to %s", archive, path);

	do {
		BOLT_IF(!(za = zip_open(archive, ZIP_RDONLY, &zerr)), E_UA_ERR,
		        "failed to open file as ZIP %s : %s", archive, aux = libzip_get_error(zerr));

		BOLT_MALLOC(buf, ua_rw_buff_size);

		for (i = 0; i < zip_get_num_entries(za, 0); i++) {
			BOLT_IF(zip_stat_index(za, i, 0, &sb), E_UA_ERR, "failed reading stat at index %d: %s", i, zip_strerror(za));

			fpath = JOIN(path, sb.name);
			len   = strlen(sb.name);
			if (sb.name[len - 1] == '/') {
				BOLT_SYS(mkdirp(fpath, 0755) && (errno != EEXIST), "failed to make directory %s", fpath);
			} else {
				BOLT_IF(!(zf = zip_fopen_index(za, i, 0)), E_UA_ERR, "failed to open/find %s: %s", sb.name, zip_strerror(za));
				BOLT_SYS(chkdirp(fpath), "failed to prepare directory for %s", fpath);
				BOLT_SYS((fd = open(fpath, O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0, "failed to open/create %s", fpath);

				sum = 0;
				while (sum != (long)sb.size) {
					BOLT_IF((len = zip_fread(zf, buf, ua_rw_buff_size)) == -1,
					        E_UA_ERR, "error reading %s : %s", sb.name, zip_file_strerror(zf));
					BOLT_IF((write(fd, buf, len) < len), E_UA_ERR, "error writing %s", sb.name);
					sum += len;
				}

				close(fd);
				zip_fclose(zf);
			}
			free(fpath);
			fpath = 0;
		}

	} while (0);

	if (buf) free(buf);
	if (za && zip_close(za)) { err = E_UA_ERR;  A_ERROR_MSG("failed to close zip archive %s : %s", archive, zip_strerror(za)); }

	if (err) {
		if (aux) free(aux);
		if (fpath) free(fpath);
	}

	return err;
}

#ifndef SHELL_COMMAND_DISABLE
static int zip_unzip(const char* archive, const char* path)
{
	int err = E_UA_OK;

	A_INFO_MSG("unzipping(zip) archive %s to %s", archive, path);
	if (archive && path) {
		do {
			BOLT_SYS(chkdirp(archive), "failed to prepare directory for %s", path);

			char* op_a = f_asprintf("%s", archive);
			char* op_p = f_asprintf("%s", path);
			char* const argv[] = {"unzip", "-o", op_a, "-d", op_p, NULL};
			A_INFO_MSG("shell command(%s %s %s %s %s)\n", argv[0], argv[1], argv[2], argv[3], argv[4]);
			BOLT_SYS(run_cmd(argv[0], argv), "failed to unzip files");
			free(op_a);
			free(op_p);
		} while (0);

	}

	return err;
}
#endif

int unzip(const char* archive, const char* path)
{
#ifndef SHELL_COMMAND_DISABLE
	int err = zip_unzip(archive, path);
#else
	int err = E_UA_ERR;
#endif
	if (err != E_UA_OK) {
		err = libzip_unzip(archive, path);
	}

	return err;
}

static int libzip_zip(const char* archive, const char* path)
{
	int zerr, err = E_UA_OK;
	char* aux = 0;
	struct stat path_stat;
	struct zip* za = 0;

	A_INFO_MSG("zipping(libzip) %s to %s", path, archive);

	do {
		BOLT_SYS(stat(path, &path_stat), "failed to get path status: %s", path);
		BOLT_IF(!S_ISREG(path_stat.st_mode) && !S_ISDIR(path_stat.st_mode), E_UA_ERR, "path is not file or directory");
		BOLT_SYS(chkdirp(archive), "failed to prepare directory for %s", archive);
		BOLT_IF(!(za = zip_open(archive, ZIP_CREATE | ZIP_TRUNCATE, &zerr)), E_UA_ERR,
		        "failed to create zip file %s : %s", archive, aux = libzip_get_error(zerr));

		err = S_ISREG(path_stat.st_mode) ? libzip_archive_add_file(za, path, 0) : libzip_archive_add_dir(za, path, 0);

	} while (0);

	if (za && zip_close(za)) { err = E_UA_ERR;  A_ERROR_MSG("failed to close zip archive %s : %s", archive, zip_strerror(za)); }

	if (err) {
		if (aux) free(aux);
		if (za) zip_discard(za);
	}

	return err;
}

#ifndef SHELL_COMMAND_DISABLE
static int zip_zip(const char* archive, const char* path)
{
	A_INFO_MSG("zipping(zip) %s to %s", path, archive);

	int err = E_UA_OK;

	if (archive && path) {
		char* wd = get_current_dir_name();
		do {
			BOLT_SYS(chdir(path), "failed to chdir to %s", path);
			BOLT_SYS(chkdirp(archive), "failed to prepare directory for %s", archive);
			remove(archive);

			char* op = f_asprintf("%s", archive);
			char* const argv[] = {"zip", "-r", op, ".", NULL};
			A_INFO_MSG("shell command(%s %s %s %s)\n", argv[0], argv[1], argv[2], argv[3]);
			BOLT_SYS(run_cmd(argv[0], argv), "failed to zip files");
			free(op);
		} while (0);

		chdir(wd);
		f_free(wd);
	}
	return err;
}
#endif

int zip(const char* archive, const char* path)
{
#ifndef SHELL_COMMAND_DISABLE
	int err = zip_zip(archive, path);
#else
	int err = E_UA_ERR;
#endif

	if (err != E_UA_OK)
		err = libzip_zip(archive, path);

	return err;
}

static int libzip_archive_add_file(struct zip* za, const char* path, const char* base)
{
	int err = E_UA_OK;
	zip_source_t* s;
	char* file;

	A_INFO_MSG("adding file %s to zip", path);

	do {
		char* bname = f_basename(path);
		file = JOIN(SAFE_STR(base), bname);
		free(bname);

		BOLT_IF(!(s = zip_source_file(za, path, 0, 0)), E_UA_ERR, "failed to source file %s : %s", path, zip_strerror(za));

		if (zip_file_add(za, file, s, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0) {
			zip_source_free(s);
			BOLT_SAY(E_UA_ERR, "error adding file: %s", zip_strerror(za));
		}

	} while (0);

	free(file);
	return err;
}


static int libzip_archive_add_dir(struct zip* za, const char* path, const char* base)
{
	int err = E_UA_OK;
	DIR* dir;
	struct dirent* entry;
	char* filepath;
	char* basepath;

	A_INFO_MSG("adding directory %s to zip", path);

	do {
		BOLT_IF (!(dir = opendir(path)), E_UA_ERR, "failed to open directory %s", path);

		while ((entry = readdir(dir)) != NULL) {
			filepath = JOIN(path, entry->d_name);
			basepath = JOIN(SAFE_STR(base), entry->d_name);

			if (entry->d_type != DT_DIR) {
				err = libzip_archive_add_file(za, filepath, base);

			} else if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				if (zip_add_dir(za, basepath) < 0) {
					A_ERROR_MSG("error adding directory: %s", basepath);
					err = E_UA_ERR;
				} else {
					libzip_archive_add_dir(za, filepath, basepath);
				}

			}

			free(filepath);
			free(basepath);

			if (err) break;
		}

		closedir(dir);

	} while (0);

	return err;
}


int libzip_find_file(const char* archive, const char* path)
{
	int zerr, err = E_UA_OK;
	char* aux = 0;
	struct zip* za;

	do {
		BOLT_IF(!(za = zip_open(archive, ZIP_RDONLY, &zerr)), E_UA_ERR,
		        "failed to open file as zip %s : %s", archive, aux = libzip_get_error(zerr));
		if (zip_name_locate(za, path, 0) < 0) {
			A_ERROR_MSG("file: %s not found in zip: %s", path, archive);
			err = E_UA_ERR;
		} else {
			A_ERROR_MSG("file: %s found in zip: %s", path, archive);
			#ifdef SUPPORT_LOGGING_INFO
			int ret;
			ret = store_data(0, 0, "D_START", 0, 0, 0);
			if (ret)
				A_INFO_MSG("D_START");
			#endif
			err = E_UA_OK;
		}

	} while (0);

	if (za && zip_close(za)) { err = E_UA_ERR;  A_ERROR_MSG("failed to close zip archive %s : %s", archive, zip_strerror(za)); }

	if (err) {
		if (aux) free(aux);
	}

	return err;

}


int copy_file(const char* from, const char* to)
{
	int err   = E_UA_OK;
	FILE* in  = 0;
	FILE* out = 0;
	char* buf = 0;
	size_t nread;

	A_INFO_MSG("copying file from %s to %s", from, to);

	do {
		BOLT_SYS(!(in = fopen(from, "r")), "opening file: %s", from);

		BOLT_SYS(chkdirp(to), "failed to prepare directory for %s", to);
		BOLT_SYS(!(out = fopen(to, "w")), "creating file: %s", to);

		BOLT_MALLOC(buf, ua_rw_buff_size);

		do {
			BOLT_SYS(((nread = fread(buf, sizeof(char), ua_rw_buff_size, in)) == 0) && ferror(in), "reading from file: %s", from);
			BOLT_SYS(nread && (fwrite(buf, sizeof(char), nread, out) != nread), "writing to file: %s", to);
		} while (nread);

	} while (0);

	if (buf) free(buf);
	if (in && fclose(in)) A_ERROR_MSG("closing file: %s", from);
	if (out && fclose(out)) A_ERROR_MSG("closing file: %s", to);

	return err;
}

int make_file_hard_link(const char* from, const char* to)
{
	int err = E_UA_OK;

	do {
		A_INFO_MSG("Making hardlink from %s to %s", from, to);
		if (!access(to, F_OK)) remove(to);
		BOLT_SYS(chkdirp(to), "Error making directory path for %s", to);
		BOLT_SYS(link(from, to), "Error creating hard link from %s to %s", from, to);
	} while (0);

	return err;
}

int calc_sha256(const char* fpath, unsigned char obuff[SHA256_DIGEST_LENGTH])
{
	int err = E_UA_OK;
	FILE* file;
	SHA256_CTX ctx;
	char* buf = 0;
	size_t nread;

	do {
		BOLT_SYS(!(file = fopen(fpath, "rb")), "opening file: %s", fpath);

		BOLT_MALLOC(buf, ua_rw_buff_size);

		SHA256_Init(&ctx);

		while ((nread = fread(buf, sizeof(char), ua_rw_buff_size, file))) {
			SHA256_Update(&ctx, buf, nread);
		}

		SHA256_Final(obuff, &ctx);

		BOLT_SYS(fclose(file), "closing file: %s", fpath);

	} while (0);

	if (buf) free(buf);

	return err;
}

int calc_sha256_hex(const char* fpath, char obuff[SHA256_HEX_LENGTH])
{
	int i, err = E_UA_OK;
	unsigned char hash[SHA256_DIGEST_LENGTH];

	if (!(err = calc_sha256(fpath, hash))) {
		for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
			sprintf(obuff + (i * 2), "%02x", (unsigned char)hash[i]);
		}

		obuff[SHA256_HEX_LENGTH - 1] = 0;

	}

	return err;
}


int calc_sha256_x(const char* archive, char obuff[SHA256_B64_LENGTH])
{
	int i, zerr, err = E_UA_OK;
	char* buf = 0;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	zip_uint64_t sum = 0;
	zip_int64_t len = 0;
	char* zstr = 0;
	struct zip* za;
	struct zip_file* zf;
	struct zip_stat sb;
	SHA256_CTX ctx;
	struct sha256_list* sl         = 0;
	struct sha256_list* aux        = 0;
	struct sha256_list* sha256List = 0;

	do {
		BOLT_IF(!(za = zip_open(archive, ZIP_RDONLY, &zerr)), E_UA_ERR,
		        "failed to open file as ZIP %s : %s", archive, zstr = libzip_get_error(zerr));

		BOLT_MALLOC(buf, ua_rw_buff_size);

		for (i = 0; i < zip_get_num_entries(za, 0); i++) {
			BOLT_IF(zip_stat_index(za, i, 0, &sb), E_UA_ERR, "failed reading stat at index %d: %s", i, zip_strerror(za));

			if (sb.name[strlen(sb.name) - 1] != '/') {
				if (!strcmp(sb.name, MANIFEST) ||
				    !strcmp(sb.name, MANIFEST_DIFF) ||
				    !strncmp(sb.name, XL4_X_PREFIX, strlen(XL4_X_PREFIX)) ||
				    !strncmp(sb.name, XL4_SIGNATURE_PREFIX, strlen(XL4_SIGNATURE_PREFIX)))
					continue;

				BOLT_IF(!(zf = zip_fopen_index(za, i, 0)), E_UA_ERR, "failed to open/find %s: %s", sb.name, zip_strerror(za));

				SHA256_Init(&ctx);
				SHA256_Update(&ctx, sb.name, strlen(sb.name));

				sum = 0;
				len = (zip_int64_t)ua_rw_buff_size;
				while (len == (zip_int64_t)ua_rw_buff_size) {
					BOLT_IF((len = zip_fread(zf, buf, (zip_uint64_t)ua_rw_buff_size)) == -1, E_UA_ERR, "error reading %s : %s", sb.name, zip_file_strerror(zf));
					if(len > 0) {
						SHA256_Update(&ctx, buf, len);
						sum += (zip_uint64_t)len;
					}
				}

				if (sum != sb.size) {
					A_INFO_MSG("ZIP warning: reported size of file %s is %" PRIu64 ", total bytes read: %" PRIu64 "", sb.name, sb.size, sum);
				}

				sl = f_malloc(sizeof(struct sha256_list));
				SHA256_Final(sl->sha256, &ctx);
				LL_APPEND(sha256List, sl);

				zip_fclose(zf);
			}
		}

		if (!err) {
			SHA256_Init(&ctx);
			LL_SORT(sha256List, sha256cmp);
			LL_FOREACH(sha256List, sl) {
				SHA256_Update(&ctx, sl->sha256, SHA256_DIGEST_LENGTH);
			}
			SHA256_Final(hash, &ctx);

			err = base64_encode(hash, obuff);
		}

	} while (0);

	if (buf) free(buf);
	if (za && zip_close(za)) { err = E_UA_ERR;  A_ERROR_MSG("failed to close zip archive %s : %s", archive, zip_strerror(za)); }

	LL_FOREACH_SAFE(sha256List, sl, aux) {
		LL_DELETE(sha256List, sl);
		free(sl);
	}

	if (err) {
		if (zstr) free(zstr);
	}

	return err;
}

int base64_encode(unsigned char hash[SHA256_DIGEST_LENGTH], char b64buff[SHA256_B64_LENGTH])
{
	int err       = E_UA_ERR;
	BIO* b64      = NULL;
	BUF_MEM* bptr = NULL;

	b64 = BIO_push(BIO_new(BIO_f_base64()), BIO_new(BIO_s_mem()));

	if (b64 && BIO_write(b64, hash, SHA256_DIGEST_LENGTH) > 0) {
		(void)BIO_flush(b64);
		BIO_get_mem_ptr(b64, &bptr);
		memcpy(b64buff, bptr->data, SHA256_B64_LENGTH - 1);
		b64buff[SHA256_B64_LENGTH - 1] = 0;
		BIO_free_all(b64);
		err = E_UA_OK;
	}

	return err;
}

int calculate_sha256_b64(const char* file, char b64buff[SHA256_B64_LENGTH])
{
	int err = E_UA_ERR;
	unsigned char hash[SHA256_DIGEST_LENGTH];

	if (!(err = calc_sha256(file, hash))) {
		if (b64buff)
			err = base64_encode(hash, b64buff);

	} else {
		A_ERROR_MSG("SHA256 Hash calculation failed : %s", file);
	}

	return err;

}

int verify_file_hash_b64(const char* file, const char* sha256_b64)
{
	int err = E_UA_ERR;
	char b64buff[SHA256_B64_LENGTH];

	if (file && sha256_b64 && !(err = calculate_sha256_b64(file, b64buff))) {
		if (!strncmp(b64buff, sha256_b64, sizeof(b64buff) - 1)) {
			A_INFO_MSG("SHA256 Hash matched %s : Expected: %s  Calculated: %s", file, sha256_b64, b64buff);
			err = E_UA_OK;

		} else {
			A_ERROR_MSG("SHA256 Hash mismatch %s : Expected: %s  Calculated: %s", file, sha256_b64, b64buff);
			err = E_UA_ERR;
		}

	}

	return err;

}

int sha256xcmp(const char* archive, char b64[SHA256_B64_LENGTH])
{
	char b64_tmp[SHA256_B64_LENGTH];

	memset(b64_tmp, 0, sizeof(b64_tmp));

	calc_sha256_x(archive, b64_tmp);

	return strcmp(b64, b64_tmp);

}

#ifndef SHELL_COMMAND_DISABLE
int is_cmd_runnable(const char* cmd)
{
	int err    = E_UA_OK;
	char* path = 0;

	do {
		if (strchr(cmd, '/')) {
			err = access(cmd, X_OK);
			break;
		}

		BOLT_IF(!(path = f_strdup(getenv("PATH"))), E_UA_ERR, "PATH env empty");
		char* pch = strtok(path, ":");
		while (pch != NULL) {
			char* p = JOIN(pch, cmd);
			if ((err = access(p, X_OK)) == 0) {
				free(p);
				break;
			}
			free(p);
			pch = strtok(NULL, ":");
		}
	} while (0);
	if (path) free(path);
	return err;
}
#endif

char* randstring(int length)
{
	const char* string     = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
	size_t stringLen = strlen(string);
	char* randomString;

	randomString = malloc(sizeof(char) * (length +1));

	if (randomString == NULL) {
		return (char*)0;
	}

	unsigned int key = 0;

	for (int n = 0; n < length; n++) {
		key             = random() % stringLen;
		randomString[n] = string[key];
	}

	randomString[length] = '\0';

	return randomString;
}

int remove_subdirs_except(char* parent_dir, char* subdir_to_keep)
{
	DIR* d;
	struct dirent* dir;
	struct stat statbuf;
	char fullpath[PATH_MAX];

	d = opendir(parent_dir);
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			snprintf(fullpath, sizeof(fullpath), "%s/%s", parent_dir, dir->d_name);
			stat(fullpath, &statbuf);
			if (S_ISDIR(statbuf.st_mode)
			    && strcmp(dir->d_name, ".")
			    && strcmp(dir->d_name, "..")
			    && strcmp(dir->d_name, subdir_to_keep)) {
				rmdirp(fullpath);

			}
		}
		closedir(d);
	}
	return(0);
}

int reply_id_matched(char* s1, char* s2)
{
	int matched = 0;

	if (s1 && s2)
		matched = !strcmp(s1, s2);
	return matched;
}
