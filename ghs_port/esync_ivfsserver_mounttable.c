/*
 *                          Copyright 2001-2016                   
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
#include <sys/mnttab.h>

vfs_MountEntry vfs_MountTable[] = {
    {
	"<ram:512k>ramroot:0",
	"/",
	MOUNT_FFS, 
	(char *)0, 
	0, 
	0
    },
    
    {
#ifdef USE_KERNEL_SDIO_MMC
      "<iod>SDHIDev:a",
#else
      "<sdhc_mmc:rcar_sdhi_gen3>SDHIDev:a",
#endif
      "/sdcard",
      MOUNT_MSDOS,
      0,
      MNTTAB_MAKEMP,
      0
    },
    {NULL, NULL, NULL, NULL, 0, 0} /* Must end with NULL/0 entry */
};

