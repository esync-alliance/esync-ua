#ifndef _PORTING_H_
#define _PORTING_H_

#include <stdarg.h>

#ifdef __QNX__

#define CLOCK_MONOTONIC_RAW  CLOCK_MONOTONIC

int vasprintf(char **buf, const char *fmt, va_list ap);
char* strcasestr(const char *s, const char *find);

#endif /* __QNX__ */

#endif /* _PORTING_H_ */
