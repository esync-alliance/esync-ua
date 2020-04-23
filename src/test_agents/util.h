/*
 * util.h
 */
#ifndef UTIL_H
#define UTIL_H

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include "build_config.h"
#if HAVE_LIMITS_H
#include <linux/limits.h>
#endif

#define _ltime_ \
        char __now[24]; \
        struct tm __tmnow; \
        struct timeval __tv; \
        memset(__now, 0, 24); \
        gettimeofday(&__tv, 0); \
        localtime_r(&__tv.tv_sec, &__tmnow); \
        strftime(__now, 20, "%m-%d:%H:%M:%S.", &__tmnow); \
        sprintf(__now+15, "%03d", (int)(__tv.tv_usec/1000))

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define DBG(fmt, a ...) {do { _ltime_; printf("[%s] %s:%d " fmt "\n", __now, __FILENAME__, __LINE__, ## a); } while (0); }

typedef struct scp_info {
	char* url;
	char* user;
	char* password;
	char* scp_bin;
	char* sshpass_bin;
	char* local_dir;
	char dest_path[PATH_MAX];

}scp_info_t;

int xl4_run_cmd(char* argv[]);
scp_info_t* scp_init(scp_info_t* scp);
char* scp_get_file(scp_info_t* scp, char* remote_path);
scp_info_t* scp_get_info(void);
int do_transfer_file(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile);

#endif /* UTIL_H */
