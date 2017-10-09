/*
 * handler.c
 */
#include "handler.h"
#include "utils.h"
#include "config.h"
#include "debug.h"
#include "uaclient.h"

static void process_query_package(json_object * jsonObj);
static void process_ready_download(json_object * jsonObj);
static void process_ready_update(json_object * jsonObj);
static void process_download_report(json_object * jsonObj);
static void process_sota_report(json_object * jsonObj);
static void process_log_report(json_object * jsonObj);
static void pre_update_action(char * nodeType, char * packageName, char * installVersion);
static void update_action(char * nodeType, char * packageName, char * installVersion, char * downloadFile);
static void post_update_action(void);
static char * install_state_string(install_state_t state);


update_agent_t * uah;


int ua_handler_start(update_agent_t * ua) {

    if (!ua->do_get_version || !ua->do_set_version || !ua->do_install || !ua->url
#ifdef USE_XL4BUS_TRUST
            || !ua->ua_type
#else
            || !ua->cert_dir
#endif
             )
        return E_UA_ARG;

    uah = ua;

    return xl4bus_client_start(uah->url, ua->cert_dir, uah->ua_type);

}


void ua_handler_stop(void) {

    xl4bus_client_stop();

}


int ua_send_message(json_object * jsonObj) {

    DBG("Sending : %s", json_object_to_json_string(jsonObj));
    return xl4bus_client_send_msg(json_object_to_json_string(jsonObj));

}


void handle_status(int status) {

    DBG("Status : %d", status);

}


void handle_delivered(const char * msg, int ok) {

    DBG("Message delivered %s", ok ? "OK" : "NOT OK");

}


void handle_presence(int connected, int disconnected) {

    DBG("Connected : %d,  Disconnected : %d", connected, disconnected);

}


void handle_message(const char * msg) {

    DBG("Message : %s", msg);
    json_object * json;
    char * type;

    if((json = json_tokener_parse(msg))) {
        if (get_type_from_json(json, &type) == E_UA_OK) {
            if (!strcmp(type, QUERY_PACKAGE)) {
                process_query_package(json);
            } else if (!strcmp(type, READY_DOWNLOAD)) {
                process_ready_download(json);
            } else if (!strcmp(type, READY_UPDATE)) {
                process_ready_update(json);
            } else if (!strcmp(type, DOWNLOAD_REPORT)) {
                process_download_report(json);
            } else if (!strcmp(type, SOTA_REPORT)) {
                process_sota_report(json);
            } else if (!strcmp(type, LOG_REPORT)) {
                process_log_report(json);
            } else {
                DBG("Unknown type %s received in %s", type, json_object_to_json_string(json));
            }
        }
        json_object_put(json);
    } else {
        DBG("Json parsing error : %s", msg);
    }

}


static void process_query_package(json_object * jsonObj) {

    char * nodeType;
    char * packageName;
    char * replyId;
    char * installedVer;

    if (!get_pkg_type_from_json(jsonObj, &nodeType) &&
            !get_pkg_name_from_json(jsonObj, &packageName) &&
            !get_replyid_from_json(jsonObj, &replyId)) {

        (*uah->do_get_version)(packageName, &installedVer);

        DBG("DMClient is querying version info of : %s Returning %s", packageName, installedVer);

        json_object * pkgObject = json_object_new_object();
        json_object_object_add(pkgObject, "type", json_object_new_string(nodeType));
        json_object_object_add(pkgObject, "name", json_object_new_string(packageName));
        json_object_object_add(pkgObject, "version", json_object_new_string(SAFE_STR(installedVer)));

        json_object * bodyObject = json_object_new_object();
        json_object_object_add(bodyObject, "package", pkgObject);

        json_object * jObject = json_object_new_object();
        json_object_object_add(jObject, "type", json_object_new_string(QUERY_PACKAGE));
        json_object_object_add(jObject, "reply-to", json_object_new_string(replyId));
        json_object_object_add(jObject, "body", bodyObject);

        ua_send_message(jObject);

        json_object_put(pkgObject);
        json_object_put(bodyObject);
        json_object_put(jObject);
    }

}


