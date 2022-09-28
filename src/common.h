/*
 * common.h
 */

#ifndef UA_COMMON_H_
#define UA_COMMON_H_

#include <stddef.h>
#include <string.h>
#include "porting.h"

#define XL4_UNUSED(x) (void)x;
#define BASE_TEN_CONVERSION 10

/* Force error if string.h functions are used */
#define strcpy(a,b)     _Pragma("GCC error \"strcpy not safe use strcpy_s\"")
#define strncpy(a,b,c)  _Pragma("GCC error \"strncpy not safe use strcpy_s\"")

/**
 * strcpy_s - Copy a C-string into a sized buffer
 * @dest:  Where to copy the string to
 * @src:   Where to copy the string from
 * @count: Size of destination buffer
 *
 * Copy the string, or as much of it as fits, into the dest buffer.  The
 * behavior is undefined if the string buffers overlap.  The destination
 * buffer is always NUL terminated, unless it's zero-sized.
 *
 * Preferred to strncpy() since it always returns a valid string, and
 * doesn't unnecessarily force the tail of the destination buffer to be
 * zeroed.
 *
 * Returns:
 * * The number of characters copied (not including the trailing %NUL)
 */
size_t strcpy_s(char *dst, const char *src, size_t size);

char* f_asprintf(const char* fmt, ...);
char* f_strdup(const char*);
char* f_strndup(const void* s, size_t len);
void* f_malloc(size_t);
void* f_realloc(void*, size_t);
void f_free(void* p);
char* f_dirname(const char* s);
char* f_basename(const char* s);

#define Z_FREE(p) {f_free(p); p=NULL; } do {} while (0)
#define Z_STRDUP(d, s) {if(d){ f_free(d); d=NULL; } d = f_strdup(s); }do {} while (0)

#ifdef SUPPORT_UA_DOWNLOAD
int f_remove_dir(const char* dir);
int f_copy(const char* source, const char* dest);
int f_size(const char* file, int* size);
#endif

#endif /* UA_COMMON_H_ */
