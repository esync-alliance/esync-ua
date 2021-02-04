/* Copyright(c) 2021 Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * make_backup.c
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/limits.h>
#include "misc.h"
#include "xml.h"
#include "debug.h"

int ua_debug_lvl = 0;
int ua_debug     = 1;

static void _help(const char* app)
{
	printf("Construct eSync backup directory, mandatory arguments are package name, version, and file path.\n");
	printf("  Usage: %s [OPTION...] package_path \n\n%s", app,
	       "  Options:\n"
	       "  -b <path>  : path to backup directory (default: \"/data/sota/esync/\")\n"
	       "  -p <name>  : package name\n"
	       "  -v <ver>   : package version\n"
	       "  -e         : enable error msg\n"
	       "  -w         : enable warning msg\n"
	       "  -i         : enable information msg\n"
	       "  -d         : enable all debug msg\n"
	       "  -h         : display this help and exit\n"
	       );
	_exit(1);
}


int main(int argc, char** argv)
{
	int c                     = 0, err = 0;
	char backup_dir[PATH_MAX] = "/data/sota/esync";
	char* pkg_name            = 0;
	char* backup_manifest     = 0;
	pkg_file_t pf = {0};

	while ((c = getopt(argc, argv, ":b:p:v:ewidDFh")) != -1) {
		switch (c) {
			case 'b':
				strncpy(backup_dir, optarg, sizeof(backup_dir));
				break;
			case 'p':
				pkg_name = f_strdup(optarg);
				break;
			case 'v':
				pf.version = f_strdup(optarg);
				break;
			case 'e':
				ua_debug = 1;
				break;
			case 'w':
				ua_debug = 2;
				break;
			case 'i':
				ua_debug = 3;
				break;
			case 'd':
				ua_debug = 4;
				break;
			case 'h':
			default:
				_help(argv[0]);
				break;
		}
	}

	if (argc - optind < 1)
		_help(argv[0]);

	pf.file = f_strdup(argv[optind]);

	if (pkg_name && pf.version && pf.file) {
		do {
			char* bname     = f_basename(pf.file);
			char* dest_file = 0;

			if (backup_dir[0] != '/') {
				char cwd[PATH_MAX];
				if (getcwd(cwd, sizeof(cwd)) != NULL) {
					dest_file = JOIN(cwd,backup_dir, "backup", pkg_name, pf.version, bname);
				}
			} else {
				dest_file = JOIN(backup_dir, "backup", pkg_name, pf.version, bname);
			}

			A_INFO_MSG("prepare esync backup for %s version %s in directory %s \n", pkg_name, pf.version, backup_dir);
			backup_manifest = JOIN(backup_dir, "backup", pkg_name, MANIFEST_PKG);
			BOLT_SYS(chkdirp(dest_file), "failed to create directory for %s", dest_file);

			calc_sha256_x(pf.file, pf.sha_of_sha);
			calculate_sha256_b64(pf.file, pf.sha256b64);

			BOLT_IF(add_pkg_file_manifest(backup_manifest, &pf), err, "failed to create manifest %s", backup_manifest);

			BOLT_IF(copy_file(pf.file, dest_file), err, "failed to copy file from %s to %s", pf.file, dest_file);

			A_INFO_MSG("eSync backup has been created in %s/backup/%s", backup_dir, pkg_name);

		} while (0);

	} else {
		if(!pkg_name)
			printf("\nPlease provide package name.\n");
		
		if(!pf.file)
			printf("\nPlease provide package file path.\n");

		if(!pf.version)
			printf("\nPlease provide package version.\n");

		_help(argv[0]);

	}

	Z_FREE(pkg_name);
	Z_FREE(pf.version);
	Z_FREE(pf.file);
	Z_FREE(backup_manifest);
	return err;

}
