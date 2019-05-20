/*
 * tmpl_updateagent.c
 */

#include "tmpl_updateagent.h"
#include <fcntl.h>           /* Definition of AT_* constants */
#include <unistd.h>
#include <stdio.h>

pkg_version_t* package_version = 0;
update_mode_t test_update_mode = 0;
int test_reboot = 0;

int get_tmpl_version(const char* type, const char* pkgName, char** version)
{

	TMPL_VER_GET(pkgName, *version);
	return E_UA_OK;

}

static int set_tmpl_version(const char* type, const char* pkgName, const char* version)
{
	TMPL_VER_SET(pkgName, version);
	return E_UA_OK;

}

static install_state_t do_tmpl_pre_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	return INSTALL_IN_PROGRESS;

}

static install_state_t do_tmpl_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	install_state_t rc = INSTALL_COMPLETED;
	static int cnt = 0;
	printf("Emulate installation of %s for %s(%s) using %s\n", type, pkgName, version, pkgFile);
	if(test_reboot) {
		printf("Emulate system reboot for update installation.\n");
		abort();
	} else {
		switch (test_update_mode) {
			case UPDATE_MODE_FAILURE:
				rc = INSTALL_FAILED;
				break;

			case UPDATE_MODE_ALTERNATE_FAILURE_SUCCESSFUL:
				if(cnt++ % 2 == 0) {
					rc = INSTALL_FAILED;
				}
				break;
				
			case UPDATE_MODE_SUCCESSFUL:
				rc = INSTALL_COMPLETED;
				break;
			default:
				break;
		}
	}

	if(rc == INSTALL_COMPLETED)
		printf("Emulate installation succeeded\n");
	else
		printf("Emulate installation failed(%d)\n", rc);

	return rc;

}

static void do_tmpl_post_install(const char* type, const char* pkgName)
{
	return;

}

static install_state_t do_prepare_install(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	return INSTALL_READY;

}

static download_state_t do_prepare_download(const char* type, const char* pkgName, const char* version)
{
	return DOWNLOAD_CONSENT;

}

static int tmpl_on_dmc_presence(dmc_presence_t *dp)
{
	return 0;
}
ua_routine_t tmpl_rtns = {
	.on_get_version      = get_tmpl_version,
	.on_set_version      = set_tmpl_version,
	.on_pre_install      = do_tmpl_pre_install,
	.on_install          = do_tmpl_install,
	.on_post_install     = do_tmpl_post_install,
	.on_prepare_install  = do_prepare_install,
	.on_prepare_download = do_prepare_download,
	.on_dmc_presence     = tmpl_on_dmc_presence
};

ua_routine_t* get_tmpl_routine(void)
{
	return &tmpl_rtns;

}

void set_test_installation_mode(update_mode_t mode, int reboot)
{
	test_update_mode = mode;
	test_reboot = reboot;
}