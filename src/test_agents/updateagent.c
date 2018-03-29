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

static void _help()
{
    printf("%s",
            "Usage: updateagent [OPTION...]\n\n"
            "Options:\n"
            "  -u <url>   : url of xl4bus broker (default: \"tcp://localhost:9133\")\n"
            "  -k <path>  : path to certificate directory (default: \"./../pki/certs/updateagent\")\n"
            "  -b <path>  : path to backup directory (default: \"/data/sota/esync/\")\n"
            "  -c <path>  : path to cache directory (default: \"/tmp/esync/\")\n"
            "  -d         : enable verbose\n"
            "  -D         : disable delta reconstruction\n"
            "  -h         : display this help and exit\n"
    );
    _exit(1);
}


int main(int argc, char ** argv) {

    printf("updateagent %s\n", BUILD_VERSION);

    int c = 0;
    ua_cfg_t cfg;
    memset(&cfg, 0, sizeof(ua_cfg_t));

    cfg.debug       = 0;
    cfg.delta       = 1;
    cfg.cert_dir    = "./../pki/certs/updateagent";
    cfg.url         = "tcp://localhost:9133";
    cfg.cache_dir   = "/tmp/esync/";
    cfg.backup_dir  = "/data/sota/esync/";

    while ((c = getopt(argc, argv, ":k:u:b:c:t:dvh")) != -1) {
        switch (c) {
            case 'k':
                cfg.cert_dir = optarg;
                break;
            case 'u':
                cfg.url = optarg;
                break;
            case 'b':
                cfg.backup_dir = optarg;
                break;
            case 'c':
                cfg.cache_dir = optarg;
                break;
            case 't':
                uah[0].type_handler = optarg;
                break;
            case 'd':
                cfg.debug = 1;
                break;
            case 'D':
                cfg.delta = 0;
                break;
            case 'h':
            default:
                _help();
                break;
        }
    }


    if (ua_init(&cfg)) {
        printf("Updateagent failed!");
        _exit(1);
    }

    int l = sizeof(uah)/sizeof(ua_handler_t);
    ua_register(uah, l);

    while (1) { pause(); }

}
