/*
 * common.c
 */

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdarg.h>
#include <string.h>
#include "debug.h"

char* f_asprintf(char* fmt, ...)
{
	char* ret;
	va_list ap;

	va_start(ap, fmt);
	int rc = vasprintf(&ret, fmt, ap);
	va_end(ap);

	if (rc < 0) {
		return 0;
	}

	return ret;
}


char* f_strdup(const char* s)
{
	if (!s) { return 0; }
	size_t l = strlen(s) + 1;
	char* r  = f_malloc(l);
	return memcpy(r, s, l);
}


char* f_strndup(const void* s, size_t len)
{
	if (!s) { return 0; }
	char* s2 = f_malloc(len + 1);
	if (!s2) { return 0; }
	memcpy(s2, s, len);
	s2[len] = 0;
	return s2;
}


void* f_malloc(size_t t)
{
	void* r = malloc(t);

	if (!r) {
		DBG("Failed to malloc %ld bytes", t);
		abort();
	}

	memset(r, 0, t);

	return r;
}


void* f_realloc(void* m, size_t t)
{
	void* r = realloc(m, t);

	if (!r) {
		DBG("Failed to realloc %p to %ld bytes", m, t);
		abort();
	}

	return r;
}

void f_free(void* p)
{
	if (p) {
		free(p);
	}
	p = NULL;
}


char* f_dirname(const char* s)
{
	if (!s) { return 0; }
	char* aux = f_strdup(s);
	char* s2  = f_strdup(dirname(aux));
	free(aux);
	return s2;
}


char* f_basename(const char* s)
{
	if (!s) { return 0; }
	char* aux = f_strdup(s);
	char* s2  = f_strdup(basename(aux));
	free(aux);
	return s2;
}

