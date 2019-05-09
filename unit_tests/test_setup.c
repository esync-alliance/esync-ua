
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmocka.h"

#include "xl4ua.h"
#include "handler.h"
#include "test_agents/tmpl_updateagent.h"
#include "test_setup.h"
extern char* handler_type;
ua_handler_t uah[] = {
	{UT_DEFAULT_HANDLER_TYPE, get_tmpl_routine }
};

int test_ua_setup(void** state)
{
	ua_cfg_t cfg = {0};

	cfg.debug          = 0;
	cfg.delta          = 1;
	cfg.reboot_support = 1;
	cfg.cert_dir       = "/data/sota/pki/certs/ua";
	cfg.url            = "tcp://localhost:9133";
	cfg.cache_dir      = "/tmp/esync/";
	cfg.backup_dir     = "/data/sota/esync/";
	cfg.delta_config   = &(delta_cfg_t){.delta_cap = "A:3;B:3;C:10"};

	will_return(__wrap_xl4bus_client_init, 0);
	will_return(__wrap_xl4bus_client_stop, 0);

	if (ua_init(&cfg)) {
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

int __wrap_xl4bus_client_init(char* url, char* cert_dir)
{
	mock_type(int);
	return 0;
	//return mock_type(int);
}

int __wrap_xl4bus_client_stop(void)
{
	usleep(500*1000);
	mock_type(int);
	return 0;

}

int __wrap_xl4bus_client_send_msg(const char* message)
{
	return 0;
	return mock_type(int);
}