/*
 * common.c
 */

#include "common.h"

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

char * f_strndup(const void *s, size_t len) {

    if (!s) { return 0; }
    char * s2 = f_malloc(len + 1);
    if (!s2) { return 0; }
    memcpy(s2, s, len);
    s2[len] = 0;
    return s2;
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

int z_strcmp(const char * s1, const char * s2) {

    if (!s1) {
        if (!s2) { return 0; }
        return -1;
    }

    if (!s2) {
        return 1;
    }

    return strcmp(s1, s2);
}




