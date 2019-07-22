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

typedef struct py_ua_cb {
	char* ua_get_version;
	char* ua_set_version;
	char* ua_pre_install;
	char* ua_install;
	char* ua_post_install;
    char* ua_prepare_install;
	char* ua_prepare_download;
}py_ua_cb_t;

int pua_get_version(const char* type, const char* pkgName, char** version);
int pua_set_version(const char* type, const char* pkgName, const char* version);
download_state_t pua_prepare_download(const char* type, const char* pkgName, const char* version);
install_state_t pua_prepare_install(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile);
install_state_t pua_pre_install(const char* type, const char* pkgName, const char* version, const char* pkgFile);
install_state_t pua_install(const char* type, const char* pkgName, const char* version, const char* pkgFile);
void pua_post_install(const char* type, const char* pkgName);
void pua_set_callbacks(PyObject* class_instance, py_ua_cb_t* cb);

int pua_start(char* node_type, ua_cfg_t *cfg);
int pua_send_message(char* message);
void pua_end(void);

#endif /* PUA_H_ */
