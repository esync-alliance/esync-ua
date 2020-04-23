/*
 * common.h
 */

#ifndef UA_COMMON_H_
#define UA_COMMON_H_

#include <stddef.h>
#include "porting.h"

char* f_asprintf(char* fmt, ...);
char* f_strdup(const char*);
char* f_strndup(const void* s, size_t len);
void* f_malloc(size_t);
void* f_realloc(void*, size_t);
void f_free(void* p);
char* f_dirname(const char* s);
char* f_basename(const char* s);

#define Z_FREE(p) {f_free(p); p=NULL; } do {} while (0)

#ifdef SUPPORT_UA_DOWNLOAD
int f_remove_dir(const char* dir);
int f_copy(const char* source, const char* dest);
int f_size(const char* file, int* size);
#endif

#endif /* UA_COMMON_H_ */
