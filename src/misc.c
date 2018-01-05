/*
 * misc.c
 */

#include "misc.h"

static int zip_archive_add_file(struct zip *za, char *path, char *base);
static int zip_archive_add_dir(struct zip *za, char *path, char *base);
static char * get_zip_error(int ze);

#define BUF_SIZE               4096


int mkdirp(char* path, int umask) {

    char * aux = path;
    int rc;
    struct stat statb;

    if (*aux != '/') {
        errno = EINVAL;
        return 1;
    }

    while (*(aux+1) == '/') { aux++; }

    if (!*(aux+1)) { return 0; }

    while (1) {

        *aux = '/';
        aux++;
        if (!*aux) { break; }
        aux = strchr(aux, '/');

        if (aux) {
            while (*(aux+1) == '/') { aux++; }
            *aux = 0;
        }

        rc = stat(path, &statb);

        if (rc || !S_ISDIR(statb.st_mode)) {
            rc = mkdir(path, umask);
            if (rc) { return rc; }
        }

        if (!aux) { break; }

    }

    return 0;
}


int rmdirp(const char* path) {

    int err = E_UA_OK;
    DIR *dir;
    struct dirent *entry;
    char * filepath;

    DBG("removing dir %s", path);

    do {

        BOLT_IF(!(dir = opendir(path)), E_UA_ERR, "failed to open directory %s", path);

        while ((entry = readdir(dir)) != NULL) {

            filepath = JOIN(path, entry->d_name);

            if (entry->d_type != DT_DIR) {
                if (remove(filepath) < 0) {
                    err = E_UA_SYS;
                    DBG_SYS("error removing file: %s", filepath);
                }
            } else if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                rmdirp(filepath);
            }

            free(filepath);
            if (err) break;
        }
        closedir(dir);
        BOLT_SYS(remove(path), "error removing directory: %s", path);
    } while (0);

    return err;

}


static char * get_zip_error(int ze) {

    int se = errno;
    char buf[256];
    char * buf2 = 0;
    int x = zip_error_to_str(buf, 256, ze, se);
    if (x >= 255) {
        buf2 = f_malloc(x+1);
        zip_error_to_str(buf2, x+1, ze, se);
    }

    if (buf2) { return buf2; }
    return f_strdup(buf);

}


