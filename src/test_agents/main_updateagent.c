/*
 * main_updateagent.c
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "tmpl_updateagent.h"

ua_handler_t uah[] = {
	{"/template", get_tmpl_routine }
};

static void _help(const char* app)
{
	printf("Usage: %s [OPTION...]\n\n%s", app,
	       "Options:\n"
	       "  -u <url>   : url of xl4bus broker (default: \"tcp://localhost:9133\")\n"
	       "  -k <path>  : path to certificate directory (default: \"./../pki/certs/updateagent\")\n"
	       "  -b <path>  : path to backup directory (default: \"/data/sota/esync/\")\n"
	       "  -c <path>  : path to cache directory (default: \"/tmp/esync/\")\n"
	       "  -d         : enable verbose\n"
	       "  -D         : disable delta reconstruction\n"
	       "  -a <cap>   : delta capability\n"
	       "  -m <size>  : read/write buffer size, in kilobytes\n"
	       "  -t <type>  : handler type\n"
	       "  -M <0/1/2/3> : update mode - 0: success(default); 1: failure; 2: toggle; 3: rollback"
	       "  -r <path>  : path to rbconf file (default: \"/data/sota/rbConf\")\n"
	       "  -Z         : emulate reboot after update (default no reboot) \n"
	       "  -h         : display this help and exit\n"
	       );
	_exit(1);
}


int main(int argc, char** argv)
{
	printf("updateagent %s, xl4bus %s\n", ua_get_updateagent_version(), ua_get_xl4bus_version());

	int c = 0;
	ua_cfg_t cfg;
	memset(&cfg, 0, sizeof(ua_cfg_t));
	int mode          = 0;
	int reboot_enable = 0;

	cfg.debug                         = 0;
	cfg.delta                         = 1;
	cfg.cert_dir                      = "./../pki/certs/updateagent";
	cfg.url                           = "tcp://localhost:9133";
	cfg.cache_dir                     = "/tmp/esync/";
	cfg.backup_dir                    = "/data/sota/esync/";
	cfg.reboot_support                = 0;
	cfg.package_verification_disabled = 0;

	while ((c = getopt(argc, argv, ":k:u:b:c:a:m:t:M:r:dDZh")) != -1) {
		switch (c) {
			case 'k':
				cfg.cert_dir = optarg;
				break;
			case 'u':
				cfg.url = optarg;
				break;
			case 'b':
				cfg.backup_dir = optarg;
				set_backup_dir(optarg);
				break;
			case 'c':
				cfg.cache_dir = optarg;
				break;
			case 'a':
				cfg.delta_config = &(delta_cfg_t){.delta_cap = optarg};
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
			case 'm':
				if ((cfg.rw_buffer_size = atoi(optarg)) > 0)
					break;
			case 'Z':
				reboot_enable = 1;
				break;
			case 'M':
				mode = atoi(optarg);
				break;
			case 'r':
				set_rbConf_path(optarg);
				break;
			case 'h':
			default:
				_help(argv[0]);
				break;
		}
	}

	set_test_installation_mode((update_mode_t)mode, reboot_enable);

	if (ua_init(&cfg)) {
		printf("Updateagent failed!");
		_exit(1);
	}

	int l = sizeof(uah)/sizeof(ua_handler_t);
	ua_register(uah, l);

	while (1) { pause(); }

}
