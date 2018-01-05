/*
 * updateagent.c
 */

#include "ua_version.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <xl4ua.h>
#include "tmpl_updateagent.h"

ua_handler_t uah[] = {
        {"/template", get_tmpl_routine }
};

int main(int argc, char ** argv) {

    printf("updateagent %s", BUILD_VERSION);
    //todo take cert dir arg

    char * cdir = "./../pki/certs/updateagent";

    ua_cfg_t cfg;
    memset(&cfg, 0, sizeof(ua_cfg_t));

    cfg.cert_dir     = cdir;
    cfg.debug        = 1;
    cfg.url          = "tcp://localhost:9133";

    if (ua_init(&cfg)) {
        printf("Updateagent failed!");
        _exit(1);
    }

    int l = sizeof(uah)/sizeof(ua_handler_t);
    ua_register(uah, l);

    while (1) { pause(); }

}