int unzip(char * archive, char * path) {

    int i, len, fd, zerr, err = E_UA_OK;
    char buf[BUF_SIZE];
    long sum;
    char * aux = 0;
    char * fpath = 0;
    struct zip *za;
    struct zip_file *zf;
    struct zip_stat sb;

    DBG("unziping archive %s to %s", archive, path);

    do {

        BOLT_IF(!(za = zip_open(archive, ZIP_RDONLY, &zerr)), E_UA_ERR,
                "failed to open file as ZIP %s : %s", archive, aux = get_zip_error(zerr));

        for (i = 0; i < zip_get_num_entries(za, 0); i++) {

            BOLT_IF(zip_stat_index(za, i, 0, &sb), E_UA_ERR, "failed reading stat at index %d: %s", i, zip_strerror(za));

            fpath = JOIN(path, sb.name);
            len = strlen(sb.name);
            if (sb.name[len - 1] == '/') {
                BOLT_SYS(mkdirp(fpath, 0755) && (errno != EEXIST), "failed to make directory %s", fpath);
            } else {
                BOLT_IF(!(zf = zip_fopen_index(za, i, 0)), E_UA_ERR, "failed to open/find %s: %s", sb.name, zip_strerror(za));
                BOLT_SYS((fd = open(fpath, O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0, "failed to open/create %s", fpath);

                sum = 0;
                while (sum != sb.size) {
                    BOLT_IF((len = zip_fread(zf, buf, sizeof(buf))) < 0,
                            E_UA_ERR, "error reading %s : %s", sb.name, zip_file_strerror(zf));
                    write(fd, buf, len);
                    sum += len;
                }

                close(fd);
                zip_fclose(zf);
            }
            free(fpath);
            fpath = 0;
        }
        BOLT_IF(zip_close(za), E_UA_ERR, "failed to close zip archive %s", archive);

    } while (0);

    if (err) {
        if(aux) free(aux);
        if(fpath) free(fpath);
    }

    return err;
}


int zip(char * archive, char * path) {

    int zerr, err = E_UA_OK;
    char * aux = 0;
    struct stat path_stat;
    struct zip * za = 0;
    char * dname = f_dirname(archive);

    DBG("ziping %s to %s", path, archive);

    do {

        BOLT_SYS(stat(path, &path_stat), "failed to get path status: %s", path);
        BOLT_IF(!S_ISREG(path_stat.st_mode) && !S_ISDIR(path_stat.st_mode), E_UA_ERR, "path is not file or directory");
        BOLT_SYS(mkdirp(dname, 0755) && (errno != EEXIST), "failed to make directory %s", dname);
        BOLT_IF(!(za = zip_open(archive, ZIP_CREATE | ZIP_TRUNCATE, &zerr)), E_UA_ERR,
                            "failed to create zip file %s : %s", archive, aux = get_zip_error(zerr));

        err = S_ISREG(path_stat.st_mode) ? zip_archive_add_file(za, path, 0) : zip_archive_add_dir(za, path, 0);

        BOLT_IF(zip_close(za), E_UA_ERR, "failed to close zip archive %s", archive);
    } while (0);

    if (err) {
        if (aux) free(aux);
        if (za) zip_discard(za);
    }

    free(dname);

    return err;
}


static int zip_archive_add_file(struct zip * za, char * path, char * base) {

    int err = E_UA_OK;
    zip_source_t *s;
    char * file;

    DBG("adding file %s to zip", path);

    do {

        char * bname = f_basename(path);
        file = JOIN(SAFE_STR(base), bname);
        free(bname);

        BOLT_IF(!(s = zip_source_file(za, path, 0, 0)), E_UA_ERR, "failed to source file %s: %s", path, zip_strerror(za));

        if (zip_file_add(za, file, s, ZIP_FL_ENC_UTF_8) < 0) {
            zip_source_free(s);
            BOLT_SAY(E_UA_ERR, "error adding file: %s", zip_strerror(za));
        }

    } while (0);

    free(file);
    return err;
}


static int zip_archive_add_dir(struct zip *za, char *path, char *base) {

    int err = E_UA_OK;
    DIR *dir;
    struct dirent *entry;
    char * filepath;
    char * basepath;

    DBG("adding directory %s to zip", path);

    do {
        BOLT_IF (!(dir = opendir(path)), E_UA_ERR, "failed to open directory %s", path);

        while ((entry = readdir(dir)) != NULL) {

            filepath = JOIN(path, entry->d_name);
            basepath = JOIN(SAFE_STR(base), entry->d_name);

            if (entry->d_type != DT_DIR) {

                err = zip_archive_add_file(za, filepath, base);

            } else if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {

                if (zip_add_dir(za, basepath) < 0) {
                    DBG("error adding directory: %s", basepath);
                    err = E_UA_ERR;
                } else {
                    zip_archive_add_dir(za, filepath, basepath);
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


int zip_find_file(char * archive, char * path) {

    int zerr, err = E_UA_OK;
    char * aux = 0;
    struct zip *za;

    do {

        BOLT_IF(!(za = zip_open(archive, ZIP_RDONLY, &zerr)), E_UA_ERR,
                        "failed to open file as ZIP %s : %s", archive, aux = get_zip_error(zerr));
        if (zip_name_locate(za, path, 0) < 0) {
            DBG("file: %s not found in zip: %s", path, archive);
            err = E_UA_ERR;
        } else {
            DBG("file: %s found in zip: %s", path, archive);
            err = E_UA_OK;
        }

    } while (0);

    if (err) {
        if(aux) free(aux);
    }

    return err;

}


int copy_file(char *from, char *to) {

    int err = E_UA_OK;
    FILE *in, *out;
    char buf[BUF_SIZE];
    size_t nread;
    char * dname = f_dirname(to);

    DBG("copying file from %s to %s", from, to);

    do {

        BOLT_SYS(!(in = fopen(from, "r")), "opening file: %s", from);

        BOLT_SYS(mkdirp(dname, 0755) && (errno != EEXIST), "failed to make directory %s", dname);
        BOLT_SYS(!(out = fopen(to, "w")), "creating file: %s", to);

        do {
            BOLT_SYS(((nread = fread(buf, sizeof(char), sizeof(buf), in)) == 0) && ferror(in), "reading from file: %s", from);
            BOLT_SYS(nread && (fwrite(buf, sizeof(char), nread, out) != nread), "writing to file: %s", to);
        } while (nread);

    } while (0);

    if (in && fclose(in)) DBG_SYS("closing file: %s", from);
    if (out && fclose(out)) DBG_SYS("closing file: %s", to);
    free(dname);

    return err;
}


int calc_sha256(const char * path, char outputBuffer[65]) {

    int i, err = E_UA_OK;
    FILE *file;
    SHA256_CTX sha256;
    char buf[BUF_SIZE];
    size_t nread;
    unsigned char hash[SHA256_DIGEST_LENGTH];

    do {

        BOLT_SYS(!(file = fopen(path, "rb")), "opening file: %s", path);

        SHA256_Init(&sha256);

        while ((nread = fread(buf, sizeof(char), sizeof(buf), file))) {
            SHA256_Update(&sha256, buf, nread);
        }

        SHA256_Final(hash, &sha256);

        for(i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            sprintf(outputBuffer + (i * 2), "%02x", (unsigned char)hash[i]);
        }

        outputBuffer[64] = 0;

    } while (0);

    if (file && fclose(file)) DBG_SYS("closing file: %s", path);

    return err;
}


int is_cmd_runnable(const char *cmd) {

    int err = E_UA_OK;
    char * path = 0;

    do {

        if(strchr(cmd, '/')) {
            err = access(cmd, X_OK);
            break;
        }

        BOLT_IF(!(path = f_strdup(getenv("PATH"))), E_UA_ERR, "PATH env empty");

        char* pch = strtok (path, ":");
        while (pch != NULL) {
            char * p = JOIN(pch, cmd);
            if((err = access(p, X_OK)) == 0) {
                free(p);
                break;
            }
            free(p);
            pch = strtok (NULL, ":");
       }
    } while (0);

    if (path) free(path);
    return err;
}