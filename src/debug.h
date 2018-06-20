/*
 * debug.h
 */

#ifndef _UA_DEBUG_H_
#define _UA_DEBUG_H_

#include "common.h"

extern ua_cfg_t ua_cfg;

#if !XL4_HAVE_GETTIMEOFDAY
#define _ltime_ \
    char now[1]; \
    now[0] = 0
#else
#define _ltime_ \
    char now[24]; \
    struct tm tmnow; \
    struct timeval tv; \
    memset(now, 0, 24); \
    gettimeofday(&tv, 0); \
    usec_to_msec(&tv); \
    localtime_r(&tv.tv_sec, &tmnow); \
    strftime(now, 20, "%m-%d:%H:%M:%S.", &tmnow); \
    sprintf(now+15, "%03ld", tv.tv_usec)
#endif

#define DBG(a,b...) do { if (ua_cfg.debug) { \
    _ltime_; \
    char * _str = f_asprintf("[%s] %s:%d " a, now, chop_path(__FILE__), __LINE__, ## b); \
    if (_str) { \
        printf("%s\n", _str); \
        fflush(stdout); \
        free(_str); \
    } \
} } while(0)

#define DBG_SYS(a,b...) do { if (ua_cfg.debug) { \
    int _errno = errno; \
    _ltime_; \
    char * _str = f_asprintf("[%s] %s:%d error %s(%d): " a, now, chop_path(__FILE__), __LINE__, strerror(_errno), _errno, ## b); \
    if (_str) { \
        printf("%s\n", _str); \
        fflush(stdout); \
        free(_str); \
    } \
} } while(0)

static inline void usec_to_msec(struct timeval * tv) {

    for (int i=0; i<3; i++) {
        int r = (int)(tv->tv_usec % 10);
        tv->tv_usec /= 10;
        if (r >= 5) {
            tv->tv_usec++;
        }
    }

    if (tv->tv_usec >= 1000) {
        tv->tv_usec -= 1000;
        tv->tv_sec++;
    }

}

#define BOLT_MEM(a) if (!(a)) { \
    err = E_UA_MEMORY; \
    DBG("out of memory"); \
    break; \
} do{}while(0)

// pointer to realloc
// type of pointer
// size
#define BOLT_REALLOC(ptr,type,size,newsize) { \
    int __size = size; \
    void * __aux = realloc(ptr, (__size)*sizeof(type)); \
    if (!__aux) { err = E_UA_MEMORY; DBG("out of memory, realloc %d", __size); break; } \
    ptr = (type*)__aux; \
    newsize = __size; \
} do{}while(0)


#define BOLT(why) err = (why); DBG("setting err %d", err); break; do{}while(0)
#define BOLT_SAY(__err, msg, x...) err = (__err); DBG(msg ", setting err %d", ## x, err); break; do{}while(0)
#define BOLT_IF(cond, __err, msg, x...) if ((cond)) { err = (__err); DBG(msg ", setting err %d", ## x, err); break; } do{}while(0)
#define BOLT_SYS(a, m, x...) if ((a)) { DBG_SYS(m, ## x); err = E_UA_SYS; break; } do{}while(0)
#define BOLT_SUB(a) { err = (a); if (err != E_UA_OK) { BOLT_SAY(err, #a); }} do{}while(0)
#define BOLT_MALLOC(var, how_much) { if (!((var) = f_malloc(how_much))) { BOLT_SAY(E_UA_MEMORY, "failed to alloc %d for %s", how_much, #var); } } do{}while(0)


#endif /* _UA_DEBUG_H_ */
