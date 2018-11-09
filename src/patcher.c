#include "delta.h"
#include "ua_version.h"

int ua_debug;

static void _help(const char * app) {
    printf("Usage: %s [OPTION...] oldfile difffile newfile \n\n%s", app,
            "Options:\n"
            "  -c <path>  : path to cache directory (default: \"/tmp/deltapatcher/\")\n"
            "  -a <cap>   : delta capability\n"
            "  -d         : enable verbose\n"
            "  -m <size>  : read/write buffer size, in kilobytes\n"
            "  -h         : display this help and exit\n"
    );
    _exit(1);
}

int main(int argc, char ** argv) {

    printf("Delta Patcher %s\n", BUILD_VERSION);

    int err = E_UA_OK;
    int c = 0;
    delta_cfg_t cfg;
    memset(&cfg, 0, sizeof(delta_cfg_t));

    char * cache_dir   = "/tmp/deltapatcher/";

    while ((c = getopt(argc, argv, ":c:a:m:dh")) != -1) {
        switch (c) {
            case 'c':
                cache_dir = optarg;
                break;
            case 'a':
                cfg.delta_cap = optarg;
                break;
            case 'd':
                ua_debug = 1;
                break;
            case 'm':
                if ((ua_rw_buff_size = atoi(optarg) * 1024) > 0)
                break;
            case 'h':
            default:
                _help(argv[0]);
                break;
        }
    }

    if (argc - optind < 3) {
        _help(argv[0]);
    }

    const char * oldfile  = argv[optind];
    const char * difffile = argv[optind+1];
    const char * newfile  = argv[optind+2];

    do {

        if ((err = delta_init(cache_dir, &cfg))) {
            printf("Initialization failed!\n");
            break;
        }

        printf("Delta capability supported : %s\n", get_delta_capability());

        if (!is_delta_package(difffile)) {
            printf("%s is not a delta file\n", difffile);
            err = E_UA_ARG; break;
        }

        if ((err = delta_reconstruct(oldfile, difffile, newfile))) {
            printf("Delta reconstruction failed!\n");
        } else {
            printf("Delta reconstruction success!\n");
        }

    } while (0);

    delta_stop();

    return err != E_UA_OK;
}
