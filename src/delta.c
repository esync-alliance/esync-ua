#include "delta.h"
#include "xml.h"
#include "utlist.h"
#include "debug.h"
#ifdef SHELL_COMMAND_DISABLE
#include "delta_utils.h"
#endif
#include <zip.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef __QNX__
#include <limits.h>
#else
#include <linux/limits.h>
#endif
#include <unistd.h>

static int add_delta_tool(delta_tool_hh_t** hash, const delta_tool_t* tool, int count, int isPatchTool);
static void clear_delta_tool(delta_tool_hh_t* hash);
static char* get_deflt_delta_cap(delta_tool_hh_t* patchTool, delta_tool_hh_t* decompTool);
static char* get_config_delta_cap(char* delta_cap);
static char* expand_tool_args(const char* args, const char* old, const char* new, const char* diff);
static int delta_patch(diff_info_t* diffInfo, const char* old, const char* new, const char* diff);
static int verify_file(const char* file, const char* sha256);

#define snprintf_nowarn(...) (snprintf(__VA_ARGS__) < 0 ? abort() : (void)0)

delta_stg_t delta_stg = {0};

const delta_tool_t deflt_patch_tools[] = {
#ifndef SHELL_COMMAND_DISABLE
	{"bsdiff", "bspatch", OFA " "NFA " "PFA, 1},
	{"rfc3284", "xdelta3", "-D -d -s "OFA " "PFA " "NFA, 0},
#endif
	{"esdiff", "espatch", OFA " "NFA " "PFA, 1}
};

const delta_tool_t deflt_decomp_tools[] = {
#ifndef SHELL_COMMAND_DISABLE
	{"gzip", "gzip", "-cd "OFA " > "NFA, 0},
	{"bzip2", "bzip2", "-cd "OFA " > "NFA, 0},
#endif
	{"xz", "xz", "-cd "OFA " > "NFA, 0}
};


int delta_init(char* cacheDir, delta_cfg_t* deltaConfig)
{
	int err = E_UA_OK;

	do {
		memset(&delta_stg, 0, sizeof(delta_stg_t));

		BOLT_IF(!S(cacheDir) || chkdirp(delta_stg.cache_dir = cacheDir), E_UA_ARG, "cache directory invalid");
		BOLT_IF(add_delta_tool(&delta_stg.patch_tool, deflt_patch_tools, sizeof(deflt_patch_tools)/sizeof(deflt_patch_tools[0]), 1), E_UA_ERR, "default patch tools adding failed");
		BOLT_IF(add_delta_tool(&delta_stg.decomp_tool, deflt_decomp_tools, sizeof(deflt_decomp_tools)/sizeof(deflt_decomp_tools[0]), 0), E_UA_ERR, "default decompression tools adding failed");

		BOLT_IF(deltaConfig && deltaConfig->patch_tools && add_delta_tool(&delta_stg.patch_tool, deltaConfig->patch_tools, deltaConfig->patch_tool_cnt, 1), E_UA_ARG, "patch tools adding failed");
		BOLT_IF(deltaConfig && deltaConfig->decomp_tools && add_delta_tool(&delta_stg.decomp_tool, deltaConfig->decomp_tools, deltaConfig->decomp_tool_cnt, 0), E_UA_ARG, "decompression tools adding failed");

		delta_stg.delta_cap = (deltaConfig && S(deltaConfig->delta_cap)) ? get_config_delta_cap(deltaConfig->delta_cap) :
		                      get_deflt_delta_cap(delta_stg.patch_tool, delta_stg.decomp_tool);
		BOLT_IF((delta_stg.delta_cap == NULL), E_UA_ERR, "delta_cap is NULL");

	} while (0);

	if (err) {
		delta_stop();
	}

	return err;
}


