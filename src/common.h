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
#include <libxl4bus/types.h>
#include "updateagent.h"
#include "debug.h"


void debug_print(const char * msg);
char * f_asprintf(char * fmt, ...);
char * f_strdup(const char *);
void * f_malloc(size_t);
void * f_realloc(void *, size_t);

char * get_cert_dir(char * lbl, char * argv0);

int load_xl4_x509_creds(xl4bus_identity_t * identity, char * dir);

int load_simple_x509_creds(xl4bus_identity_t * identity, char * p_key_path,
        char * cert_path, char * ca_path, char * password);

void release_identity(xl4bus_identity_t *);

#endif
