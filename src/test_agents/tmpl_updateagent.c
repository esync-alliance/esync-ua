/*
 * tmpl_updateagent.c
 */

#include "tmpl_updateagent.h"

pkg_version_t* package_version = 0;

#ifdef LIBUA_VER_2_0

static int get_tmpl_version(ua_callback_ctl_t* ctl)
{
	TMPL_VER_GET(ctl->pkg_name, ctl->version);
	return E_UA_OK;
}

static int set_tmpl_version(ua_callback_ctl_t* ctl)
{
	TMPL_VER_SET(ctl->pkg_name, ctl->version);
	return E_UA_OK;
}

static download_state_t do_tmpl_prepare_download(ua_callback_ctl_t* ctl)
{
	XL4_UNUSED(ctl);
	return DOWNLOAD_CONSENT;
}

static install_state_t do_tmpl_pre_install(ua_callback_ctl_t* ctl)
{
	XL4_UNUSED(ctl);
	return INSTALL_IN_PROGRESS;
}

static install_state_t do_tmpl_install(ua_callback_ctl_t* ctl)
{
	XL4_UNUSED(ctl);
	return INSTALL_COMPLETED;
}

static void do_tmpl_post_install(ua_callback_ctl_t* ctl)
{
	XL4_UNUSED(ctl);
	return;
}

static install_state_t do_tmpl_prepare_install(ua_callback_ctl_t* ctl)
{
	XL4_UNUSED(ctl);
	return INSTALL_READY;
}

#else // LIBUA_VER_2_0

static int get_tmpl_version(const char *type, const char *pkgName, char **version)
{
	TMPL_VER_GET(pkgName, *version);
	XL4_UNUSED(type);
	return E_UA_OK;
}

static int set_tmpl_version(const char *type,
                            const char *pkgName,
                            const char *version)
{
	TMPL_VER_SET(pkgName, version);
	XL4_UNUSED(type);
	return E_UA_OK;
}

static download_state_t do_tmpl_prepare_download(const char *type,
                                                 const char *pkgName,
                                                 const char *version)
{
	XL4_UNUSED(type);
	XL4_UNUSED(pkgName);
	XL4_UNUSED(version);
	return DOWNLOAD_CONSENT;
}

static install_state_t do_tmpl_pre_install(const char *type,
                                           const char *pkgName,
                                           const char *version,
                                           const char *pkgFile)
{
	XL4_UNUSED(type);
	XL4_UNUSED(pkgName);
	XL4_UNUSED(version);
	XL4_UNUSED(pkgFile);
	return INSTALL_IN_PROGRESS;
}

static install_state_t do_tmpl_install(const char *type,
                                       const char *pkgName,
                                       const char *version,
                                       const char *pkgFile)
{
	XL4_UNUSED(type);
	XL4_UNUSED(pkgName);
	XL4_UNUSED(version);
	XL4_UNUSED(pkgFile);
	return INSTALL_COMPLETED;
}

static void do_tmpl_post_install(const char *type, const char *pkgName)
{
	XL4_UNUSED(type);
	XL4_UNUSED(pkgName);
	return;
}

static install_state_t do_tmpl_prepare_install(const char *type,
                                               const char *pkgName,
                                               const char *version,
                                               const char *pkgFile,
                                               char **newFile)
{
	XL4_UNUSED(type);
	XL4_UNUSED(pkgName);
	XL4_UNUSED(version);
	XL4_UNUSED(pkgFile);
	XL4_UNUSED(*newFile);
	return INSTALL_READY;
}

#if TMPL_UA_SUPPORT_SCP_TRANSFER
static int do_tmpl_transfer_file(const char *type,
                                 const char *pkgName,
                                 const char *version,
                                 const char *pkgFile,
                                 char **newFile)
{
	XL4_UNUSED(type);
	XL4_UNUSED(pkgName);
	XL4_UNUSED(version);
	XL4_UNUSED(pkgFile);
	XL4_UNUSED(*newFile);
	return E_UA_OK;
}
#endif

#endif //LIBUA_VER_2_0

static int tmpl_on_dmc_presence(dmc_presence_t* dp)
{
	XL4_UNUSED(dp);
	return 0;
}

static void tmpl_on_esyncbus_status(int status)
{
	XL4_UNUSED(status);
}

ua_routine_t tmpl_rtns = {
	.on_get_version     = get_tmpl_version,
	.on_set_version     = set_tmpl_version,
	.on_prepare_download= do_tmpl_prepare_download,
	.on_pre_install     = do_tmpl_pre_install,
	.on_install         = do_tmpl_install,
	.on_post_install    = do_tmpl_post_install,
	.on_prepare_install = do_tmpl_prepare_install,
#if TMPL_UA_SUPPORT_SCP_TRANSFER
	.on_transfer_file   = do_tmpl_transfer_file,
#endif
	.on_dmc_presence    = tmpl_on_dmc_presence,
	.on_esyncbus_status = tmpl_on_esyncbus_status
};

ua_routine_t* get_tmpl_routine(void)
{
	return &tmpl_rtns;
}
