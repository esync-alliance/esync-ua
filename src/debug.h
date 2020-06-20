/*
 * debug.h
 */

#ifndef UA_DEBUG_H_
#define UA_DEBUG_H_

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include "misc.h"
#include "xl4ua.h"

extern int ua_debug;

#ifdef HAVE_INSTALL_LOG_HANDLER
extern ua_log_handler_f ua_log_handler;
#endif

#if !XL4_HAVE_GETTIMEOFDAY
#define _ltime_ \
        char __now[1]; \
        __now[0] = 0
#else
#define _ltime_ \
        char __now[24]; \
        struct tm __tmnow; \
        struct timeval __tv; \
        memset(__now, 0, 24); \
        gettimeofday(&__tv, 0); \
        localtime_r(&__tv.tv_sec, &__tmnow); \
        strftime(__now, 20, "%m-%d:%H:%M:%S.", &__tmnow); \
        sprintf(__now+15, "%03d", (int)(__tv.tv_usec/1000))
#endif

#ifdef HAVE_INSTALL_LOG_HANDLER

#define DBG(a,b ...)     do { if (ua_debug) { \
				      _ltime_; \
				      char* _str = f_asprintf("[%s] %s:%d " a, __now, chop_path(__FILE__), __LINE__, ## b); \
				      if (_str) { \
					      if (ua_log_handler) { \
						      ua_log_handler(ua_debug_log, _str); \
					      } else { \
						      printf("%s\n", _str); \
						      fflush(stdout); \
					      } \
					      free(_str); \
				      } \
			      } } while (0)

#define DBG_SYS(a,b ...) do { if (ua_debug) { \
				      int _errno = errno; \
				      _ltime_; \
				      char* _str = f_asprintf("[%s] %s:%d error %s(%d): " a, __now, chop_path(__FILE__), __LINE__, strerror(_errno), _errno, ## b); \
				      if (_str) { \
					      if (ua_log_handler) { \
						      ua_log_handler(ua_debug_log, _str); \
					      } else { \
						      printf("%s\n", _str); \
						      fflush(stdout); \
					      } \
					      free(_str); \
				      } \
			      } } while (0)

#define A_ERROR_MSG	DBG
#define A_WARN_MSG	DBG
#define A_INFO_MSG	DBG
#define A_DEBUG_MSG	DBG

#else

#define A_ERROR_MSG(a,b ...) do { if (ua_debug >= 0) { \
				      int _errno = errno; \
				      _ltime_; \
				      char* _str = f_asprintf("[%s] %s:%d error %s(%d): " a, __now, chop_path(__FILE__), __LINE__, strerror(_errno), _errno, ## b); \
				      if (_str) { \
					      printf("%s\n", _str); \
					      fflush(stdout); \
					      free(_str); \
				      } \
			      } } while (0)

#define A_WARN_MSG(a,b ...)     do { if (ua_debug >= 1) { \
				      _ltime_; \
				      char* _str = f_asprintf("[%s] %s:%d " a, __now, chop_path(__FILE__), __LINE__, ## b); \
				      if (_str) { \
					      printf("%s\n", _str); \
					      fflush(stdout); \
					      free(_str); \
				      } \
			      } } while (0)

#define A_INFO_MSG(a,b ...)     do { if (ua_debug >= 2) { \
				      _ltime_; \
				      char* _str = f_asprintf("[%s] %s:%d " a, __now, chop_path(__FILE__), __LINE__, ## b); \
				      if (_str) { \
					      printf("%s\n", _str); \
					      fflush(stdout); \
					      free(_str); \
				      } \
			      } } while (0)

#define A_DEBUG_MSG(a,b ...)     do { if (ua_debug >= 3) { \
				      _ltime_; \
				      char* _str = f_asprintf("[%s] %s:%d " a, __now, chop_path(__FILE__), __LINE__, ## b); \
				      if (_str) { \
					      printf("%s\n", _str); \
					      fflush(stdout); \
					      free(_str); \
				      } \
			      } } while (0)

#endif

#define BOLT_MEM(a) if (!(a)) { \
		err = E_UA_MEMORY; \
		A_INFO_MSG("out of memory"); \
		break; \
} do {} while (0)

// pointer to realloc
// type of pointer
// size
#define BOLT_REALLOC(ptr,type,size,newsize) { \
		int __size  = size; \
		void* __aux = realloc(ptr, (__size)*sizeof(type)); \
		if (!__aux) { err = E_UA_MEMORY; A_INFO_MSG("out of memory, realloc %d", __size); break; } \
		ptr     = (type*)__aux; \
		newsize = __size; \
} do {} while (0)


#define BOLT(why)                        err = (why); A_ERROR_MSG("setting err %d", err); break; do {} while (0)
#define BOLT_SAY(__err, msg, x ...)      {err = (__err); A_ERROR_MSG(msg ", setting err %d", ## x, err); break;}
#define BOLT_IF(cond, __err, msg, x ...) if ((cond)) { err = (__err); A_ERROR_MSG(msg ", setting err %d", ## x, err); break; } do {} while (0)
#define BOLT_SYS(a, m, x ...)            { if ((a)) { A_ERROR_MSG(m, ## x); err = E_UA_SYS; break; } }
#define BOLT_SUB(a)                      { err = (a); if (err != E_UA_OK) { BOLT_SAY(err, # a); }} do {} while (0)
#define BOLT_MALLOC(var, how_much)       { if (!((var) = f_malloc(how_much))) { BOLT_SAY(E_UA_MEMORY, "failed to alloc %d for %s", how_much, # var); } } do {} while (0)


#endif /* UA_DEBUG_H_ */
