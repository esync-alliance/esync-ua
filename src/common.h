/*
 * common.h
 */

#ifndef UA_COMMON_H_
#define UA_COMMON_H_

#define _GNU_SOURCE

#include <stddef.h>

char* f_asprintf(char* fmt, ...);
char* f_strdup(const char*);
char* f_strndup(const void* s, size_t len);
void* f_malloc(size_t);
void* f_realloc(void*, size_t);
void f_free(void* p);
char* f_dirname(const char* s);
char* f_basename(const char* s);

#endif /* UA_COMMON_H_ */
