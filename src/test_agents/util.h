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
#include "misc.h"
#if HAVE_LIMITS_H
#include <linux/limits.h>
#endif

#ifdef LIBUA_VER_2_0
#include "esyncua.h"
#else
#include "xl4ua.h"
#endif //LIBUA_VER_2_0

extern int ua_debug_lvl;
static inline const char* cut_path(const char* path)
{
	const char* aux = strrchr(path, '/');

	if (aux) { return aux + 1; }
	return path;
}

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
#define A_INFO_MSG(fmt, a ...) do { if (ua_debug_lvl == 4 || ua_debug_lvl == 5) { \
					    _ltime_; \
					    printf("[%s] %s:%d " fmt "\n", __now, cut_path(__FILE__), __LINE__, ## a); \
					    fflush(stdout); \
				    } } while (0);
#if 0
#define A_INFO_MSG(a,b ...)    do { if (ua_debug == 4 || ua_debug == 5) { \
					    _ltime_; \
					    char* _str = f_asprintf("[%s] %s:%d " a, __now, chop_path(__FILE__), __LINE__, ## b); \
					    if (_str) { \
						    printf("%s\n", _str); \
						    fflush(stdout); \
						    free(_str); \
					    } \
				    } } while (0)
#endif
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

#ifdef LIBUA_VER_2_0
int do_transfer_file(ua_callback_ctl_t* ctl);

#else
int do_transfer_file(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile);
#endif

#endif /* UTIL_H */
