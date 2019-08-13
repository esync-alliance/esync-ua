/*
 * pua.h
 *
 * Created on: Jan 10, 2019
 *
 */

#ifndef PUA_H_
#define PUA_H_

#include <Python.h>
#include <xl4ua.h>
#include <unistd.h>


#define _ltime_ \
        char __now[24]; \
        struct tm __tmnow; \
        struct timeval __tv; \
        memset(__now, 0, 24); \
        gettimeofday(&__tv, 0); \
        localtime_r(&__tv.tv_sec, &__tmnow); \
        strftime(__now, 20, "%m-%d:%H:%M:%S.", &__tmnow); \
        sprintf(__now+15, "%03d", (int)(__tv.tv_usec/1000))

#define PY_DBG(a,b ...)     do { { \
				      _ltime_; \
				      printf("[%s] %s:%d " a, __now, chop_path(__FILE__), __LINE__, ## b); printf("\n");\
			      } } while (0)

typedef struct py_ua_cb {
	char* ua_get_version;
	char* ua_set_version;
	char* ua_pre_install;
	char* ua_install;
	char* ua_post_install;
	char* ua_prepare_install;
	char* ua_prepare_download;
	char* ua_transfer_file;
	char* ua_dmc_presence;
	char* ua_on_message;
}py_ua_cb_t;

int pua_get_version(const char* type, const char* pkgName, char** version);
int pua_set_version(const char* type, const char* pkgName, const char* version);
download_state_t pua_prepare_download(const char* type, const char* pkgName, const char* version);
install_state_t pua_prepare_install(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile);
install_state_t pua_pre_install(const char* type, const char* pkgName, const char* version, const char* pkgFile);
install_state_t pua_install(const char* type, const char* pkgName, const char* version, const char* pkgFile);
void pua_post_install(const char* type, const char* pkgName);
int pua_dmc_presence(dmc_presence_t* dp);
int pua_transfer_file(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile);
int pua_on_message(const char* msgType, void* message);
void pua_set_callbacks(PyObject* class_instance, py_ua_cb_t* cb);

int pua_start(char* node_type, ua_cfg_t* cfg);
int pua_send_message(char* message);
void pua_end(void);

static inline const char* chop_path(const char* path)
{
	const char* aux = strrchr(path, '/');

	if (aux) { return aux + 1; }
	return path;
}

#endif /* PUA_H_ */
