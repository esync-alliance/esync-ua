/************************************************************************
 *  Copyright 2020 © Excelfore Corporation
 *  All rights reserved
 *
 *  Licensed under the Excelfore License Agreement (“License”);
 *  You may not use this file except in compliance with the License.
 *  You may obtain a copy of the License by contacting legal@excelfore.com
 *
 *  Source file name  : string_safe.c
 *  Description       : safer strcpy, strcat, strncpy, strncat functions
 ************************************************************************/
#include "string_safe.h"

size_t strcpy_s(char *dst, const char *src, unsigned int size)
{
    if (size == 0)
        return 0;
    if (strlen(src) >= size) {
        memcpy(dst, src, size-1);
        dst[size-1] = 0;
        return size-1;
    }
#undef strcpy
    strcpy(dst, src);
    return strlen(dst);
}

size_t strcat_s(char *dst, const char *src, unsigned int size)
{
    size_t dstlen = strlen(dst);
    if (size <= dstlen)
        return dstlen;
    if (dstlen+strlen(src) >= size) {
        memcpy(&dst[dstlen], src, (size-1)-dstlen);
        dst[size-1] = 0;
        return size-1;
    }
#undef strcat
    strcat(dst, src);
    return strlen(dst);
}