void delta_stop()
{
	if (delta_stg.patch_tool != NULL) {
		clear_delta_tool(delta_stg.patch_tool);
		delta_stg.patch_tool = NULL;
	}

	if (delta_stg.decomp_tool != NULL) {
		clear_delta_tool(delta_stg.decomp_tool);
		delta_stg.decomp_tool = NULL;
	}

	if (delta_stg.delta_cap != NULL) {
		free(delta_stg.delta_cap);
		delta_stg.delta_cap = NULL;
	}

}


const char* get_delta_capability()
{
	return delta_stg.delta_cap;
}


int is_delta_package(const char* pkgFile)
{
	return !libzip_find_file(pkgFile, MANIFEST_DIFF);
}

int is_prepared_delta_package(char* archive)
{
	int err               = E_UA_ERR;
	int is_prepared_delta = 0;
	zip_t* zt             = NULL;
	zip_file_t* zf        = NULL;
	char* buf             = NULL;

	do {
		int errorp;
		BOLT_IF(!archive, E_UA_ERR, "archive is NULL");

		BOLT_IF(!(zt= zip_open(archive, ZIP_RDONLY,&errorp)), E_UA_ERR, "failed to open archive %s", archive);

		zip_stat_t zsb;
		BOLT_IF(!(zf = zip_fopen(zt, MANIFEST_DIFF, 0)), E_UA_ERR, "failed to open file %s", MANIFEST_DIFF);

		BOLT_IF((zip_stat(zt, MANIFEST_DIFF, 0, &zsb) == -1), E_UA_ERR, "failed to stat file %s", MANIFEST_DIFF);
		A_INFO_MSG("size of %s is %d", MANIFEST_DIFF, zsb.size);

		BOLT_MALLOC(buf, zsb.size +1);
		BOLT_IF((zip_fread(zf, buf, zsb.size) == -1), E_UA_ERR, "failed to stat file %s", MANIFEST_DIFF);

		if (strstr(buf, "<prepared>") && strstr(buf, "</prepared>")) {
			A_INFO_MSG("Found prepared delta package");
			is_prepared_delta = 1;
		}

	} while (0);

	if (zf) zip_fclose(zf);
	if (zt) zip_close(zt);
	f_free(buf);

	return is_prepared_delta;
}

int delta_use_external_algo(void)
{
	return delta_stg.use_external_algo;
}

/**
 * Replace all occurrence of a character in given string.
 */
void replaceAll(char * str, char oldChar, char newChar)
{
    int i = 0;

    /* Run till end of string */
    while(str[i] != '\0')
    {
        /* If occurrence of character is found */
        if(str[i] == oldChar)
        {
            str[i] = newChar;
        }

        i++;
    }
}

