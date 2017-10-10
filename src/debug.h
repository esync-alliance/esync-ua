#ifndef _UA_DEBUG_H_
#define _UA_DEBUG_H_

#include "common.h"


extern int debug;

#define _ltime_ \
    char now[20]; \
    struct tm tmnow; \
    struct timeval tv; \
    memset(now, 0, 20); \
    gettimeofday(&tv, 0); \
    localtime_r(&tv.tv_sec, &tmnow); \
    strftime(now, 19, "%m-%d:%H:%M:%S", &tmnow)

#define DBG(a,b...) do { if (debug) { \
    _ltime_; \
    char * _str = f_asprintf("[%s] %s:%d " a, now, chop_path(__FILE__), __LINE__, ## b); \
    if (_str) { \
        printf("%s\n", _str); \
        free(_str); \
    } \
} } while(0)

#define DBG_SYS(a,b...) do { if (debug) { \
    int _errno = errno; \
    _ltime_; \
    char * _str = f_asprintf("[%s] %s:%d error %s(%d): " a, now, chop_path(__FILE__), __LINE__, strerror(_errno), _errno, ## b); \
    if (_str) { \
        printf("%s\n", _str); \
        free(_str); \
    } \
} } while(0)

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

#define SAFE_STR(s) (s?s:"null")

#define BOLT(why) err = (why); DBG("setting err %d", err); break; do{}while(0)
#define BOLT_SAY(__err, msg, x...) err = (__err); DBG(msg ", setting err %d", ## x, err); break; do{}while(0)
#define BOLT_IF(cond, __err, msg, x...) if ((cond)) { err = (__err); DBG(msg ", setting err %d", ## x, err); break; } do{}while(0)
#define BOLT_SYS(a, m, x...) if ((a)) { DBG_SYS(m, ## x); err = E_UA_SYS; break; } do{}while(0)
#define BOLT_SUB(a) { err = (a); if (err != E_UA_OK) { BOLT_SAY(err, #a); }} do{}while(0)
#define BOLT_MALLOC(var, how_much) { if (!((var) = f_malloc(how_much))) { BOLT_SAY(E_UA_MEMORY, "failed to alloc %d for %s", how_much, #var); } } do{}while(0)

static inline const char * chop_path(const char * path) {
    const char * aux = strrchr(path, '/');
    if (aux) { return aux + 1; }
    return path;
}

#endif // _UA_DEBUG_H_
