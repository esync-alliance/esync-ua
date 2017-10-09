/*
 * updateagent.c
 */

#include "ua_version.h"
#include "updateagent.h"
#include "handler.h"
#include "config.h"
#include "debug.h"
#include "common.h"

int get_version(char * pkgName, char ** version);
int set_version(char * pkgName, char * version);
install_state_t do_install(char * pkgName, char * version, char * pkgFile);

int debug = 1;

int main(int argc, char ** argv) {

    printf("updateagent %s", BUILD_VERSION);

    //todo take cert dir arg

    char * label = "updateagent";

    char * cdir = get_cert_dir(label, argv[0]);

    update_agent_t ua;
    memset(&ua, 0, sizeof(update_agent_t));

    ua.do_set_version   = set_version;
    ua.do_get_version   = get_version;
    ua.do_install       = do_install;
    ua.cert_dir         = cdir;
    ua.url              = URL;

    if (ua_handler_start(&ua)) {
        DBG("Updateagent start failed!");
        _exit(1);
    }

    while (1) {
        sleep(60);
    }

}



int get_version(char * pkgName, char ** version) {

    *version = "null";
    return E_UA_OK;

}

int set_version(char * pkgName, char * version) {

    return E_UA_OK;

}

install_state_t do_install(char * pkgName, char * version, char * pkgFile) {

    return INSTALL_COMPLETED;

}
