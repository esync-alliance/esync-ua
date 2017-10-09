
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "common.h"
#include "debug.h"

static xl4bus_asn1_t * load_full(char * path);
static char * simple_password (struct xl4bus_X509v3_Identity *);


void debug_print(const char * msg) {

    DBG("%s", msg);

}

char * f_asprintf(char * fmt, ...) {

    char * ret;
    va_list ap;

    va_start(ap, fmt);
    int rc = vasprintf(&ret, fmt, ap);
    va_end(ap);

    if (rc < 0) {
        return 0;
    }

    return ret;

}

char * f_strdup(const char * s) {
    if (!s) { return 0; }
    size_t l = strlen(s) + 1;
    char * r = f_malloc(l);
    return memcpy(r, s, l);
}

void * f_malloc(size_t t) {

    void * r = malloc(t);
    if (!r) {
        DBG("Failed to malloc %ld bytes", t);
        abort();
    }

    memset(r, 0, t);

    return r;

}

void * f_realloc(void * m, size_t t) {

    void * r = realloc(m, t);
    if (!r) {
        DBG("Failed to realloc %p to %ld bytes", m, t);
        abort();
    }

    return r;

}

char * get_cert_dir(char * lbl, char * argv0) {

    char * dir = strrchr(lbl, '/');
    if (!dir) {
        dir = strrchr(argv0, '/');
        if (!dir) {
            dir = f_asprintf("./../pki/certs/%s", lbl);
        } else {
            char * aux = dir;
            *aux = 0;
            dir = f_asprintf("%s/../pki/certs/%s", argv0, lbl);
            *aux = '/';
        }
    } else {
        dir = f_strdup(lbl);
        const size_t l = strlen(dir);
        if (dir[l - 1] == '/')
            dir[l - 1] = 0;
    }

    return dir;

}

int load_xl4_x509_creds(xl4bus_identity_t * identity, char * dir) {

    char * p_key = f_asprintf("%s/private.pem", dir);
    char * cert = f_asprintf("%s/cert.pem", dir);
    char * ca = f_asprintf("%s/../ca/ca.pem", dir);

    int ret = load_simple_x509_creds(identity, p_key, cert, ca, 0);

    free(p_key);
    free(cert);
    free(ca);

    return ret;

}

int load_simple_x509_creds(xl4bus_identity_t * identity, char * p_key_path,
        char * cert_path, char * ca_path, char * password) {

    memset(identity, 0, sizeof(xl4bus_identity_t));
    identity->type = XL4BIT_X509;
    int ok = 0;

    do {

        identity->x509.private_key = load_full(p_key_path);
        if (!(identity->x509.trust = f_malloc(2 * sizeof(void*)))) {
            break;
        }
        if (!(identity->x509.chain = f_malloc(2 * sizeof(void*)))) {
            break;
        }
        if (!(identity->x509.trust[0] = load_full(ca_path))) {
            break;
        }
        if (!(identity->x509.chain[0] = load_full(cert_path))) {
            break;
        }
        if (password) {
            identity->x509.custom = f_strdup(password);
            identity->x509.password = simple_password;
        }

        ok = 1;

    } while(0);

    if (!ok) {
        release_identity(identity);
    }

    return !ok;

}

void release_identity(xl4bus_identity_t * identity) {

    if (identity->type == XL4BIT_X509) {

        if (identity->x509.trust) {
            for (xl4bus_asn1_t ** buf = identity->x509.trust; *buf; buf++) {
                free((*buf)->buf.data);
            }
            free(identity->x509.trust);
        }

        if (identity->x509.chain) {
            for (xl4bus_asn1_t ** buf = identity->x509.chain; *buf; buf++) {
                free((*buf)->buf.data);
            }
            free(identity->x509.chain);
        }

        free(identity->x509.custom);

        identity->type = XL4BIT_INVALID;

    }

}

// note that we return buffer with PEM, and a terminating
// 0, and length includes the terminating 0. This is what
// mbedtls requires.
xl4bus_asn1_t * load_full(char * path) {

    int fd = open(path, O_RDONLY);
    int ok = 0;
    if (fd < 0) {
        DBG_SYS("Failed to open %s", path);
        return 0;
    }

    xl4bus_asn1_t * buf = f_malloc(sizeof(xl4bus_asn1_t));
    buf->enc = XL4BUS_ASN1ENC_PEM;

    do {

        off_t size = lseek(fd, 0, SEEK_END);
        if (size == (off_t)-1) {
            DBG_SYS("Failed to seek %s", path);
            break;
        }
        if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
            DBG_SYS("Failed to rewind %s", path);
            break;
        }

        buf->buf.len = (size_t) (size + 1);
        void * ptr = buf->buf.data = f_malloc(buf->buf.len);
        while (size) {
            ssize_t rd = read(fd, ptr, (size_t) size);
            if (rd < 0) {
                DBG_SYS("Failed to read from %s", path);
                break;
            }
            if (!rd) {
                DBG("Premature EOF reading %d, file declared %d bytes, read %d bytes, remaining %d bytes",
                        path, buf->buf.len-1, ptr-(void*)buf->buf.data, size);
                break;
            }
            size -= rd;
            ptr += rd;
        }

        if (!size) { ok = 1; }

    } while(0);

    close(fd);

    if (!ok) {
        free(buf->buf.data);
        free(buf);
        return 0;
    }

    return buf;

}

static char * simple_password (struct xl4bus_X509v3_Identity * id) {

    return id->custom;

}

