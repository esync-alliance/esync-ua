#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmocka.h"

#include <linux/limits.h>
#include "handler.h"
#include "test_setup.h"
#include "test_agents/tmpl_updateagent.h"

extern char* optarg;
char* handler_type;
static char case_dir[PATH_MAX];

static void test_from_file(char* filename)
{
	char bus_message[2048];
	FILE* fd = 0;
	char fullpath[PATH_MAX];

	sprintf(fullpath, "%s/%s", case_dir, filename);
	//will_return(__wrap_get_tmpl_version, cast_ptr_to_largest_integral_type("v1.0"));
	//will_return(__wrap_get_tmpl_version, 0);
	test_ua_setup((void**)&handler_type);

	if ((fd = fopen(fullpath, "r"))) {
		while (fgets(bus_message, sizeof(bus_message), fd))
			if (strlen(bus_message) > UT_MIN_MESSAGE_LEN) {
				handle_message(handler_type, bus_message, strlen(bus_message));
				usleep(100*1000);

			}
		fclose(fd);
	}

	test_ua_teardown((void**)&filename);
}

void test_normal_update(void** state)
{
	set_test_installation_mode(UPDATE_MODE_SUCCESSFUL, 0);
	test_from_file("ut_normal_update");
}

void test_rollback_by_dmc(void** state)
{
	test_from_file("ut_rollback_by_dmc");
}

void test_rollback_ua_controlled_success(void** state)
{
	set_test_installation_mode(UPDATE_MODE_ALTERNATE_FAILURE_SUCCESSFUL, 0);
	test_from_file("ut_rollback_ua_controlled");
}

void test_rollback_ua_controlled_failed(void** state)
{
	set_test_installation_mode(UPDATE_MODE_FAILURE, 0);
	test_from_file("ut_rollback_ua_controlled");
}

void test_resume_normal_update(void** state)
{
	test_from_file("ut_resume_normal_update");
}

void test_resume_rollback_update(void** state)
{
	test_from_file("ut_resume_rollback_update");
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
				strncpy(case_dir, optarg, sizeof(case_dir));
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
		cmocka_unit_test(test_normal_update),
		//cmocka_unit_test(test_rollback_by_dmc),
		//cmocka_unit_test(test_rollback_ua_controlled_success),
		//cmocka_unit_test(test_rollback_ua_controlled_failed),
		//cmocka_unit_test(test_resume_normal_update),
		//cmocka_unit_test(test_resume_rollback_update),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
