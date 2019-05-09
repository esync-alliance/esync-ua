#ifndef TEST_SETUP_H_
#define TEST_SETUP_H_

#define UT_DEFAULT_HANDLER_TYPE "/UNITTEST/UA"
#define UT_MIN_MESSAGE_LEN	8

int test_ua_setup(void** state);
int test_ua_teardown(void** state);
int __wrap_xl4bus_client_init(char* url, char* cert_dir);

#endif //TEST_SETUP_H_