/*
 * misc.h
 */

#ifndef UA_MISC_H_
#define UA_MISC_H_

#if !defined(DISABLE_JSONC_RENAME)
#include "json-c-rename.h"
#endif
#include "common.h"
#include <openssl/sha.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#define S(s)        (s && * s)
#define SAFE_STR(s) ((s) ? (s) : "")
#define NULL_STR(s) (S(s) ? (s) : "null")

#define STRLWR(s)   __strlwr(s)
static inline char* __strlwr(char* s)
{
	char* _p,* _r=s;

	for (_p=_r; *_p; ++_p) *_p=tolower(*_p);
	return _r;
}

#define SUBSTRCNT(s,c) __substrcnt(s,c)
static inline int __substrcnt(const char* s, const char* c)
{
	const char* _p=s;
	int _l        =strlen(c);
	int _r        =0;

	while ((_p=strcasestr(_p,c))) {_p+=_l; _r++; }
	return _r;
}

#define JOIN(a,b ...) (join_path(a, ## b, NULL))

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
#define REPLY_ID_STR_LEN  24

extern size_t ua_rw_buff_size;

uint64_t currentms(void);
int unzip(const char* archive, const char* path);
int zip(const char* archive, const char* path);
int libzip_find_file(const char* archive, const char* path);
int copy_file(const char* from, const char* to);
int make_file_hard_link(const char* from, const char* to);
int calc_sha256(const char* path, unsigned char obuff[SHA256_DIGEST_LENGTH]);
int calc_sha256_hex(const char* path, char obuff[SHA256_HEX_LENGTH]);
int calc_sha256_x(const char* archive, char obuff[SHA256_B64_LENGTH]);
int calculate_sha256_b64(const char* file, char b64buff[SHA256_B64_LENGTH]);
int base64_encode(unsigned char hexdigest[SHA256_DIGEST_LENGTH], char b64buff[SHA256_B64_LENGTH]);
int verify_file_hash_b64(const char* file, const char* sha256_b64);
int sha256xcmp(const char* archive, char b64[SHA256_B64_LENGTH]);
int is_cmd_runnable(const char* cmd);
int remove_subdirs_except(char* parent_dir, char* subdir_to_keep);
int mkdirp(const char* path, int umask);
int newdirp(const char* path, int umask);
int rmdirp(const char* path);
int chkdirp(const char* path);
int run_cmd(char* cmd, char* argv[]);
char* randstring(int length);
#endif /* UA_MISC_H_ */
