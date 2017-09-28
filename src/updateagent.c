/*
 * updateagent.c
 */

#include "updateagent.h"
#include "handler.h"
#include "config.h"

int get_version(char * pkgName, char ** version);
int set_version(char * pkgName, char * version);
install_state_t do_install(char * pkgName, char * version, char * pkgFile);

int debug = 1;

int main(int argc, char ** argv) {

    update_agent_t ua;
    memset(&ua, 0, sizeof(update_agent_t));

    ua.do_set_version   = set_version;
    ua.do_get_version   = get_version;
    ua.do_install       = do_install;
    ua.ua_type          = UA;
    ua.url              = URL;

    ua_handler_start(&ua);

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