static void process_ready_download(json_object * jsonObj) {

    char * nodeType;
    char * packageName;
    char * installVersion;

    if (!get_pkg_type_from_json(jsonObj, &nodeType) &&
            !get_pkg_name_from_json(jsonObj, &packageName) &&
            !get_pkg_version_from_json(jsonObj, &installVersion)) {

        DBG("DMClient informs that package %s having version %s is available for download", packageName, installVersion);
        DBG("Instructing DMClient to download it");

        json_object * pkgObject = json_object_new_object();
        json_object_object_add(pkgObject, "version", json_object_new_string(installVersion));
        json_object_object_add(pkgObject, "status", json_object_new_string("INSTALL_PENDING"));
        json_object_object_add(pkgObject, "name", json_object_new_string(packageName));
        json_object_object_add(pkgObject, "type", json_object_new_string(nodeType));

        json_object * bodyObject = json_object_new_object();
		json_object_object_add(bodyObject, "package", pkgObject);

        json_object * jObject = json_object_new_object();
        json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
        json_object_object_add(jObject, "body", bodyObject);

        ua_send_message(jObject);

        json_object_put(bodyObject);
        json_object_put(jObject);
    }

}


static void process_ready_update(json_object * jsonObj) {

    char * nodeType;
    char * packageName;
    char * installVersion;
    char * downloadFile;

    if (!get_pkg_type_from_json(jsonObj, &nodeType) &&
            !get_pkg_name_from_json(jsonObj, &packageName) &&
            !get_pkg_version_from_json(jsonObj, &installVersion) &&
            !get_file_from_json(jsonObj, installVersion, &downloadFile)) {

        pre_update_action(nodeType, packageName, installVersion);
        update_action(nodeType, packageName, installVersion, downloadFile);
        post_update_action();
    }

}


static void process_download_report(json_object * jsonObj) {

}


static void process_sota_report(json_object * jsonObj) {

}


static void process_log_report(json_object * jsonObj) {

}


static void pre_update_action(char * nodeType, char * packageName, char * installVersion) {

    install_state_t state = INSTALL_INPROGRESS;

    if (uah->do_pre_install) {
        state = (*uah->do_pre_install)(packageName, installVersion);
    }

    json_object * pkgObject = json_object_new_object();
    json_object_object_add(pkgObject, "version", json_object_new_string(installVersion));
    json_object_object_add(pkgObject, "status", json_object_new_string(install_state_string(state)));
    json_object_object_add(pkgObject, "name", json_object_new_string(packageName));
    json_object_object_add(pkgObject, "type", json_object_new_string(nodeType));

    json_object * bodyObject = json_object_new_object();
	json_object_object_add(bodyObject, "package", pkgObject);

    json_object * jObject = json_object_new_object();
    json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
    json_object_object_add(jObject, "body", bodyObject);

    ua_send_message(jObject);

    json_object_put(bodyObject);
    json_object_put(jObject);

}

static void update_action(char * nodeType, char * packageName, char * installVersion, char * downloadFile) {

    install_state_t state = (*uah->do_install)(packageName, installVersion, downloadFile);

    if (state == INSTALL_COMPLETED) {
        (*uah->do_set_version)(packageName, installVersion);
    }

    json_object * pkgObject = json_object_new_object();
    json_object_object_add(pkgObject, "version", json_object_new_string(installVersion));
    json_object_object_add(pkgObject, "status", json_object_new_string(install_state_string(state)));
    json_object_object_add(pkgObject, "name", json_object_new_string(packageName));
    json_object_object_add(pkgObject, "type", json_object_new_string(nodeType));

    json_object * bodyObject = json_object_new_object();
	json_object_object_add(bodyObject, "package", pkgObject);

    json_object * jObject = json_object_new_object();
    json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
    json_object_object_add(jObject, "body", bodyObject);

    ua_send_message(jObject);

    json_object_put(bodyObject);
    json_object_put(jObject);

}

static void post_update_action(void) {

    if (uah->do_post_install) {
        (*uah->do_post_install)();
    }

}


static char * install_state_string(install_state_t state) {

    char * str = NULL;
    switch (state)
    {
        case INSTALL_PENDING    : str = "INSTALL_PENDING";    break;
        case INSTALL_INPROGRESS : str = "INSTALL_INPROGRESS"; break;
        case INSTALL_COMPLETED  : str = "INSTALL_COMPLETED";  break;
        case INSTALL_FAILED     : str = "INSTALL_FAILED";     break;
        case INSTALL_POSTPONED  : str = "INSTALL_POSTPONED";  break;
        case INSTALL_ABORTED    : str = "INSTALL_ABORTED";    break;
        case INSTALL_ROLLBACK   : str = "INSTALL_ROLLBACK";   break;
    }
    return str;

}

