/*
 * misc.h
 */

#ifndef _UA_MISC_H_
#define _UA_MISC_H_

#include "common.h"

#define S(s)           (s && * s)
#define SAFE_STR(s)    ((s) ? (s) : "")
#define NULL_STR(s)    (S(s) ? (s) : "null")
#define STRLWR(s)      ({char* _p,* _r=s; for (_p=_r; * _p; ++_p) * _p=tolower(* _p); _r; })
#define SUBSTRCNT(s,c) ({const char* _p=s; int _l=strlen(c); int _r=0; while ((_p=strcasestr(_p,c))) {_p+=_l; _r ++; } _r; })

#define JOIN(a,b ...)  (join_path(a, ## b, NULL))

static inline char* join_path(const char* path, ... )
{
	if (!path) { return 0; }
	va_list ap;
	va_start(ap, path);
	const char* aux;
	char* ret = f_strdup(path);
	int l     = 0;
	while ((aux = va_arg(ap, const char* ))) {
		ret = f_realloc(ret, strlen(ret) + strlen(aux) + 2);
		if (*ret && ret[(l = strlen(ret))-1] != '/')
			*(ret + (l++)) = '/';
		strcpy(ret + l, aux);
	}
	va_end(ap);
	return ret;
}

static inline const char* chop_path(const char* path)
{
	const char* aux = strrchr(path, '/');

	if (aux) { return aux + 1; }
	return path;
}


#define SHA256_HEX_LENGTH SHA256_DIGEST_LENGTH * 2 + 1
#define SHA256_B64_LENGTH 44 + 1

extern size_t ua_rw_buff_size;

uint64_t currentms();
int unzip(const char* archive, const char* path);
int zip(const char* archive, const char* path);
int zip_find_file(const char* archive, const char* path);
int copy_file(const char* from, const char* to);
int calc_sha256(const char* path, unsigned char obuff[SHA256_DIGEST_LENGTH]);
int calc_sha256_hex(const char* path, char obuff[SHA256_HEX_LENGTH]);
int calc_sha256_x(const char* archive, char obuff[SHA256_B64_LENGTH]);
int sha256xcmp(const char* archive, char b64[SHA256_B64_LENGTH]);
int is_cmd_runnable(const char* cmd);
int remove_subdirs_except(char* parent_dir, char* subdir_to_keep);
int mkdirp(const char* path, int umask);
int newdirp(const char* path, int umask);
int rmdirp(const char* path);
int chkdirp(const char* path);
int run_cmd(char* cmd, char* argv[]);
#endif /* _UA_MISC_H_ */