int process_squashfs_image (const char* squashFile, diff_info_t* di, const char* pkg_dir, bool repackaging) {
	int status = E_UA_OK;
	char unsquash_cmd[PATH_MAX]    = { 0 };
	char unsq_cmd[PATH_MAX]    = { 0 };
	char squashfs_cmd[PATH_MAX]    = { 0 };
	char run_cmd[PATH_MAX]    = { 0 };
	char cp_cmd[PATH_MAX]    = { 0 };
	char str[PATH_MAX] = {0};
	FILE* pf;
	char output[64];

	char* squash_dir = JOIN(delta_stg.cache_dir, "art", pkg_dir);

	if(!repackaging) {
		A_INFO_MSG("squash_dir: %s\n", squash_dir);
		if (squash_dir) {
			if (!access(squash_dir, W_OK)) {
				rmdirp(squash_dir);
			}
		}

		int ret_dir = mkdirp(squash_dir, 0777);
		if (ret_dir != 0) {
			A_INFO_MSG("tempdir: squsash up directory creation failed\n");
		}

		memset(unsquash_cmd, 0, PATH_MAX);
		snprintf(unsquash_cmd, (PATH_MAX - 1), "%s %s %s",UNSQUASH_BIN_PATH, "-mkfs-time", (char*)squashFile);
		pf = popen(unsquash_cmd, "r");
		if (!pf) {
			A_ERROR_MSG("Could not run unsqush to determine mkfs-time");
			return -1;

		}else {
			if (fgets(output, sizeof(output), pf) != NULL) {
			}
			A_INFO_MSG("MKFS TIME: %s\n", output);
			pclose(pf);
		}
		snprintf(unsq_cmd, (PATH_MAX - 1), "%s/%s", squash_dir, "test.bin");
	}else {
		snprintf(unsq_cmd, (PATH_MAX - 1), "%s/%s", squash_dir, "test1.bin");
	}

	memset(run_cmd, 0, PATH_MAX);
	snprintf_nowarn(run_cmd, (PATH_MAX - 1), "%s %s %s %s", UNSQUASH_BIN_PATH, "-d", unsq_cmd, (char*)squashFile);
	
	A_INFO_MSG("unsquash command: %s\n", run_cmd);
	if(system(run_cmd)) {
		A_ERROR_MSG("failed to unsquash: %s\n", squashFile);
		return E_UA_SYS;
	}

	memset(squashfs_cmd, 0, PATH_MAX);
	memset(str, 0, PATH_MAX);

	if(!repackaging) {
		snprintf(str, (PATH_MAX - 1), "%s", di->old_name);
		replaceAll(str, '/', '%');
		snprintf_nowarn(squashfs_cmd, (PATH_MAX - 1), "%s %s %s/%s %s %s", SQUASH_BIN_PATH, unsq_cmd, squash_dir ,str, "-noI -noId -noD -noF -noX -noappend -mkfs-time" ,output);
	}else {
		snprintf(str, (PATH_MAX - 1), "%s", di->name);
		replaceAll(str, '/', '%');
		snprintf_nowarn(squashfs_cmd, (PATH_MAX - 1), "%s %s %s/%s %s", SQUASH_BIN_PATH, unsq_cmd, squash_dir , str, di->nesting.un_squash_fs.mk_squash_fs_options);
	}
	
	memset(run_cmd, 0, PATH_MAX);
	snprintf_nowarn(run_cmd, (PATH_MAX - 1), "%s", squashfs_cmd);
	
	sleep(1);
	A_INFO_MSG("mksquashfs command: %s\n", run_cmd);
	if(system(run_cmd)) {
		A_ERROR_MSG("failed to mksquash: %s\n", squashFile);
   		return E_UA_SYS;;
	}
	
	if(repackaging) {
		if (remove(squashFile) < 0) {
			status = E_UA_SYS;
			A_ERROR_MSG("error removing file: %s\n", squashFile);
		}
		memset(cp_cmd, 0, PATH_MAX);
		snprintf_nowarn(cp_cmd, (PATH_MAX - 1), "%s/%s", squash_dir, str);
		A_INFO_MSG("copying file from %s to %s \n", cp_cmd, squashFile);
		copy_file(cp_cmd,squashFile);
		if(verify_file(squashFile, di->sha256.new) != E_UA_OK) {
			status = E_UA_ERR;
		}
	}
	
	rmdirp(unsq_cmd);
	return status;
}



