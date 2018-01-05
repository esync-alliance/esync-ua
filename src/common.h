/*
 * common.h
 */

#ifndef _UA_COMMON_H_
#define _UA_COMMON_H_

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <openssl/sha.h>
#include "json-c-rename.h"
#include <json.h>
#include <zip.h>
#include "build_config.h"
#include <xl4ua.h>
#include "uthash.h"
#include "utlist.h"
#include "debug.h"


char * f_asprintf(char * fmt, ...);
char * f_strdup(const char *);
char * f_strndup(const void *s, size_t len);
void * f_malloc(size_t);
void * f_realloc(void *, size_t);
int z_strcmp(const char * s1, const char * s2);
char * f_dirname(const char * s);
char * f_basename(const char * s);

#include "misc.h"

#endif /* _UA_COMMON_H_ */
