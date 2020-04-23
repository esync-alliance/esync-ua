/*
 *                          Copyright 2000-2013                      
 *                         Green Hills Software                      
 *
 *    This program is the property of Green Hills Software.  Its
 *    contents constitute the proprietary information of Green Hills
 *    Software.  Your use of this program shall be in accordance with
 *    the shrink-wrap or click-through license agreement accompanying
 *    it or the executed license agreement by and between you or your
 *    company and Green Hills Software, which supersedes the former.
 *    If you have not agreed to the terms of a license agreement with
 *    Green Hills Software, do not use this program and promptly
 *    contact Green Hills Software.
 *
 */

#include <INTEGRITY.h>
#include <stdio.h>
#include <unistd.h>

/* This file is part of the INTEGRITY virtual file system source code.  It provides
 * the entry point (main) for a virtual AddressSpace and calls the internal file system
 * function, __fs_init(), to start the file system tasks.
 *
 * This file is only used by the virtual AddressSpace version of the file
 * system.  A file system virtual AddressSpace must contain this file to ensure that
 * the file system is configured correctly.  The contents of this file may change
 * in future versions.
 */

/* Set the common variable to 1 so that link-time optimizations can be made */
const char __fs_is_virtual = 1;

/* Purposely avoid pulling in vfs_server.h, which only exists in source
 * distributions.
 */
extern Error __fs_init(void);

int vfs_main(void) {
    Error theError;
   
    if ((theError = __fs_init()) != Success) {
	fprintf(console, "Failed to start the file system, Error %d\n", theError);
    }

    Exit(0);
    return 0;
}
#pragma	weak main=vfs_main
