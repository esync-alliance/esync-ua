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

static char pua_version[VERSION_SIZE_MAX] = {0};
static ua_routine_t pua_rtns;

#if PY_MAJOR_VERSION >= 3

#define PyInt_AsLong (int)PyLong_AsLong

char* PyString_AsString(PyObject* string_obj)
{
	char* ret_string = NULL;

	if (PyUnicode_Check(string_obj)) {
		PyObject* temp_bytes = PyUnicode_AsEncodedString(string_obj, "UTF-8", "strict");
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

	printf("version info: libua: %s, libxl4bus: %s\n", ua_get_updateagent_version(), ua_get_xl4bus_version());

	if (ua_init(cfg)) {
		PY_DBG("Updateagent init failed!");
		_exit(1);
	}

	int l = sizeof(uah)/sizeof(ua_handler_t);
	ua_register(uah, l);

	pause();
	return 0;
}

int pua_send_message(char* message, char* addr)
{
	xl4bus_address_t* xl4_address;

	if (addr) {
		xl4bus_chain_address(&xl4_address, XL4BAT_UPDATE_AGENT, addr, 1);
	}else {
		xl4bus_chain_address(&xl4_address, XL4BAT_SPECIAL, XL4BAS_DM_CLIENT);
	}

	return ua_send_message_string_with_address(message, xl4_address);
}

void pua_end(void)
{
	ua_stop();
}

#ifdef LIBUA_VER_2_0

static int pua_get_int_from_pylist_ob(PyObject* ob, int pos)
{
	PyObject* string_ob = NULL;
	int ret             = 0;

	if (PyList_Size(ob) > pos && (string_ob = PyList_GetItem(ob, pos)))
		ret = (int)PyLong_AsLong(string_ob);
	else
		PY_DBG("Could not get integer from PyList object at position %d.", pos);

	return ret;
}

static char* pua_get_string_from_pylist_ob(PyObject* ob, int pos)
{
	PyObject* string_ob = NULL;
	char* ret           = NULL;

	if (PyList_Size(ob) > pos && (string_ob = PyList_GetItem(ob, pos))) {
		ret = PyString_AsString(string_ob);
	}
	else
		PY_DBG("Could not get string from PyList object at position %d.", pos);

	return ret;
}

static install_state_t pua_get_state_from_string(char* s)
{
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

static install_state_t pua_get_py_obj_state(PyObject* py_obj)
{
	return pua_get_state_from_string(PyString_AsString(py_obj));
}

static int pua_get_version(ua_callback_ctl_t* ctl)
{
	PyObject* result = NULL;
	char* tmp_ver    = NULL;
	int rc           = E_UA_ERR;

	memset(pua_version, 0, sizeof(pua_version));
	if ((py_class_instance) && (py_get_version))
	{
		result = PyObject_CallMethod(py_class_instance, py_get_version, "s", ctl->pkg_name);
		if (result) {
			if ((tmp_ver = pua_get_string_from_pylist_ob(result, 1))) {
				PY_DBG("PUA reported version: %s", tmp_ver );
				ctl->version = strncpy(pua_version, tmp_ver, sizeof(pua_version) - 1);
			}

			if ((rc =  pua_get_int_from_pylist_ob(result, 0)))
				PY_DBG("PUA get_version returned error: %d", rc);
		}else
			PY_DBG("Error PyObject_CallFunction py_get_version");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);
	return rc;

}

static int pua_set_version(ua_callback_ctl_t* ctl)
{
	PyObject* result = 0;
	int rc           = E_UA_ERR;

	if ((py_class_instance) && (py_set_version))
	{
		result = PyObject_CallMethod(py_class_instance, py_set_version, "(ss)", ctl->pkg_name, ctl->version);
		if (result) {
			rc = PyInt_AsLong(result);

		}else
			PY_DBG("Error PyObject_CallFunction py_set_version");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);
	return rc;

}

static install_state_t pua_pre_install(ua_callback_ctl_t* ctl)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	if ((py_class_instance) && (py_pre_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_pre_install, "(s)", ctl->pkg_path);
		if (result) {
			state = pua_get_py_obj_state(result);

		}else
			PY_DBG("Error PyObject_CallFunction py_pre_install");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

static install_state_t pua_install(ua_callback_ctl_t* ctl)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	//PY_DBG("pua_install for %s(%s) with file %s", pkgName, version, pkgFile);
	if ((py_class_instance) && (py_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_install, "(ss)", ctl->version, ctl->pkg_path);
		if (result) {
			state = pua_get_py_obj_state(result);

		}else
			PY_DBG("Error PyObject_CallFunction py_install");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	memset(pua_version, 0, sizeof(pua_version));
	return state;

}

static void pua_post_install(ua_callback_ctl_t* ctl)
{
	PyObject* result = 0;

	if ((py_class_instance) && (py_post_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_post_install, "(s)", ctl->pkg_name);
		if (result) {
			PY_DBG("pua_post_install returned OK.");

		}else
			PY_DBG("Error PyObject_CallFunction py_post_install" );
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

}

static install_state_t pua_prepare_install(ua_callback_ctl_t* ctl)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;
	char* state_string;
	char* ret_path;

	if ((py_class_instance) && (py_prepare_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_install, "(sss)", ctl->pkg_name, ctl->version, ctl->pkg_path);
		if (result) {
			if ((state_string = pua_get_string_from_pylist_ob(result, 0))) {
				PY_DBG("PUA prepare_install returned install status: %s", state_string);
				state = pua_get_state_from_string(state_string);
			}

			if (PyList_Size(result) > 1 && (ret_path = pua_get_string_from_pylist_ob(result, 1))) {
				PY_DBG("PUA prepare_install returned new update file path: %s", ret_path);
				ctl->new_file_path = strdup(ret_path);
			}

		}else
			PY_DBG("Error PyObject_CallFunction py_prepare_install");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

static download_state_t pua_prepare_download(ua_callback_ctl_t* ctl)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	if ((py_class_instance) && (py_prepare_download))
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_download, "(ss)", ctl->pkg_name, ctl->version);
		if (result) {
			state = pua_get_py_obj_state(result);
			PY_DBG("pua_prepare_download returning state = %d", state);

		}else
			PY_DBG("Error PyObject_CallFunction py_prepare_download");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

static int pua_dmc_presence(dmc_presence_t* dp)
{
	PyObject* result = 0;
	int rc           = E_UA_ERR;

	if ((py_class_instance) && (py_dmc_presence))
	{
		result = PyObject_CallMethod(py_class_instance, py_dmc_presence, NULL);
		if (result) {
		}else
			PY_DBG("Error PyObject_CallMethodObjArgs py_dmc_presence");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return rc;
}

static int pua_transfer_file(ua_callback_ctl_t* ctl)
{
	PyObject* result = 0;
	char* ret_path;
	int rc = E_UA_ERR;

	if ((py_class_instance) && (py_transfer_file))
	{
		result = PyObject_CallMethod(py_class_instance, py_transfer_file, "(sss)", ctl->pkg_name, ctl->version, ctl->pkg_path);
		if (result) {
			rc = pua_get_int_from_pylist_ob(result, 0);

			if (PyList_Size(result) > 1 && (ret_path = pua_get_string_from_pylist_ob(result, 1))) {
				PY_DBG("PUA do_transfer_file returned new update file path: %s", ret_path);
				ctl->new_file_path = strdup(ret_path);
			}

		}else
			PY_DBG("Error PyObject_CallFunction py_transfer_file");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return rc;
}

static int pua_on_message(const char* msgType,  const char* message)
{
	return E_UA_OK;
}

#else
static int pua_get_int_from_pylist_ob(PyObject* ob, int pos)
{
	PyObject* string_ob = NULL;
	int ret             = 0;

	if (PyList_Size(ob) > pos && (string_ob = PyList_GetItem(ob, pos)))
		ret = (int)PyLong_AsLong(string_ob);
	else
		PY_DBG("Could not get integer from PyList object at position %d.", pos);

	return ret;
}

static char* pua_get_string_from_pylist_ob(PyObject* ob, int pos)
{
	PyObject* string_ob = NULL;
	char* ret           = NULL;

	if (PyList_Size(ob) > pos && (string_ob = PyList_GetItem(ob, pos)))
		ret = PyString_AsString(string_ob);
	else
		PY_DBG("Could not get string from PyList object at position %d.", pos);

	return ret;
}

static install_state_t pua_get_state_from_string(char* s)
{
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

static install_state_t pua_get_py_obj_state(PyObject* py_obj)
{
	return pua_get_state_from_string(PyString_AsString(py_obj));
}

static int pua_get_version(const char* type, const char* pkgName, char** version)
{
	PyObject* result = NULL;
	char* tmp_ver    = NULL;
	int rc           = E_UA_ERR;

	memset(pua_version, 0, sizeof(pua_version));
	if ((py_class_instance) && (py_get_version))
	{
		result = PyObject_CallMethod(py_class_instance, py_get_version, "s", pkgName);
		if (result) {
			if ((tmp_ver = pua_get_string_from_pylist_ob(result, 1))) {
				PY_DBG("PUA reported version: %s", tmp_ver );
				*version = strncpy(pua_version, tmp_ver, sizeof(pua_version) - 1);
			}

			if ((rc =  pua_get_int_from_pylist_ob(result, 0)))
				PY_DBG("PUA get_version returned error: %d", rc);
		}else
			PY_DBG("Error PyObject_CallFunction py_get_version");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);
	return rc;

}

static int pua_set_version(const char* type, const char* pkgName, const char* version)
{
	PyObject* result = 0;
	int rc           = E_UA_ERR;

	if ((py_class_instance) && (py_set_version))
	{
		result = PyObject_CallMethod(py_class_instance, py_set_version, "(ss)", pkgName, version);
		if (result) {
			rc = PyInt_AsLong(result);

		}else
			PY_DBG("Error PyObject_CallFunction py_set_version");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);
	return rc;

}

static install_state_t pua_pre_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	if ((py_class_instance) && (py_pre_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_pre_install, "(s)", pkgFile);
		if (result) {
			state = pua_get_py_obj_state(result);

		}else
			PY_DBG("Error PyObject_CallFunction py_pre_install");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

static install_state_t pua_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	//PY_DBG("pua_install for %s(%s) with file %s", pkgName, version, pkgFile);
	if ((py_class_instance) && (py_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_install, "(ss)", version, pkgFile);
		if (result) {
			state = pua_get_py_obj_state(result);

		}else
			PY_DBG("Error PyObject_CallFunction py_install");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	memset(pua_version, 0, sizeof(pua_version));
	return state;

}

static void pua_post_install(const char* type, const char* pkgName)
{
	PyObject* result = 0;

	if ((py_class_instance) && (py_post_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_post_install, "(s)", pkgName);
		if (result) {
			PY_DBG("pua_post_install returned OK.");

		}else
			PY_DBG("Error PyObject_CallFunction py_post_install" );
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

}

static install_state_t pua_prepare_install(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;
	char* state_string;
	char* ret_path;

	if ((py_class_instance) && (py_prepare_install))
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_install, "(sss)", pkgName, version, pkgFile);
		if (result) {
			if ((state_string = pua_get_string_from_pylist_ob(result, 0))) {
				PY_DBG("PUA prepare_install returned install status: %s", state_string);
				state = pua_get_state_from_string(state_string);
			}

			if (PyList_Size(result) > 1 && (ret_path = pua_get_string_from_pylist_ob(result, 1))) {
				PY_DBG("PUA prepare_install returned new update file path: %s", ret_path);
				*newFile = ret_path;
			}

		}else
			PY_DBG("Error PyObject_CallFunction py_prepare_install");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

static download_state_t pua_prepare_download(const char* type, const char* pkgName, const char* version)
{
	PyObject* result      = 0;
	install_state_t state = INSTALL_FAILED;

	if ((py_class_instance) && (py_prepare_download))
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_download, "(ss)", pkgName, version);
		if (result) {
			state = pua_get_py_obj_state(result);
			PY_DBG("pua_prepare_download returning state = %d", state);

		}else
			PY_DBG("Error PyObject_CallFunction py_prepare_download");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return state;

}

static int pua_dmc_presence(dmc_presence_t* dp)
{
	PyObject* result = 0;
	int rc           = E_UA_ERR;

	if ((py_class_instance) && (py_dmc_presence))
	{
		result = PyObject_CallMethod(py_class_instance, py_dmc_presence, NULL);
		if (result) {
		}else
			PY_DBG("Error PyObject_CallMethodObjArgs py_dmc_presence");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return rc;
}

static int pua_transfer_file(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	PyObject* result = 0;
	char* ret_path;
	int rc = E_UA_ERR;

	if ((py_class_instance) && (py_transfer_file))
	{
		result = PyObject_CallMethod(py_class_instance, py_transfer_file, "(sss)", pkgName, version, pkgFile);
		if (result) {
			rc = pua_get_int_from_pylist_ob(result, 0);

			if (PyList_Size(result) > 1 && (ret_path = pua_get_string_from_pylist_ob(result, 1)))
				PY_DBG("PUA do_transfer_file returned new update file path: %s", ret_path);

		}else
			PY_DBG("Error PyObject_CallFunction py_transfer_file");
	}else
		PY_DBG("Invalid class instance!");

	Py_XDECREF(result);

	return rc;
}

static int pua_on_message(const char* msgType, void* message)
{
	return E_UA_OK;
}

#endif

void pua_set_callbacks(PyObject* class_instance, py_ua_cb_t* cb)
{
	Py_XDECREF(py_class_instance);
	Py_XINCREF(class_instance);
	py_class_instance = class_instance;

	if (cb->ua_get_version) {
		py_get_version          = cb->ua_get_version;
		pua_rtns.on_get_version = pua_get_version;
	}
	if (cb->ua_set_version) {
		py_set_version          = cb->ua_set_version;
		pua_rtns.on_set_version = pua_set_version;
	}
	if (cb->ua_prepare_download) {
		py_prepare_download          = cb->ua_prepare_download;
		pua_rtns.on_prepare_download = pua_prepare_download;
	}
	if (cb->ua_pre_install) {
		py_pre_install          = cb->ua_pre_install;
		pua_rtns.on_pre_install = pua_pre_install;
	}
	if (cb->ua_install) {
		py_install          = cb->ua_install;
		pua_rtns.on_install = pua_install;
	}
	if (cb->ua_post_install) {
		py_post_install          = cb->ua_post_install;
		pua_rtns.on_post_install = pua_post_install;
	}
	if (cb->ua_prepare_install) {
		py_prepare_install          = cb->ua_prepare_install;
		pua_rtns.on_prepare_install = pua_prepare_install;
	}
	if (cb->ua_transfer_file) {
		py_transfer_file          = cb->ua_transfer_file;
		pua_rtns.on_transfer_file = pua_transfer_file;
	}
	if (cb->ua_dmc_presence) {
		py_dmc_presence          = cb->ua_dmc_presence;
		pua_rtns.on_dmc_presence = pua_dmc_presence;
	}
	if (cb->ua_post_install) {
		py_on_message       = cb->ua_on_message;
		pua_rtns.on_message = pua_on_message;
	}

}
