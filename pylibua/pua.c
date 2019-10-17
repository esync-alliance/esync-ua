/*
 * pua.c
 *
 * Created on: Jan 10, 2019
 *
 */

#include "pua.h"

static PyObject * py_class_instance = NULL;
static char * py_get_version = NULL;
static char * py_set_version = NULL;
static char * py_pre_install = NULL;
static char * py_install = NULL;
static char * py_post_install = NULL;
static char * py_prepare_install = NULL;
static char * py_prepare_download = NULL;

ua_routine_t pua_rtns = {
	.on_get_version      = pua_get_version,
	.on_set_version      = pua_set_version,
	.on_pre_install      = pua_pre_install,
	.on_install          = pua_install,
	.on_post_install     = pua_post_install,
	.on_prepare_install  = pua_prepare_install,
	.on_prepare_download = pua_prepare_download
};

ua_routine_t* get_pua_routine(void)
{
	return &pua_rtns;

}

int pua_start(char* node_type, ua_cfg_t *cfg)
{
	ua_handler_t uah[] = {
		{node_type, get_pua_routine }
	};

	if (ua_init(cfg)) {
		printf("Updateagent init failed!");
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

    py_get_version = cb->ua_get_version;
    py_set_version = cb->ua_set_version;
    py_pre_install = cb->ua_pre_install;
    py_install = cb->ua_install;
    py_post_install = cb->ua_post_install;
    py_prepare_install = cb->ua_prepare_install;
    py_prepare_download = cb->ua_prepare_download;

}

static install_state_t pua_get_py_obj_state(PyObject *py_obj)
{
    char * s = PyString_AsString(py_obj);
    install_state_t state = INSTALL_FAILED;
    if(s) {
        if(!strcmp("s", "INSTALL_PENDING") | !strcmp("s", "DOWNLOAD_CONSENT")) 
            state = DOWNLOAD_CONSENT;
        else if(!strcmp("s", "INSTALL_COMPLETED")) 
            state = INSTALL_COMPLETED;
        else if(!strcmp("s", "INSTALL_ABORTED")) 
            state = INSTALL_ABORTED;
        else if(!strcmp("s", "INSTALL_ROLLBACK")) 
            state = INSTALL_ROLLBACK;
        else if(!strcmp("s", "INSTALL_FAILED")) 
            state = INSTALL_FAILED;
        else if(!strcmp("s", "INSTALL_IN_PROGRESS")) 
            state = INSTALL_IN_PROGRESS;
        
    }

    return state;

}

int pua_get_version(const char * type, const char * pkgName, char ** version)
{

    PyObject *result = 0;
    int rc = E_UA_ERR;

    printf("pua_get_version, %s\n", pkgName);

    if(PyInstance_Check(py_class_instance) && py_get_version)
	{
		result = PyObject_CallMethod(py_class_instance, py_get_version, "(s)", pkgName);
		if(result){
            *version = PyString_AsString(result);
            printf("return result: %s \n", *version );
            rc = PyInt_AsLong(result);
        }else
			printf("Error PyObject_CallMethod\n");    
	}else
	{
		printf("Not an instance object!\n");
	}	

    Py_XDECREF(result);
    return rc;

}

int pua_set_version(const char * type, const char * pkgName, const char * version)
{

    PyObject *result = 0;
    int rc = E_UA_ERR;

    if(PyInstance_Check(py_class_instance) && py_set_version)
	{
		result = PyObject_CallMethod(py_class_instance, py_set_version, "(ss)", pkgName, version);
		if(result){
            rc = PyInt_AsLong(result);

        }else
			printf("Error PyObject_CallMethod\n");    
	}else
	{
		printf("Not an instance object!\n");
	}	
    
    Py_XDECREF(result);
    return rc;

}

install_state_t pua_pre_install(const char * type, const char * pkgName, const char * version, const char * pkgFile)
{

    PyObject *result = 0;
    install_state_t state = INSTALL_FAILED;

    if(PyInstance_Check(py_class_instance) && py_pre_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_pre_install, "(s)", pkgFile);
		if(result){
            state = pua_get_py_obj_state(result);

        }else
			printf("Error PyObject_CallMethod\n");    
	}else
	{
		printf("Not an instance object!\n");
	}	
    
    Py_XDECREF(result);

    return state;

}

install_state_t pua_install(const char * type, const char * pkgName, const char * version, const char * pkgFile)
{

    PyObject *result = 0;
    install_state_t state = INSTALL_FAILED;

    if(PyInstance_Check(py_class_instance) && py_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_install, "(s)",pkgFile);
		if(result){
            state = pua_get_py_obj_state(result);

        }else
			printf("Error PyObject_CallMethod\n");    
	}else
	{
		printf("Not an instance object!\n");
	}	
    
    Py_XDECREF(result);

    return state;

}

void pua_post_install(const char * type, const char * pkgName)
{

    PyObject *result = 0;

    if(PyInstance_Check(py_class_instance) && py_post_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_post_install, "(s)", pkgName);
		if(result){
            //TODO:

        }else
			printf("Error PyObject_CallMethod\n");    
	}else
	{
		printf("Not an instance object!\n");
	}	
    
    Py_XDECREF(result);

}

install_state_t pua_prepare_install(const char * type, const char * pkgName, const char * version, const char * pkgFile, char ** newFile)
{

    PyObject *result = 0;
    install_state_t state = INSTALL_FAILED;

    if(PyInstance_Check(py_class_instance) && py_prepare_install)
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_install, "(sss)", pkgName, version, pkgFile);
		if(result){
            state = pua_get_py_obj_state(result);

        }else
			printf("Error PyObject_CallMethod\n");    
	}else
	{
		printf("Not an instance object!\n");
	}	
    
    Py_XDECREF(result);

    return state;

}

download_state_t pua_prepare_download(const char * type, const char * pkgName, const char * version)
{

    PyObject *result = 0;
    install_state_t state = INSTALL_FAILED;

    if(PyInstance_Check(py_class_instance) && py_prepare_download)
	{
		result = PyObject_CallMethod(py_class_instance, py_prepare_download, "(ss)", pkgName, version);
		if(result){
            state = pua_get_py_obj_state(result);

        }else
			printf("Error PyObject_CallMethod\n");    
	}else
	{
		printf("Not an instance object!\n");
	}	
    
    Py_XDECREF(result);

    return state;

}