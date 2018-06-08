/*
 * misc.h
 */

#ifndef _UA_MISC_H_
#define _UA_MISC_H_

#include "common.h"

#define S(s) (s && *s)
#define SAFE_STR(s) ((s)?(s):"")
#define NULL_STR(s) (S(s)?(s):"null")
#define JOIN(a,b...) (join_path(a, ##b, NULL))

static inline const char * chop_path(const char * path) {
    const char * aux = strrchr(path, '/');
    if (aux) { return aux + 1; }
    return path;
}

static inline char * join_path(const char * path, ... ) {
    if (!path) { return 0; }
    va_list ap;
    va_start(ap, path);
    const char * aux;
    char * ret = f_strdup(path);
    int l = 0;
    while ((aux = va_arg(ap, const char * ))) {
        ret = f_realloc(ret, strlen(ret) + strlen(aux) + 2);
        if (*ret && ret[(l = strlen(ret))-1] != '/')
            *(ret + (l++)) = '/';
        strcpy(ret + l, aux);
    }
    va_end(ap);
    return ret;
}


#define SHA256_STRING_LENGTH    SHA256_DIGEST_LENGTH * 2 + 1
#define SHA256_B64_LENGTH       44 + 1


uint64_t currentms();
int unzip(char * archive, char * path);
int zip(char * archive, char * path);
int zip_find_file(char * archive, char * path);
int copy_file(char *from, char *to);
int calc_sha256(const char * path, unsigned char obuff[SHA256_DIGEST_LENGTH]);
int calc_sha256_hex(const char * path, char obuff[SHA256_STRING_LENGTH]);
int calc_sha256_b64(const char * path, char obuff[SHA256_B64_LENGTH]);
int is_cmd_runnable(const char *cmd);
int mkdirp(char* path, int umask);
int rmdirp(const char* path);
int chkdirp(const char * path);

#endif /* _UA_MISC_H_ */
