/*
 * misc.c
 */

#include "misc.h"
#include "delta.h"
#include "handler.h"

extern ua_internal_t ua_intl;

static sha_le_t *sl_head = NULL;
static int temp_dirname_len = 0;

static int zip_archive_add_file(struct zip * za, const char * path, const char * base);
static int zip_archive_add_dir(struct zip *za, const char *path, const char *base);
static char * get_zip_error(int ze);

#define STACK_BUF_SIZE               4096


uint64_t currentms() {

    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp);

    return ((uint64_t)tp.tv_sec) * 1000ULL +
        tp.tv_nsec / 1000000ULL;

}


int mkdirp(const char * path, int umask) {

    char * dpath = f_strdup(path);
    char * aux = dpath;
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


int rmdirp(const char * path) {

    int err = E_UA_OK;
    DIR *dir;
    struct dirent *entry;
    char * filepath;

    DBG("removing dir %s", path);

    do {

        BOLT_SYS(!(dir = opendir(path)), "failed to open directory %s", path);

        while ((entry = readdir(dir)) != NULL) {

            filepath = JOIN(path, entry->d_name);

            if (entry->d_type != DT_DIR) {
                if (remove(filepath) < 0) {
                    err = E_UA_SYS;
                    DBG_SYS("error removing file: %s", filepath);
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


int chkdirp(const char * path) {

    int rc;
    struct stat statb;
    char * dir = f_dirname(path);

    rc = stat(dir, &statb);

    if (rc || !S_ISDIR(statb.st_mode)) {
        rc = mkdirp(dir, 0755);
    }

    free(dir);
    return rc;
}


int newdirp(const char * path, int umask) {

    int rc;
    struct stat statb;

    rc = stat(path, &statb);

    if (!rc) {
        rmdirp(path);
    }

    rc = mkdirp(path, umask);

    return rc;
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


int unzip(const char * archive, const char * path) {

    int i, len, fd, zerr, err = E_UA_OK;
    char buf[STACK_BUF_SIZE];
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
                BOLT_SYS(chkdirp(fpath), "failed to prepare directory for %s", fpath);
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

    } while (0);

    if (za && zip_close(za)) { err = E_UA_ERR;  DBG("failed to close zip archive %s : %s", archive, zip_strerror(za)); }

    if (err) {
        if(aux) free(aux);
        if(fpath) free(fpath);
    }

    return err;
}


int zip(const char * archive, const char * path) {

    int zerr, err = E_UA_OK;
    char * aux = 0;
    struct stat path_stat;
    struct zip * za = 0;

    DBG("ziping %s to %s", path, archive);

/**** ESYNC-3166 - todo: make zip as fast as zip tool****
    do {

        BOLT_SYS(stat(path, &path_stat), "failed to get path status: %s", path);
        BOLT_IF(!S_ISREG(path_stat.st_mode) && !S_ISDIR(path_stat.st_mode), E_UA_ERR, "path is not file or directory");
        BOLT_SYS(chkdirp(archive), "failed to prepare directory for %s", archive);
        BOLT_IF(!(za = zip_open(archive, ZIP_CREATE | ZIP_TRUNCATE, &zerr)), E_UA_ERR,
                            "failed to create zip file %s : %s", archive, aux = get_zip_error(zerr));

        err = S_ISREG(path_stat.st_mode) ? zip_archive_add_file(za, path, 0) : zip_archive_add_dir(za, path, 0);

    } while (0);

    if (za && zip_close(za)) { err = E_UA_ERR;  DBG("failed to close zip archive %s : %s", archive, zip_strerror(za)); }

    if (err) {
        if (aux) free(aux);
        if (za) zip_discard(za);
    }
*/

    char * cmd = 0;
    do {
        BOLT_SYS(chkdirp(archive), "failed to prepare directory for %s", archive);
        cmd = f_asprintf("cd %s; rm -f %s; zip -r %s *", path, archive, archive);
        DBG("Executing: %s", cmd);
        BOLT_SYS(system(cmd), "failed to zip files");
    } while (0);
    if (cmd) free(cmd);

    return err;
}


static int zip_archive_add_file(struct zip * za, const char * path, const char * base) {

    int err = E_UA_OK;
    zip_source_t *s;
    char * file;

    DBG("adding file %s to zip", path);

    do {

        char * bname = f_basename(path);
        file = JOIN(SAFE_STR(base), bname);
        free(bname);

        BOLT_IF(!(s = zip_source_file(za, path, 0, 0)), E_UA_ERR, "failed to source file %s : %s", path, zip_strerror(za));

        if (zip_file_add(za, file, s, ZIP_FL_ENC_UTF_8) < 0) {
            zip_source_free(s);
            BOLT_SAY(E_UA_ERR, "error adding file: %s", zip_strerror(za));
        }

    } while (0);

    free(file);
    return err;
}


static int zip_archive_add_dir(struct zip * za, const char * path, const char * base) {

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


int zip_find_file(const char * archive, const char * path) {

    int zerr, err = E_UA_OK;
    char * aux = 0;
    struct zip *za;

    do {

        BOLT_IF(!(za = zip_open(archive, ZIP_RDONLY, &zerr)), E_UA_ERR,
                        "failed to open file as zip %s : %s", archive, aux = get_zip_error(zerr));
        if (zip_name_locate(za, path, 0) < 0) {
            DBG("file: %s not found in zip: %s", path, archive);
            err = E_UA_ERR;
        } else {
            DBG("file: %s found in zip: %s", path, archive);
            err = E_UA_OK;
        }

    } while (0);

    if (za && zip_close(za)) { err = E_UA_ERR;  DBG("failed to close zip archive %s : %s", archive, zip_strerror(za)); }

    if (err) {
        if(aux) free(aux);
    }

    return err;

}


int copy_file(const char * from, const char * to) {

    int err = E_UA_OK;
    FILE *in, *out;
    char buf[STACK_BUF_SIZE];
    size_t nread;

    DBG("copying file from %s to %s", from, to);

/**** ESYNC-2789 - todo: make copy as fast as cp tool****
    do {

        BOLT_SYS(!(in = fopen(from, "r")), "opening file: %s", from);

        BOLT_SYS(chkdirp(to), "failed to prepare directory for %s", to);
        BOLT_SYS(!(out = fopen(to, "w")), "creating file: %s", to);

        do {
            BOLT_SYS(((nread = fread(buf, sizeof(char), sizeof(buf), in)) == 0) && ferror(in), "reading from file: %s", from);
            BOLT_SYS(nread && (fwrite(buf, sizeof(char), nread, out) != nread), "writing to file: %s", to);
        } while (nread);

    } while (0);

    if (in && fclose(in)) DBG_SYS("closing file: %s", from);
    if (out && fclose(out)) DBG_SYS("closing file: %s", to);
*/

    char * cmd = 0;
    do {
        BOLT_SYS(chkdirp(to), "failed to prepare directory for %s", to);
        cmd = f_asprintf("cp %s %s", from, to);
        DBG("Executing: %s", cmd);
        BOLT_SYS(system(cmd), "failed to copy file");
    } while (0);
    if (cmd) free(cmd);

    return err;
}


int calc_sha256(const char * path, unsigned char obuff[SHA256_DIGEST_LENGTH], bool fIncludeFilename) {

    int i, err = E_UA_OK;
    FILE *file;
    SHA256_CTX sha256;
    char buf[STACK_BUF_SIZE];
    size_t nread;

    do {

        BOLT_SYS(!(file = fopen(path, "rb")), "opening file: %s", path);

        SHA256_Init(&sha256);

        if(fIncludeFilename){
            char * tmp_path = path + temp_dirname_len + 1;
            SHA256_Update(&sha256, tmp_path, strlen(tmp_path));
        }

        while ((nread = fread(buf, sizeof(char), sizeof(buf), file))) {
            SHA256_Update(&sha256, buf, nread);
        }

        SHA256_Final(obuff, &sha256);

        BOLT_SYS(fclose(file), "closing file: %s", path);

    } while (0);

    return err;
}

int calc_sha256_hex(const char * path, char obuff[SHA256_HEX_LENGTH], bool fIncludeFilename) {

    int i, err = E_UA_OK;
    unsigned char hash[SHA256_DIGEST_LENGTH];

    if (!(err = calc_sha256(path, hash, fIncludeFilename))) {

        for(i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            sprintf(obuff + (i * 2), "%02x", (unsigned char)hash[i]);
        }

        obuff[SHA256_HEX_LENGTH - 1] = 0;

    }

    return err;
}

int calc_sha256_b64(const char * path, char obuff[SHA256_B64_LENGTH], bool sha_of_sha) {

    int err = E_UA_OK;
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    unsigned char hash[SHA256_DIGEST_LENGTH];

    if(sha_of_sha)
        err = calc_sha256_sh256(path, hash); 
    else    
        err = calc_sha256(path, hash, false);
 
    if(!err){
        b64 = BIO_new(BIO_f_base64());
        bmem = BIO_new(BIO_s_mem());
        b64 = BIO_push(b64, bmem);

        BIO_write(b64, hash, SHA256_DIGEST_LENGTH);
        BIO_flush(b64);
        BIO_get_mem_ptr(b64, &bptr);

        memcpy(obuff, bptr->data, SHA256_B64_LENGTH - 1);
        obuff[SHA256_B64_LENGTH - 1] = 0;

        BIO_free_all(b64);
    }

    return err;
}

static int sha256cmp(sha_le_t *a, sha_le_t *b) {

    return memcmp(a->sha256, b->sha256, SHA256_DIGEST_LENGTH);

}

static bool is_file_for_sha(char * path){

    if(path){
        return (strcmp(path, MANIFEST_DIFF) &&  
                strcmp(path, MANIFEST) && 
                strncmp(path, XL4_X_PREFIX, strlen(XL4_X_PREFIX)) &&
                strncmp(path, XL4_SIGNATURE_PREFIX, strlen(XL4_SIGNATURE_PREFIX)));
    }else
        return false;
}

static int process_dir_entry (const char *fpath, const struct stat *sb,
                        int typeflag, struct FTW *ftwbuf)
{
    int err = E_UA_OK;

    if(fpath && typeflag == FTW_F && is_file_for_sha(fpath+temp_dirname_len+1)){

        sha_le_t *tmp = (sha_le_t *)malloc(sizeof(sha_le_t));
        if((err = calc_sha256(fpath, tmp->sha256, true)) == E_UA_OK){
            DL_APPEND(sl_head, tmp);           
        }else{
            DBG("calc_sha256_hex failed err is %d", err);
            free(tmp);
        }
    }

    return err;
}

int calc_sha256_sh256(const char * path, unsigned char obuff[SHA256_DIGEST_LENGTH]) {

    int err = E_UA_OK;
    sha_le_t *elt, *tmp;
    SHA256_CTX sha256;
    char buf[STACK_BUF_SIZE];
    int i = 0;
    char *temp_dir = 0;
    char *template = JOIN(ua_intl.cache_dir, "sha.XXXXXX");

    do{

        BOLT_SYS(!(temp_dir = mkdtemp(template)), "mkdtemp failed");
        temp_dirname_len = strlen(temp_dir);

        BOLT_SUB(unzip(path, temp_dir));

        BOLT_SYS(!SHA256_Init(&sha256), "SHA256_Init");
    
        BOLT_SYS(nftw(temp_dir, process_dir_entry, 20, 0), "nftw failed");

        DL_SORT(sl_head, sha256cmp);

        DL_FOREACH(sl_head,elt) {
            SHA256_Update(&sha256, elt->sha256, SHA256_DIGEST_LENGTH);
        }

        BOLT_SYS(!SHA256_Final(obuff, &sha256), "SHA256_Final");

        rmdirp(temp_dir);

        if(ua_debug){
            printf("*************************   sha of sha  ************************\n");
            for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) 
            {
                printf("%02x", obuff[i]);
            }
            printf("\n****************************************************************\n");        
        }    

    }while(0);

    DL_FOREACH_SAFE(sl_head,elt,tmp) {
        DL_DELETE(sl_head,elt);
        free(elt);
    }

    f_free(template);
    
    return err;
}

int is_cmd_runnable(const char * cmd) {

    int err = E_UA_OK;
    char * path = 0;

    do {

        if(strchr(cmd, '/')) {
            err = access(cmd, X_OK);
            break;
        }

        BOLT_IF(!(path = f_strdup(getenv("PATH"))), E_UA_ERR, "PATH env empty");

        char * pch = strtok(path, ":");
        while (pch != NULL) {
            char * p = JOIN(pch, cmd);
            if((err = access(p, X_OK)) == 0) {
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
