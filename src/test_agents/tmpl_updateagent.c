/*
 * tmpl_updateagent.c
 */

#include "tmpl_updateagent.h"
#include <string.h>

pkg_version_t* package_version = 0;

#ifdef LIBUA_VER_2_0

static int get_tmpl_version(ua_callback_clt_t* clt)
{
	//TMPL_VER_GET(pkgName, *version);
	return E_UA_OK;

}

static int set_tmpl_version(ua_callback_clt_t* clt)
{
	//TMPL_VER_SET(pkgName, version);
	return E_UA_OK;

}

static install_state_t do_tmpl_pre_install(ua_callback_clt_t* clt)
{
	return INSTALL_IN_PROGRESS;

}

static install_state_t do_tmpl_install(ua_callback_clt_t* clt)
{
	return INSTALL_COMPLETED;

}

static void do_tmpl_post_install(ua_callback_clt_t* clt)
{
	return;

}

static install_state_t do_prepare_install(ua_callback_clt_t* clt)
{
	return INSTALL_READY;

}

#else

static int get_tmpl_version(const char* type, const char* pkgName, char** version)
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

#endif

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
	.on_dmc_presence    = tmpl_on_dmc_presence,
#if TMPL_UA_SUPPORT_SCP_TRANSFER
	.on_transfer_file = do_transfer_file
#endif
};

ua_routine_t* get_tmpl_routine(void)
{
	return &tmpl_rtns;

}
