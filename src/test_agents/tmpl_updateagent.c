/*
 * tmpl_updateagent.c
 */

#include "tmpl_updateagent.h"
#include <string.h>

#define MAX_VER_LEN 512
char tmpl_version[MAX_VER_LEN] = {0};

static int get_tmpl_version(const char* type, const char* pkgName, char** version)
{
	*version = tmpl_version;
	return E_UA_OK;

}

static int set_tmpl_version(const char* type, const char* pkgName, const char* version)
{
	if(version)
		strncpy(tmpl_version, version, sizeof(tmpl_version));
	return E_UA_OK;

}

static install_state_t do_tmpl_pre_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	return INSTALL_IN_PROGRESS;

}

static install_state_t do_tmpl_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	return INSTALL_COMPLETED;

}

static void do_tmpl_post_install(const char* type, const char* pkgName)
{
	return;

}

static install_state_t do_prepare_install(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	return INSTALL_READY;

}

static int tmpl_on_dmc_presence(dmc_presence_t* dp)
{
	return 0;

}

ua_routine_t tmpl_rtns = {
	.on_get_version     = get_tmpl_version,
	.on_set_version     = set_tmpl_version,
	.on_pre_install     = do_tmpl_pre_install,
	.on_install         = do_tmpl_install,
	.on_post_install    = do_tmpl_post_install,
	.on_prepare_install = do_prepare_install,
	.on_dmc_presence    = tmpl_on_dmc_presence
};

ua_routine_t* get_tmpl_routine(void)
{
	return &tmpl_rtns;

}
