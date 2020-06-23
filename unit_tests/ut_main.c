#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmocka.h"

#include <linux/limits.h>
#include "handler.h"
#include "test_setup.h"
#include "ut_updateagent.h"

typedef void (*ut_test_func)(void** state);

extern char* optarg;
char* handler_type;

static char* xl4_msg_path = 0;
static char case_dir[PATH_MAX];
static char *cases[] = {
	"test_normal_update_success",
	"test_normal_update_failure",
	"test_delta_dmc_rollback_success",
	"test_delta_dmc_rollback_failure",
	"test_rollback_mixed",
	"test_rollback_fake_version",
};

static void handle_messages_from_file(char* filename, ua_cfg_t* cfg)
{
	char bus_message[2048];
	FILE* fd = 0;
	char fullpath[PATH_MAX];

	//ua_cfg_t* cfg = NULL;

	sprintf(fullpath, "%s%s", case_dir, filename);
	test_ua_setup((void**)&cfg);

	if ((fd = fopen(fullpath, "r"))) {
		while (fgets(bus_message, sizeof(bus_message), fd))
			if (strlen(bus_message) > UT_MIN_MESSAGE_LEN) {
				handle_message(handler_type, bus_message, strlen(bus_message));
				usleep(200*1000);

			}
		fclose(fd);
	}
	//while(1);
	test_ua_teardown((void**)&filename);
}

