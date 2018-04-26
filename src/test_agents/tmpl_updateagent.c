/*
 * tmpl_updateagent.c
 */

#include <xl4ua.h>
#include <string.h>
#include "tmpl_updateagent.h"

char version_string[10] = "";

static int get_tmpl_version(char * pkgName, char ** version) {

    *version = version_string;
    return E_UA_OK;

}

static int set_tmpl_version(char * pkgName, char * version) {

    strcpy(version_string, version);
    return E_UA_OK;

}

static install_state_t do_tmpl_install(char * pkgName, char * version, char * pkgFile) {

    return INSTALL_COMPLETED;

}

static install_state_t do_tmpl_pre_install(char * pkgName, char * version, char * pkgFile) {

    return INSTALL_INPROGRESS;

}

static void do_tmpl_post_install() {

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
