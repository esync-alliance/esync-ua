/*
 * tmpl_updateagent.c
 */

#include "tmpl_updateagent.h"

pkg_version_t * package_version = 0;

static int get_tmpl_version(char * pkgName, char ** version) {

    TMPL_VER_GET(pkgName, *version);
    return E_UA_OK;

}

static int set_tmpl_version(char * pkgName, char * version) {

    TMPL_VER_SET(pkgName, version);
    return E_UA_OK;

}

static install_state_t do_tmpl_pre_install(char * pkgName, char * version, char * pkgFile, char ** newFile) {

    return INSTALL_INPROGRESS;

}

static install_state_t do_tmpl_install(char * pkgName, char * version, char * pkgFile) {

    return INSTALL_COMPLETED;

}

static void do_tmpl_post_install(char * pkgName) {

    return;

}

ua_routine_t tmpl_rtns = {
    .on_get_version = get_tmpl_version,
    .on_set_version = set_tmpl_version,
    .on_pre_install = do_tmpl_pre_install,
    .on_install = do_tmpl_install,
    .on_post_install = do_tmpl_post_install
};

ua_routine_t* get_tmpl_routine(void) {

    return &tmpl_rtns;

}
