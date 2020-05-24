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

extern char* optarg;
char* handler_type;
static char case_dir[PATH_MAX];

static void handle_messages_from_file(char* filename)
{
	char bus_message[2048];
	FILE* fd = 0;
	char fullpath[PATH_MAX];

	ua_cfg_t* cfg = NULL;

	sprintf(fullpath, "%s/%s", case_dir, filename);
	test_ua_setup((void**)&cfg);

	if ((fd = fopen(fullpath, "r"))) {
		while (fgets(bus_message, sizeof(bus_message), fd))
			if (strlen(bus_message) > UT_MIN_MESSAGE_LEN) {
				handle_message(handler_type, bus_message, strlen(bus_message));
				usleep(200*1000);

			}
		fclose(fd);
	}

	test_ua_teardown((void**)&filename);
}

void test_normal_update_success(void** state)
{
	set_test_installation_mode(UPDATE_MODE_SUCCESSFUL, 0);
	assert_true(!system("rm -rf /data/sota/esync/backup"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM_4.0/4.0/ECU_HMNS_ROM-4.0.x /data/sota/tmp/"));

	handle_messages_from_file("ut_normal_update");

	assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/4.0/ECU_HMNS_ROM-4.0.x", F_OK));
	assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
	assert_true(access("/tmp/esync/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
}

void test_normal_update_failure(void** state)
{
	set_test_installation_mode(UPDATE_MODE_FAILURE, 0);
	assert_true(!system("rm -rf /data/sota/esync/backup"));
	assert_true(!system("cp -f ../unit_tests/fixure/backup/ECU_HMNS_ROM_4.0/4.0/ECU_HMNS_ROM-4.0.x /data/sota/tmp/"));

	handle_messages_from_file("ut_normal_update_failed");

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
		handle_messages_from_file("ut_rollback_by_dmc_failed");
		assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/1.0", F_OK));
		assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/4.0", F_OK));

	}else {
		handle_messages_from_file("ut_rollback_by_dmc");
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
	handle_messages_from_file("ut_rollback_mixed");
	assert_true(access("/data/sota/esync/backup/ECU_HMNS_ROM/1.0", F_OK));

	assert_true(!access("/data/sota/esync/backup/ECU_HMNS_ROM/4.0", F_OK));

	assert_true(access("/tmp/esync/ECU_HMNS_ROM/manifest_pkg.xml", F_OK));
}

static void _help(const char* app)
{
	printf("Usage: %s [OPTION...]\n\n%s", app,
	       "Options:\n"
	       "  -t <type>  : handler type (default: \"/UNITTEST/UA/\")\n"
	       "  -c <path>  : path to test cases directory (default \"../unit_tests/cases/\")\n"
	       "  -h         : display this help and exit\n"
	       );
	_exit(1);
}

int main(int argc, char** argv)
{
	int c = 0;

	handler_type = UT_DEFAULT_HANDLER_TYPE;
	strcpy(case_dir, "../unit_tests/cases");
	while ((c = getopt(argc, argv, ":c:t:h")) != -1) {
		switch (c) {
			case 'c':
				memset(case_dir, 0, sizeof(case_dir));
				strncpy(case_dir, optarg, sizeof(case_dir) - 1);
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

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_normal_update_success),
		cmocka_unit_test(test_normal_update_failure),
		cmocka_unit_test(test_delta_dmc_rollback_success),
		cmocka_unit_test(test_delta_dmc_rollback_failure),
		cmocka_unit_test(test_rollback_mixed),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
