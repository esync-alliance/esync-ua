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
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include "json-c-rename.h"
#include <json.h>
#include <zip.h>
#include "build_config.h"
#include <xl4ua.h>
#include "debug.h"


char * f_asprintf(char * fmt, ...);
char * f_strdup(const char *);
void * f_malloc(size_t);
void * f_realloc(void *, size_t);

#endif /* _UA_COMMON_H_ */