void test_normal_update_success(void** state)
{
	set_test_installation_mode(UPDATE_MODE_SUCCESSFUL, 0);
	assert_true(!system("rm -rf /data/sota/esync/backup"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM_4.0/4.0/ECU_HMNS_ROM-4.0.x /data/sota/tmp/"));

	handle_messages_from_file("ut_normal_update", NULL);

	assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/4.0/ECU_HMNS_ROM-4.0.x", F_OK));
	assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
	assert_true(access("/tmp/esync/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
}

void test_normal_update_failure(void** state)
{
	set_test_installation_mode(UPDATE_MODE_FAILURE, 0);
	assert_true(!system("rm -rf /data/sota/esync/backup"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM_4.0/4.0/ECU_HMNS_ROM-4.0.x /data/sota/tmp/"));

	handle_messages_from_file("ut_normal_update_failed", NULL);

	assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/4.0/ECU_HMNS_ROM-4.0.x", F_OK));
	assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
	assert_true(access("/tmp/esync/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));

}

void test_rollback_by_dmc(int test_failed)
{
	assert_true(!system("rm -rf /data/sota/esync/backup"));
	assert_true(!system("mkdir -p /data/sota/esync/backup/ECU_HMNS_ROM"));
	assert_true(!system("cp -rf ../unit_tests/fixure/backup/ECU_HMNS_ROM_4.0/* /data/sota/esync/backup/ECU_HMNS_ROM/"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM_1.0/1.0/ECU_HMNS_ROM-1.0.x /data/sota/tmp/ECU_HMNS_ROM-1.0.x"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM-2.0.x /data/sota/tmp/ECU_HMNS_ROM-2.0.x"));
	assert_true(!system("cp -f ../unit_tests/fixure/delta/4.0_3.0/ECU_HMNS_ROM-3.0.x /data/sota/tmp/ECU_HMNS_ROM-3.0.x"));
	set_test_installation_mode(UPDATE_MODE_ROLLBACK, 0);
	if (test_failed) {
		set_rbconf_file("../unit_tests/fixure/rollback/rbFailed");
		handle_messages_from_file("ut_rollback_by_dmc_failed", NULL);
		assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/1.0", F_OK));
		assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/4.0", F_OK));

	}else {
		handle_messages_from_file("ut_rollback_by_dmc", NULL);
		assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/1.0/ECU_HMNS_ROM-1.0.x", F_OK));
		assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));

	}

	assert_true(access("/tmp/esync/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
}

void test_delta_dmc_rollback_success(void** state)
{
	test_rollback_by_dmc(0);

}

void test_delta_dmc_rollback_failure(void** state)
{
	test_rollback_by_dmc(1);
}

void test_rollback_mixed(void** state)
{
	assert_true(!system("rm -rf /data/sota/esync/backup"));
	assert_true(!system("mkdir -p /data/sota/esync/backup/ECU_HMNS_ROM"));
	assert_true(!system("cp -rf ../unit_tests/fixure/backup/ECU_HMNS_ROM_4.0/* /data/sota/esync/backup/ECU_HMNS_ROM/"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM_1.0/1.0/ECU_HMNS_ROM-1.0.x /data/sota/tmp/ECU_HMNS_ROM-1.0.x"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM-2.0.x /data/sota/tmp/ECU_HMNS_ROM-2.0.x"));
	assert_true(!system("cp -f ../unit_tests/fixure/delta/4.0_3.0/ECU_HMNS_ROM-3.0.x /data/sota/tmp/ECU_HMNS_ROM-3.0.x"));

	set_test_installation_mode(UPDATE_MODE_ROLLBACK, 0);
	set_rbconf_file("../unit_tests/fixure/rollback/rbFailed");
	handle_messages_from_file("ut_rollback_mixed", NULL);

	assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/1.0", F_OK));
	assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/4.0", F_OK));
	assert_true(access("/tmp/esync/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
}


void test_rollback_fake_version(void** state)
{
	assert_true(!system("rm -rf /data/sota/esync/backup"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM-2.0.x /data/sota/tmp/ECU_HMNS_ROM-2.0.x"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM_3.0/3.0/ECU_HMNS_ROM-3.0.x /data/sota/tmp/ECU_HMNS_ROM-3.0.x"));

	ua_cfg_t cfg = {0};

	cfg.debug          = 4;
	cfg.delta          = 1;
	cfg.reboot_support = 1;
	cfg.cert_dir       = "/data/sota/pki/certs/ua";
	cfg.url            = "tcp://localhost:9133";
	cfg.cache_dir      = "/tmp/esync/";
	cfg.backup_dir     = "/data/sota/esync/";
	cfg.delta_config   = &(delta_cfg_t){.delta_cap = "A:4"};
	
	set_test_installation_mode(UPDATE_MODE_ROLLBACK, 0);
	handle_messages_from_file("ut_rollback_fake_version", &cfg);

	assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/1.0", F_OK));
	assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/3.0", F_OK));
	assert_true(access("/tmp/esync/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
}

void test_xl4_msg_file(void** state)
{

	handle_messages_from_file(xl4_msg_path, NULL);

}

static ut_test_func case_funcs[] = {
	test_normal_update_success,
	test_normal_update_failure,
	test_delta_dmc_rollback_success,
	test_delta_dmc_rollback_failure,
	test_rollback_mixed,
	test_rollback_fake_version,

};

static void _help(const char* app)
{
	printf("Usage: %s [OPTION...]\n\n%s", app,
	       "Options:\n"
	       "  -h         : display this help and exit\n"
	       "  -t <type>  : handler type (default: \"/UNITTEST/UA/\")\n"
	       "  -f <file>  : only run test with <file>, containing xl4bus messages\n"
	       "  -c <path>  : path to test cases directory (default \"../unit_tests/cases/\")\n"
	       "  -i index   : run single ith test case, default(0) is to run all tests> \n"
	       );
	for (int i = 0; i < sizeof(cases)/sizeof(cases[0]);  i++) {
		printf("  	       %d =  %s \n", i+1, cases[i]);
	}
	_exit(1);
}

int main(int argc, char** argv)
{
	int c = 0;
	int sel = -1;

	handler_type = UT_DEFAULT_HANDLER_TYPE;
	strcpy(case_dir, "../unit_tests/cases/");

	while ((c = getopt(argc, argv, ":c:i:t:f:h")) != -1) {
		switch (c) {
			case 'c':
				memset(case_dir, 0, sizeof(case_dir));
				strncpy(case_dir, optarg, sizeof(case_dir) - 1);
				break;
			case 'i':
				sel = atoi(optarg) - 1;
				break;
			case 'f':
				xl4_msg_path = optarg;
				break;
			case 't':
				handler_type = optarg;
				break;
			case 'h':
			default:
				_help(argv[0]);
				break;
		}
	}

	if(xl4_msg_path) {
		case_dir[0] = 0;
		const struct CMUnitTest signle[] = {
			cmocka_unit_test(test_xl4_msg_file),
		};
		return cmocka_run_group_tests(signle, NULL, NULL);		
	}

	if(sel >= 0) {
		const struct CMUnitTest signle[] = {
			cmocka_unit_test(case_funcs[sel]),
		};
		return cmocka_run_group_tests(signle, NULL, NULL);
	} else {

		const struct CMUnitTest tests[] = {
			cmocka_unit_test(test_normal_update_success),
			cmocka_unit_test(test_normal_update_failure),
			cmocka_unit_test(test_delta_dmc_rollback_success),
			cmocka_unit_test(test_delta_dmc_rollback_failure),
			cmocka_unit_test(test_rollback_mixed),
			cmocka_unit_test(test_rollback_fake_version),
		};

		return cmocka_run_group_tests(tests, NULL, NULL);		
	}

}