int delta_reconstruct(const char* oldPkgFile, const char* diffPkgFile, const char* newPkgFile)
{
	int err = E_UA_OK;
	diff_info_t* di, * aux, * diList = 0;
	char* oldFile, * diffFile, * newFile;
	char* oldPath       = 0, * diffPath = 0, * newPath = 0;
	char* diff_manifest = 0, * manifest_old = 0, * manifest_diff = 0, * manifest_new = 0;
	char* top_delta_dir = 0, * pkg_dir = 0, * p = 0;
      char* delta_pkg_dir = 0;

	do {
		pkg_dir = f_basename(diffPkgFile);
		if ((p =strrchr(pkg_dir, '.'))) *p = 0;
		
		top_delta_dir = JOIN(delta_stg.cache_dir, "delta");
		delta_pkg_dir = JOIN(top_delta_dir, pkg_dir);
		A_INFO_MSG("delta_pkg_dir: %s", delta_pkg_dir);
		if (delta_pkg_dir) {
 			if (!access(delta_pkg_dir, W_OK))
				rmdirp(delta_pkg_dir);
		}

#define DTR_MK(type) \
	BOLT_SYS(newdirp(type ## Path = JOIN(top_delta_dir, pkg_dir, # type), 0755) && (errno != EEXIST), "failed to make directory %s", type ## Path); \
	manifest_ ## type = JOIN(type ## Path, MANIFEST); do { \
	} while (0)

		DTR_MK(old);
		DTR_MK(diff);
		DTR_MK(new);

#undef DTR_MK

		diff_manifest = JOIN(diffPath, MANIFEST_DIFF);

		BOLT_IF(unzip(oldPkgFile, oldPath), E_UA_ERR, "unzip failed: %s", oldPkgFile);
		BOLT_IF(unzip(diffPkgFile, diffPath), E_UA_ERR, "unzip failed: %s", diffPkgFile);

		BOLT_IF(parse_diff_manifest(diff_manifest, &diList), E_UA_ERR, "failed to parse: %s", diff_manifest);

		DL_FOREACH_SAFE(diList, di, aux) {
			if (!err) {
				bool squashfsDone = 0;
				if(di->old_name != NULL)
					oldFile  = JOIN(oldPath, di->old_name);
				else
					oldFile  = JOIN(oldPath, di->name);

				diffFile = JOIN(diffPath, di->name);
				newFile  = JOIN(newPath, di->name);

				if(di->nesting.un_squash_fs.un_squash_fs && di->nesting.squash_fs_uncompressed.squash_fs_uncompressed && di->nesting.single_file_delta.single_file_delta) {

					if(process_squashfs_image(oldFile, di, pkg_dir, 0) != E_UA_OK) err = E_UA_ERR;
					
					char str[PATH_MAX] = {0};
					snprintf(str, (PATH_MAX - 1), "%s", di->old_name);
					replaceAll(str, '/', '%');
					oldFile = JOIN(delta_stg.cache_dir, "art", pkg_dir, str);
					squashfsDone = 1;
				}

				A_INFO_MSG("oldFile: %s", oldFile);
				A_INFO_MSG("diffFile: %s", diffFile);
				A_INFO_MSG("newFile: %s", newFile);

				if (di->type == DT_ADDED) {
					if (verify_file(diffFile, di->sha256.new) || copy_file(diffFile, newFile)) err = E_UA_ERR;

				} else if (di->type == DT_UNCHANGED) {
					if (verify_file(oldFile, di->sha256.old) || copy_file(oldFile, newFile)) err = E_UA_ERR;

				} else if (di->type == DT_CHANGED) {
					if(squashfsDone)  {
						if (delta_patch(di, oldFile, newFile, diffFile)) err = E_UA_ERR;
					} else {
						if (verify_file(oldFile, di->sha256.old) || delta_patch(di, oldFile, newFile, diffFile) || verify_file(newFile, di->sha256.new)) err = E_UA_ERR;
					}
				}

				if (!access(oldFile, R_OK))
					remove(oldFile);
				if (!access(diffFile, R_OK))
					remove(diffFile);

				if(squashfsDone) {
					if(di->nesting.un_squash_fs.un_squash_fs && di->nesting.squash_fs_uncompressed.squash_fs_uncompressed && di->nesting.single_file_delta.single_file_delta) {
						if (process_squashfs_image(newFile, di, pkg_dir, 1) != E_UA_OK) err = E_UA_ERR;
					}
					squashfsDone = 0;		
				}

				free(oldFile);
				free(diffFile);
				free(newFile);

			}

			DL_DELETE(diList, di);
			free_diff_info(di);

		}

		if (!err) {
			rmdirp(oldPath);
			BOLT_IF(copy_file(manifest_diff, manifest_new), E_UA_ERR, "failed to copy manifest");
			rmdirp(diffPath);
			BOLT_IF(zip(newPkgFile, newPath), E_UA_ERR, "zip failed: %s", newPkgFile);

		}

	} while (0);

	char* tempDir = JOIN(delta_stg.cache_dir, "art", pkg_dir);
	if (!access(tempDir, R_OK)) {
		rmdirp(tempDir);
		
	}

#define DTR_RM(type) \
	if ((type ## Path) ) { if (!access(type ## Path, F_OK) && rmdirp(type ## Path)) A_ERROR_MSG("error removing directory %s", type ## Path); free(type ## Path); } \
	Z_FREE(manifest_ ## type); do { } while (0)

	DTR_RM(old);
	DTR_RM(diff);
	DTR_RM(new);

#undef DTR_RM
	p = JOIN(top_delta_dir, pkg_dir);
	rmdir(p);
	f_free(p);
	f_free(pkg_dir);
	f_free(top_delta_dir);
	Z_FREE(diff_manifest);

	return err;
}


static int add_delta_tool(delta_tool_hh_t** hash, const delta_tool_t* tool, int count, int isPatchTool)
{
	int i, err = E_UA_OK;
	delta_tool_hh_t* dth, * aux;

	for (i = 0; i < count; i++) {
		if (S((tool + i)->algo) && S((tool + i)->path) && S((tool + i)->args) &&
#ifndef SHELL_COMMAND_DISABLE
		    !is_cmd_runnable((tool + i)->path) &&
#endif
		    (SUBSTRCNT((tool + i)->args, OFA) == 1) && (SUBSTRCNT((tool + i)->args, NFA) == 1) && (SUBSTRCNT((tool + i)->args, PFA) == (isPatchTool ? 1 : 0))) {
			dth            = f_malloc(sizeof(delta_tool_hh_t));
			dth->tool.algo = STRLWR(f_strdup((tool + i)->algo));
			dth->tool.path = f_strdup((tool + i)->path);
			dth->tool.args = f_strdup((tool + i)->args);
			dth->tool.intl = (tool + i)->intl;
			HASH_FIND_STR(*hash, dth->tool.algo, aux);
			if (!aux) {
				HASH_ADD_STR(*hash, tool.algo, dth);
			} else {
				HASH_REPLACE_STR(*hash, tool.algo, dth, aux);
				free_delta_tool_hh(aux);
			}
		} else if (tool != deflt_patch_tools && tool != deflt_decomp_tools) {
			A_ERROR_MSG("failed to add %s tool: %s %s", (isPatchTool ? "patch" : "diff"), SAFE_STR((tool + i)->algo), SAFE_STR((tool + i)->path));
			err = E_UA_ERR;
			break;
		}
	}

	return err;
}


static void clear_delta_tool(delta_tool_hh_t* hash)
{
	delta_tool_hh_t* dth, * aux;

	if (!hash) { return; }

	HASH_ITER(hh, hash, dth, aux) {
		HASH_DEL(hash, dth);
		free_delta_tool_hh(dth);

	}

}


static int get_espatch_version(char* ver, int len)
{
	int rc = E_UA_OK;

	if (ver) {
#ifndef SHELL_COMMAND_DISABLE
		char output[64];
		FILE* pf;

		pf = popen("espatch -v", "r");
		if (!pf) {
			A_ERROR_MSG("Could not run espatch to determine its version, using default V2.5.");
			rc = E_UA_ERR;

		}else {
			if (fgets(output, sizeof(output), pf) != NULL) {
				if (output[strlen(output) - 1] == '\n')
					output[strlen(output) - 1] = 0;
				char* tmp = strstr(output, "version: ");
				if (tmp && (strlen(tmp)- strlen("version: ") < len-1)) {
					memset(ver, 0, len);
					strcpy_s(ver, tmp+strlen("version: "), len);
					A_INFO_MSG("espatch outputs version: %s", ver);
				} else {
					A_ERROR_MSG("error: could not parse version from espatch output: %s.", output);
					rc = E_UA_ERR;
				}

			} else {
				A_ERROR_MSG("error: espatch has no output.");
				rc = E_UA_ERR;
			}

			pclose(pf);
		}
#else
		rc = E_UA_ERR;
		const char* version = espatch_get_version();
		if (version) {
			strcpy_s(ver, version, strlen(version)+1);
			rc = E_UA_OK;
		}
#endif
	}

	return rc;
}

static char* get_config_delta_cap(char* delta_cap)
{
	char espatch_ver[7];
	char* ret_cap       = NULL;

//	bzero(espatch_ver, sizeof(espatch_ver));
	memset(espatch_ver, 0, sizeof(espatch_ver));

	if (delta_cap) {
		char* tmp_cap = f_strdup(delta_cap);
		char* tmp     = strtok(tmp_cap, ";");
		if (tmp && *tmp == 'A') {
			char* format = strstr(tmp, ":");
			while (format) {
				if (!strstr(delta_cap, "E:") && *(format+1) == '3') {
					if (E_UA_OK == get_espatch_version(espatch_ver, sizeof(espatch_ver)))
						ret_cap = f_asprintf("%s;E:%s", delta_cap, espatch_ver);
				} else if (*(format+1) == '4') {
					delta_stg.use_external_algo = 1;
				}

				tmp    = format + 1;
				format = strstr(tmp, ",");
			}

		}

		if (ret_cap == NULL)
			ret_cap = f_strdup(delta_cap);
		free(tmp_cap);
	}

	return ret_cap;
}

static char* get_deflt_delta_cap(delta_tool_hh_t* patchTool, delta_tool_hh_t* decompTool)
{
	int memory            = 100;
	int espatch_ver_valid = E_UA_ERR;
	delta_tool_hh_t* dth  = 0;
	char* delta_cap       = NULL;

	char format[16];
	char compression[16];
	char espatch_ver[7];
//	bzero(format, sizeof(format));
//	bzero(compression, sizeof(compression));
//	bzero(espatch_ver, sizeof(espatch_ver));
	memset(format, 0, sizeof(format));
	memset(compression, 0, sizeof(compression));
	memset(espatch_ver, 0, sizeof(espatch_ver));
	strcpy_s(format, "A:", sizeof(format));
	strcpy_s(compression, "B:", sizeof(compression));

	HASH_FIND_STR(patchTool, "bsdiff", dth);
	if (dth) strcat(format, "1,");
	HASH_FIND_STR(patchTool, "rfc3284", dth);
	if (dth) strcat(format, "2,");
	HASH_FIND_STR(patchTool, "esdiff", dth);
	if (dth) {
		strcat(format, "3,");
		espatch_ver_valid = get_espatch_version(espatch_ver, sizeof(espatch_ver));
	}

	HASH_FIND_STR(decompTool, "gzip", dth);
	if (dth) strcat(compression, "1,");
	HASH_FIND_STR(decompTool, "bzip2", dth);
	if (dth) strcat(compression, "2,");
	HASH_FIND_STR(decompTool, "xz", dth);
	if (dth) strcat(compression, "3,");

	if (strlen(format) > 2)
		format[strlen(format) - 1] = ';';
	else
		strcpy_s(format, "A:0;", sizeof(format));

	if (strlen(compression) > 2)
		compression[strlen(compression) - 1] = ';';
	else
		strcpy_s(compression, "B:0;", sizeof(compression));

	if (strlen(format) > 0 || strlen(compression) > 0) {
		if (espatch_ver_valid == E_UA_OK)
			delta_cap = f_asprintf("%s%sC:%d;E:%s", format, compression, memory, espatch_ver);
		else
			delta_cap = f_asprintf("%s%sC:%d", format, compression, memory);
	}

	return delta_cap;
}


__attribute__((unused)) static char* expand_tool_args(const char* args, const char* old, const char* new, const char* diff)
{
	int i = 0;
	const char* rp, * ex, * s = args;

	char* res = f_malloc(strlen(s) + 1 +
	                     ((old) ? SUBSTRCNT(s, OFA) * (strlen(old) - strlen(OFA)) : 0) +
	                     ((new) ? SUBSTRCNT(s, NFA) * (strlen(new) - strlen(NFA)) : 0) +
	                     ((diff) ? SUBSTRCNT(s, PFA) * (strlen(diff) - strlen(PFA)) : 0));

	while (*s) {
		if (((rp = old) && (strcasestr(s, ex = OFA) == s)) || ((rp = new) && (strcasestr(s, ex = NFA) == s)) || ((rp = diff) && (strcasestr(s, ex = PFA) == s))) {
#undef strcpy
			strcpy(&res[i], rp);
			i += strlen(rp);
			s += strlen(ex);
		} else {
			res[i++] = *s++;
		}
	}
	res[i] = 0;

	return res;
}

static int delta_patch(diff_info_t* diffInfo, const char* old, const char* new, const char* diff)
{
	int err     = E_UA_OK;
	char* diffp = 0;

#ifndef SHELL_COMMAND_DISABLE
	char* targs;
	char* cmd = 0;
#endif
	delta_tool_hh_t* decompdth, * patchdth = 0;

	do {
#ifndef SHELL_COMMAND_DISABLE
#define TOOL_EXEC(_t, _o, _n, _p) { \
		targs = expand_tool_args(_t->tool.args, _o, _n, _p); \
		cmd   = f_asprintf("%s %s", _t->tool.path, targs); \
		A_INFO_MSG("Executing: %s", cmd);                         \
		if (system(cmd)) err = E_UA_SYS; \
		free(targs); \
		free(cmd); \
} do {} while (0)
#endif
		if (strcmp(diffInfo->format, "none")) {
			HASH_FIND_STR(delta_stg.patch_tool, diffInfo->format, patchdth);
			BOLT_IF(!patchdth, E_UA_ERR, "Diff format %s not found", diffInfo->format);
		}

		if (strcmp(diffInfo->compression, "none") && !(patchdth && patchdth->tool.intl)) {
			HASH_FIND_STR(delta_stg.decomp_tool, diffInfo->compression, decompdth);
			BOLT_IF(!decompdth, E_UA_ERR, "Decompression %s not found", diffInfo->compression);
			diffp = f_asprintf("%s%s", diff, ".z");
#ifndef SHELL_COMMAND_DISABLE
			TOOL_EXEC(decompdth, diff, diffp, 0);
#else
			err = xzdec(diff, diffp);
#endif
			if (err) { A_INFO_MSG("Decompression failed"); break; }
		}

		if (patchdth) {
			chkdirp(new);
#ifndef SHELL_COMMAND_DISABLE
			TOOL_EXEC(patchdth, old, new, diffp ? diffp : diff);
#else
			err = espatch(old, new, diffp ? diffp : diff);
#endif
			if (err) { A_INFO_MSG("Patching failed"); break; }
		} else {
			BOLT_SUB(copy_file(diffp ? diffp : diff, new));
		}
#ifndef SHELL_COMMAND_DISABLE
#undef TOOL_EXEC
#endif
	} while (0);

	if (diffp) free(diffp);

	return err;
}


static int verify_file(const char* file, const char* sha256)
{
	int err = E_UA_OK;
	char hash[SHA256_HEX_LENGTH];

	if (!(err = calc_sha256_hex(file, hash))) {
		if (strncmp(hash, sha256, SHA256_HEX_LENGTH - 1)) {
			err = E_UA_ERR;
			A_INFO_MSG("SHA256 Hash mismatch %s : Expected: %s  Calculated: %s", file, sha256, hash);
		}

	} else {
		A_ERROR_MSG("SHA256 Hash calculation failed : %s", file);
	}

	return err;
}

void free_delta_tool_hh(delta_tool_hh_t* dth)
{
	Z_FREE(dth->tool.algo);
	Z_FREE(dth->tool.path);
	Z_FREE(dth->tool.args);
	Z_FREE(dth);

}

void free_diff_info(diff_info_t* di)
{
	Z_FREE(di->name);
	Z_FREE(di->format);
	Z_FREE(di->compression);
	Z_FREE(di->old_name);
	//Z_FREE(di->nesting.un_squash_fs.un_squash_fs);
	Z_FREE(di);

}



