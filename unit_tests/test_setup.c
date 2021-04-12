
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmocka.h"

#ifdef LIBUA_VER_2_0
#include "esyncua.h"
#else
#include "xl4ua.h"
#endif //LIBUA_VER_2_0

#include "handler.h"
#include "test_agents/tmpl_updateagent.h"
#include "test_setup.h"

extern char* handler_type;
ua_handler_t uah[] = {
	{UT_DEFAULT_HANDLER_TYPE, get_tmpl_routine }
};

int test_ua_setup(void** state)
{
	ua_cfg_t* cfg    = (ua_cfg_t*)*state;
	ua_cfg_t dfl_cfg = {0};

	if (!cfg) {
		dfl_cfg.debug          = 4;
		dfl_cfg.delta          = 1;
		dfl_cfg.reboot_support = 1;
		dfl_cfg.cert_dir       = "/data/sota/pki/certs/ua";
		dfl_cfg.url            = "tcp://localhost:9133";
		dfl_cfg.cache_dir      = "/tmp/esync/";
		dfl_cfg.backup_dir     = "/data/sota/esync/";
		dfl_cfg.delta_config   = &(delta_cfg_t){.delta_cap = "A:3;B:3;C:10"};

		cfg = &dfl_cfg;
	}


	if (ua_init(cfg)) {
		printf("Updateagent failed!");
		_exit(1);
	}
	if (handler_type)
		uah[0].type_handler = handler_type;
	ua_register(uah, sizeof(uah)/sizeof(uah[0]));

	return 0;
}

int test_ua_teardown(void** state)
{
	ua_unregister(uah, sizeof(uah)/sizeof(uah[0]));
	ua_stop();
	return 0;
}

int __wrap_xl4bus_client_init(char* url, char* cert_dir, char* pwd)
{
	return 0;
}

int __wrap_xl4bus_client_stop(void)
{
	usleep(500*1000);
	return 0;
}

int __wrap_xl4bus_client_send_msg(const char* message)
{
	return 0;
}

int __wrap_calc_sha256_x(const char* archive, char obuff[SHA256_B64_LENGTH])
{
	return E_UA_OK;
}

int __wrap_verify_file_hash_b64(const char* file, const char* sha256_b64)
{
	return E_UA_OK;
}


int __wrap_reply_id_matched(char* s1, char* s2)
{
	return 1;
}