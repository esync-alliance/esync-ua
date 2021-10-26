/*
 * updateagent.c
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "tmpl_updateagent.h"

#define BASE_TEN_CONVERSION 10

int ua_debug_lvl = 0;

#ifdef LIBUA_VER_2_0
ua_handler_t uah[] = {
	{"/ECU/ROM", get_tmpl_routine, "UA SELF REF" }
};
#else
ua_handler_t uah[] = {
	{"/ECU/ROM", get_tmpl_routine}
};
#endif 
static int stop = 0;

static void sig_handler(int num)
{
	stop = 1;
	XL4_UNUSED(num);
}

static void _help(const char* app)
{
	printf("Usage: %s [OPTION...]\n\n%s", app,
	       "Options:\n"
	       "  -a <cap>   : delta capability\n"
	       "  -b <path>  : path to backup directory (default: \"/data/sota/esync/\")\n"
	       "  -c <path>  : path to cache directory (default: \"/tmp/esync/\")\n"
	       "  -e         : enable error msg\n"
	       "  -w         : enable warning msg\n"
	       "  -i         : enable information msg\n"
	       "  -d         : enable all debug msg\n"
	       "  -t <type>  : handler type\n"
	       "  -D         : disable delta reconstruction\n"
	       "  -F         : enable fake rollback version\n"
#if TMPL_UA_SUPPORT_SCP_TRANSFER
	       "  -H <host>  : host of scp server.\n"
	       "  -U <user>  : scp username\n"
	       "  -P <pass>  : scp password\n"
	       "  -C <path>  : path to cache direcotry used for scp (default: \"/tmp/esync/scp\")\n"
#endif
	       "  -h         : display this help and exit\n"
	       );
	_exit(1);
}


int main(int argc, char** argv)
{
	printf("updateagent %s, xl4bus %s\n", ua_get_updateagent_version(), ua_get_xl4bus_version());

	int c     = 0;
	char *end = NULL;
	ua_cfg_t cfg;
	memset(&cfg, 0, sizeof(ua_cfg_t));

	cfg.debug      = 0;
	cfg.delta      = 1;
	cfg.cert_dir   = "./../pki/certs/updateagent";
	cfg.url        = "tcp://localhost:9133";
	cfg.cache_dir  = "/tmp/esync/";
	cfg.backup_dir = "/data/sota/esync/";

#if TMPL_UA_SUPPORT_SCP_TRANSFER
	scp_info_t scpi;
	memset(&scpi, 0, sizeof(scp_info_t));

	char* op[]  = {"/tmp/esync/scp", "scp", "sshpass", NULL};

	scpi.local_dir   = op[0];
	scpi.scp_bin     = op[1];
	scpi.sshpass_bin = op[2];
#endif

	while ((c = getopt(argc, argv, ":k:u:b:c:a:m:t:H:U:P:C:ewidDFh")) != -1) {
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
			case 'a':
				cfg.delta_config = &(delta_cfg_t){.delta_cap = optarg};
				break;
			case 't':
				uah[0].type_handler = optarg;
				break;
#if TMPL_UA_SUPPORT_SCP_TRANSFER
			case 'H':
				scpi.url = optarg;
				break;
			case 'U':
				scpi.user = optarg;
				break;
			case 'P':
				scpi.password = optarg;
				break;
			case 'C':
				scpi.local_dir = optarg;
				break;
#endif
			case 'e':
				cfg.debug = 1;
				break;
			case 'w':
				cfg.debug = 2;
				break;
			case 'i':
				cfg.debug = 3;
				break;
			case 'd':
				cfg.debug = 4;
				break;
			case 'D':
				cfg.delta = 0;
				break;
			case 'F':
				cfg.enable_fake_rb_ver = 1;
				break;
			case 'm':
				if ((cfg.rw_buffer_size = strtol(optarg, &end, BASE_TEN_CONVERSION)) > 0)
				break;
			case 'h':
			default:
				_help(argv[0]);
				break;
		}
	}

#if TMPL_UA_SUPPORT_SCP_TRANSFER
	scp_init(&scpi);
#endif

	signal(SIGINT, sig_handler);

	ua_debug_lvl = cfg.debug;

	if (ua_init(&cfg)) {
		printf("Updateagent failed!");
		_exit(1);
	}

	int l = sizeof(uah)/sizeof(ua_handler_t);
	ua_register(uah, l);

	while (!stop) {sleep(1); };

	ua_unregister(uah, l);

	ua_stop();

}
