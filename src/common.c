/*
 * common.c
 */

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdarg.h>
#include <string.h>
#ifdef SUPPORT_UA_DOWNLOAD
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#endif
#include "debug.h"

size_t strcpy_s(char *dst, const char *src, size_t size)
{
    if (size == 0)
        return 0;
    if (strlen(src) >= size) {
        memcpy(dst, src, size-1);
        dst[size-1] = 0;
        return size-1;
    }
#undef strncpy
    strncpy(dst, src, size-1);
    dst[size-1] = 0;
    return strlen(dst);
}

char* f_asprintf(const char* fmt, ...)
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
		A_ERROR_MSG("Failed to malloc %ld bytes", t);
		abort();
	}

	memset(r, 0, t);

	return r;
}


void* f_realloc(void* m, size_t t)
{
	void* r = realloc(m, t);

	if (!r) {
		A_ERROR_MSG("Failed to realloc %p to %ld bytes", m, t);
		abort();
	}

	return r;
}

void f_free(void* p)
{
	if (p) {
		free(p);
	}
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

#ifdef SUPPORT_UA_DOWNLOAD
int f_is_dir(char* filename)
{
	struct stat buf;
	int ret = stat(filename,&buf);

	if (0 == ret) {
		if (S_ISDIR(buf.st_mode)) {
			return 0;
		} else {
			return 1;
		}
	}

	return -1;
}

int f_remove_dir(const char* dirname)
{
	char chBuf[256];
	DIR* dir = NULL;
	struct dirent* ptr;
	int ret = 0;

	dir = opendir(dirname);

	if (NULL == dir) {
		return -1;
	}

	while ((ptr = readdir(dir)) != NULL) {
		ret = strcmp(ptr->d_name, ".");
		if (0 == ret) {
			continue;
		}

		ret = strcmp(ptr->d_name, "..");
		if (0 == ret) {
			continue;
		}

		snprintf(chBuf, 256, "%s/%s", dirname, ptr->d_name);
		ret = f_is_dir(chBuf);
		if (0 == ret) {
			ret = f_remove_dir(chBuf);
			if (0 != ret) {
				continue;
			}
		} else if (1 == ret) {
			ret = remove(chBuf);
			if (0 != ret) {
				continue;
			}
		}
	}

	closedir(dir);
	ret = remove(dirname);

	if (0 != ret) {
		return -1;
	}

	return 0;
}

int f_copy(const char* source, const char* dest)
{
	FILE* s_fp;
	FILE* d_fp;
	char buf[1024];
	int len;
	int ret = 0;

	if (!source || !dest) {
		return -1;
	}

	s_fp = fopen(source, "r");
	if (!s_fp) {
		return -1;
	}

	d_fp = fopen(dest, "w+");
	if (!d_fp) {
		fclose(s_fp);
		return -1;
	}

	while ((len = fread(buf, 1, 1024, s_fp))) {
		if (len != fwrite(buf, 1, len, d_fp)) {
			ret = -1;
			break;
		}
	}

	fclose(s_fp);
	fclose(d_fp);
	return ret;
}

int f_size(const char* file, int* size)
{
	FILE* fp;

	if (!file || !size) {
		return -1;
	}

	fp = fopen(file, "r");
	if (!fp) {
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fclose(fp);

	return 0;
}
#endif

