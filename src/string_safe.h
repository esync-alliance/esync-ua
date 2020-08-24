/************************************************************************
 *  Copyright 2020 © Excelfore Corporation
 *  All rights reserved
 *
 *  Licensed under the Excelfore License Agreement (“License”);
 *  You may not use this file except in compliance with the License.
 *  You may obtain a copy of the License by contacting legal@excelfore.com
 *
 *  Source file name  : string.h
 *  Description       : safer strcpy, strcat, strncpy, strncat functions deceleration
 ************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
size_t strcpy_s(char *dst, const char *src, unsigned int size);
size_t strcat_s(char *dst, const char *src, unsigned int size);
