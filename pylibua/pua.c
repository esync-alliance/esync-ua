/*
 * pua.c
 *
 * Created on: Jan 10, 2019
 *
 */

#include "pua.h"

static PyObject* py_class_instance = NULL;
static char* py_get_version        = NULL;
static char* py_set_version        = NULL;
static char* py_pre_install        = NULL;
static char* py_install            = NULL;
static char* py_post_install       = NULL;
static char* py_prepare_install    = NULL;
static char* py_prepare_download   = NULL;
static char* py_transfer_file      = NULL;
static char* py_dmc_presence       = NULL;
static char* py_on_message         = NULL;

ua_routine_t pua_rtns = {
	.on_get_version      = pua_get_version,
	.on_set_version      = pua_set_version,
	.on_pre_install      = pua_pre_install,
	.on_install          = pua_install,
	.on_post_install     = pua_post_install,
	.on_prepare_install  = pua_prepare_install,
	.on_prepare_download = pua_prepare_download,
	//TODO: Connect the following callbacks with Python
	.on_transfer_file    = NULL,
	.on_dmc_presence     = NULL,
	.on_message          = NULL

	/*
	.on_transfer_file    = pua_transfer_file,
	.on_dmc_presence     = pua_dmc_presence,
	.on_message          = pua_on_message
	*/
};

#if PY_MAJOR_VERSION >= 3

#define PyInt_AsLong      (int)PyLong_AsLong

char* PyString_AsString(PyObject *string_obj)
{
	char* ret_string = NULL;

	if (PyUnicode_Check(string_obj)) {
		PyObject * temp_bytes = PyUnicode_AsEncodedString(string_obj, "UTF-8", "strict");
		if (temp_bytes != NULL) {
			ret_string = PyBytes_AS_STRING(temp_bytes);
			Py_DECREF(temp_bytes);
		}
	}
	return ret_string;
}

#endif

ua_routine_t* get_pua_routine(void)
{
	return &pua_rtns;

}

int pua_start(char* node_type, ua_cfg_t* cfg)
{
	ua_handler_t uah[] = {
		{node_type, get_pua_routine }
	};

	if (ua_init(cfg)) {
		PY_DBG("Updateagent init failed!");
		_exit(1);
	}

	int l = sizeof(uah)/sizeof(ua_handler_t);
	ua_register(uah, l);

	while (1) { pause(); }
	return 0;
}

int pua_send_message(char* message)
{
	return 0;
}

void pua_end(void)
{
	ua_stop();
}

void pua_set_callbacks(PyObject* class_instance, py_ua_cb_t* cb)
{
	Py_XDECREF(py_class_instance);
	Py_XINCREF(class_instance);
	py_class_instance = class_instance;

	py_get_version      = cb->ua_get_version;
	py_set_version      = cb->ua_set_version;
	py_pre_install      = cb->ua_pre_install;
	py_install          = cb->ua_install;
	py_post_install     = cb->ua_post_install;
	py_prepare_install  = cb->ua_prepare_install;
	py_prepare_download = cb->ua_prepare_download;
	py_transfer_file    = cb->ua_transfer_file;
	py_dmc_presence     = cb->ua_dmc_presence;
	py_on_message       = cb->ua_on_message;

}

static install_state_t pua_get_py_obj_state(PyObject* py_obj)
{
	char* s               = PyString_AsString(py_obj);
	install_state_t state = INSTALL_FAILED;
	if (s) {
		if (!strcmp(s, "INSTALL_PENDING") | !strcmp(s, "DOWNLOAD_CONSENT"))
			state = DOWNLOAD_CONSENT;
		else if (!strcmp(s, "INSTALL_COMPLETED"))
			state = INSTALL_COMPLETED;
		else if (!strcmp(s, "INSTALL_ABORTED"))
			state = INSTALL_ABORTED;
		else if (!strcmp(s, "INSTALL_ROLLBACK"))
			state = INSTALL_ROLLBACK;
		else if (!strcmp(s, "INSTALL_FAILED"))
			state = INSTALL_FAILED;
		else if (!strcmp(s, "INSTALL_IN_PROGRESS"))
			state = INSTALL_IN_PROGRESS;
		else if (!strcmp(s, "INSTALL_READY"))
			state = INSTALL_READY;

	}

	return state;

}

int pua_get_version(const char* type, const char* pkgName, char** version)
{
	PyObject* result = 0;
	int rc           = E_UA_ERR;

	if ((py_class_instance) && py_get_version)
	{
		result = PyObject_CallMethod(py_class_instance, py_get_version, "(s)", pkgName);
		if (result) {
			*version = PyString_AsString(result);
			PY_DBG("pua reported version: %s", *version );
			rc = E_UA_OK;
		}else
			PY_DBG("Error PyObject_CallMethod %s", py_get_version);
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);
	return rc;

}

int pua_set_version(const char* type, const char* pkgName, const char* version)
{
	PyObject* result = 0;
	int rc           = E_UA_ERR;

	if ((py_class_instance) && py_set_version)
	{
		result = PyObject_CallMethod(py_class_instance, py_set_version, "(ss)", pkgName, version);
		if (result) {
			rc = PyInt_AsLong(result);

		}else
			PY_DBG("Error PyObject_CallMethod, %s", py_set_version);
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);
	return rc;

}

install_state_t pua_pre_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	if ((py_class_instance) && py_pre_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_pre_install, "(s)", pkgFile);
		if (result) {
			state = pua_get_py_obj_state(result);

		}else
			PY_DBG("Error PyObject_CallMethod %s", py_pre_install);
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

install_state_t pua_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;
	PY_DBG("pua_install(%s), %s\n", pkgName, py_install);
	if ((py_class_instance) && py_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_install, "(s)",pkgFile);
		if (result) {
			state = pua_get_py_obj_state(result);

		}else
			PY_DBG("Error PyObject_CallMethod %s", py_install);
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

void pua_post_install(const char* type, const char* pkgName)
{
	PyObject* result = 0;

	if ((py_class_instance) && py_post_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_post_install, "(s)", pkgName);
		if (result) {
			PY_DBG("pua_prepare_install returning result = %d", result);

		}else
			PY_DBG("Error PyObject_CallMethod %s", py_post_install);
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

}

install_state_t pua_prepare_install(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	if ((py_class_instance) && py_prepare_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_install, "(ssss)", pkgName, version, pkgFile, newFile);
		if (result) {
			state = pua_get_py_obj_state(result);
			PY_DBG("pua_prepare_install returning state = %d", state);

		}else
			PY_DBG("Error PyObject_CallMethod, %s", py_prepare_install);
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

download_state_t pua_prepare_download(const char* type, const char* pkgName, const char* version)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	if ((py_class_instance) && py_prepare_download)
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_download, "(ss)", pkgName, version);
		if (result) {
			state = pua_get_py_obj_state(result);
			PY_DBG("pua_prepare_download returning state = %d", state);

		}else
			PY_DBG("Error PyObject_CallMethod %s", py_prepare_download);
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

int pua_dmc_presence(dmc_presence_t* dp)
{
	return E_UA_OK;
}

int pua_transfer_file(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	return E_UA_OK;
}

int pua_on_message(const char* msgType, void* message)
{
	return E_UA_OK;
}