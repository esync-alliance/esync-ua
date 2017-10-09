
#ifndef _XL4BROKER_COMMON_H_
#define _XL4BROKER_COMMON_H_

#include <stdint.h>
#include <sys/types.h>
#include <libxl4bus/types.h>

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
