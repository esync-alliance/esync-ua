#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "porting.h"

int vasprintf(char **buf, const char *fmt, va_list ap)
{
    static char _T_emptybuffer = '\0';
    int chars;
    char *b;

    if(!buf) { return -1; }

#ifdef WIN32
    chars = _vscprintf(fmt, ap)+1;
#else /* !defined(WIN32) */
    /* CAW: RAWR! We have to hope to god here that vsnprintf doesn't overwrite
       our buffer like on some 64bit sun systems.... but hey, its time to move on */
    chars = vsnprintf(&_T_emptybuffer, 0, fmt, ap)+1;
    if(chars < 0) { chars *= -1; } /* CAW: old glibc versions have this problem */
#endif /* defined(WIN32) */

    b = (char*)malloc(sizeof(char)*chars);
    if(!b) { return -1; }

    if((chars = vsprintf(b, fmt, ap)) < 0)
    {
        free(b);
    } else {
        *buf = b;
    }

    return chars;
}

char* strcasestr(const char *s, const char *find)
{
    char c, sc;
    size_t len;
    if ((c = *find++) != 0) {
        c = (char)tolower((unsigned char)c);
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == 0)
                    return (NULL);
            } while ((char)tolower((unsigned char)sc) != c);
        } while (strncasecmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}
