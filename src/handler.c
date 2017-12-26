/*
 * hander.c
 */

#include "handler.h"

#include <xl4ua.h>
#include <pthread.h>
#include "utils.h"
#include "xl4busclient.h"

static void process_message(ua_routine_t * uar, const char * msg, size_t len);
static void process_query_package(ua_routine_t * uar, json_object * jsonObj);
static void process_ready_download(ua_routine_t * uar, json_object * jsonObj);
static void process_ready_update(ua_routine_t * uar, json_object * jsonObj);
static void process_download_report(ua_routine_t * uar, json_object * jsonObj);
static void process_sota_report(ua_routine_t * uar, json_object * jsonObj);
static void process_log_report(ua_routine_t * uar, json_object * jsonObj);
static void pre_update_action(ua_routine_t * uar, char * pkgType, char * pkgName, char * pkgVersion);
static void update_action(ua_routine_t * uar, char * pkgType, char * pkgName, char * pkgVersion, char * downloadFile);
static void post_update_action(ua_routine_t * uar);
static void send_update_status(char * pkgType, char * pkgName, char * pkgVersion, install_state_t state);
static char * install_state_string(install_state_t state);
static char * log_type_string(log_type_t log);
static int send_message(json_object * jsonObj);
void * runner_loop(void * arg);

ua_cfg_t ua_cfg;
runner_info_t * registered_updater = 0;

#define TRIM(x) do { size_t __l = strlen(x); if (x[__l-1]=="/") x[__l-1] = 0 } while (0)

int ua_init(ua_cfg_t * in_cfg) {

    int err = E_UA_OK;

    do {

        BOLT_IF(!in_cfg->url
#ifdef USE_XL4BUS_TRUST
                || !in_cfg->ua_type
#else
                || !in_cfg->cert_dir
#endif
                , E_UA_ARG, "configuration error");

        memcpy(&ua_cfg, in_cfg, sizeof(ua_cfg_t));

        BOLT_SUB(xl4bus_client_init(&ua_cfg));

    } while (0);

    return err;

}

int ua_register(ua_handler_t * uah, int len) {

    int err = E_UA_OK;
    runner_info_t * ri = 0;
    for (int i=0; i<len; i++) {
        do {
            ri = f_malloc(sizeof(runner_info_t));
            ri->uar = (*(uah + i)->get_routine)();
            ri->type = (uah + i)->type_handler;
            ri->run = 1;
            BOLT_IF(!ri->uar->on_get_version || !ri->uar->on_install || !strlen(ri->type), E_UA_ARG, "registration error");
            BOLT_SYS(pthread_mutex_init(&ri->lock, 0), "lock init");
            BOLT_SYS(pthread_cond_init(&ri->cond, 0), "cond init");
            BOLT_SYS(pthread_create(&ri->thread, 0, runner_loop, ri), "pthread create");
            HASH_ADD_STR(registered_updater, type, ri);
            DBG("Registered: %s", ri->type);
        } while (0);

        if (err) {
            free(ri);
            break;
        }
    }

    return err;

}

int ua_unregister(ua_handler_t * uah, int len) {

    int err = E_UA_OK;
    runner_info_t * ri = 0;
    for (int i=0; i<len; i++) {
        do {
            const char * type = (uah + i)->type_handler;
            HASH_FIND_STR(registered_updater, type, ri);
            ri->run = 0;
            BOLT_SYS(pthread_cond_broadcast(&ri->cond), "cond broadcast");
            BOLT_SYS(pthread_mutex_unlock(&ri->lock), "lock unlock");
            BOLT_SYS(pthread_cond_destroy(&ri->cond), "cond destroy");
            BOLT_SYS(pthread_mutex_destroy(&ri->lock), "lock destroy");
            free(ri);
            DBG("Unregistered: %s", type);
        } while (0);

        if (err) {
            break;
        }
    }

    return err;

}

int ua_stop() {

    return xl4bus_client_stop();

}

int ua_report_log(char * pkgType, log_data_t * logdata, log_type_t logtype) {

    int err = E_UA_OK;
    char timestamp[30];

    do {
        BOLT_IF(!(pkgType && *pkgType) || !(logdata->message || logdata->binary), E_UA_ARG, "log report invalid");

        if (logdata->timestamp && *logdata->timestamp) {
            strncpy(timestamp, logdata->timestamp, sizeof(timestamp));
        } else {
            time_t t = time(NULL);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
        }

        json_object * bodyObject = json_object_new_object();
        json_object_object_add(bodyObject, "type", json_object_new_string(pkgType));
        json_object_object_add(bodyObject, "level", json_object_new_string(log_type_string(logtype)));
        json_object_object_add(bodyObject, "timestamp", json_object_new_string(timestamp));

        if (logdata->message) {
            json_object_object_add(bodyObject, "message", json_object_get((json_object *)logdata->message));
            json_object_object_add(bodyObject, "compound", json_object_new_boolean(logdata->compound? 1 : 0));
        } else if (logdata->binary){
            json_object_object_add(bodyObject, "binary", json_object_new_string(logdata->binary));
        }

        json_object * jObject = json_object_new_object();
        json_object_object_add(jObject, "type", json_object_new_string(LOG_REPORT));
        json_object_object_add(jObject, "body", bodyObject);

        err = send_message(jObject);

        json_object_put(bodyObject);
        json_object_put(jObject);

    } while(0);

    return err;
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


void handle_message(const char * type, const char * msg, size_t len) {

    int err;
    runner_info_t * info;
    do {
        DBG("Incoming message : %s", msg);
        HASH_FIND_STR(registered_updater, type, info);
        if (info) {
            BOLT_SYS(pthread_mutex_lock(&info->lock), "lock failed");
            incoming_msg_t * im = f_malloc(sizeof(incoming_msg_t));
            im->msg = f_strdup(msg);
            im->msg_len = len;
            DL_APPEND(info->queue, im);
            BOLT_SYS(pthread_cond_signal(&info->cond), "cond signal");
            BOLT_SYS(pthread_mutex_unlock(&info->lock), "unlock failed");
        } else {
            DBG("Incoming message on non-registered %s : %s", type, msg);
        }
    } while (0);

}

int send_message(json_object * jsonObj) {

    DBG("Sending : %s", json_object_to_json_string(jsonObj));
    return xl4bus_client_send_msg(json_object_to_json_string(jsonObj));

}

void * runner_loop(void * arg) {

    runner_info_t * info = arg;
    while(info->run) {
        incoming_msg_t * im;
        if (pthread_mutex_lock(&info->lock)) {
            DBG_SYS("lock failed");
            continue;
        }

        while (!(im = info->queue))
            pthread_cond_wait(&info->cond, &info->lock);

        DL_DELETE(info->queue, im);
        pthread_mutex_unlock(&info->lock);

        process_message(info->uar, im->msg, im->msg_len);

        free(im->msg);
        free(im);
    }

}


static void process_message(ua_routine_t * uar, const char * msg, size_t len) {

    char * type;
    enum json_tokener_error jErr;

    json_object * jObj = json_tokener_parse_verbose(msg, &jErr);
    if(jErr == json_tokener_success) {
        if (get_type_from_json(jObj, &type) == E_UA_OK) {
            if (!strcmp(type, QUERY_PACKAGE)) {
                process_query_package(uar, jObj);
            } else if (!strcmp(type, READY_DOWNLOAD)) {
                process_ready_download(uar, jObj);
            } else if (!strcmp(type, READY_UPDATE)) {
                process_ready_update(uar, jObj);
            } else if (!strcmp(type, DOWNLOAD_REPORT)) {
                process_download_report(uar, jObj);
            } else if (!strcmp(type, SOTA_REPORT)) {
                process_sota_report(uar, jObj);
            } else if (!strcmp(type, LOG_REPORT)) {
                process_log_report(uar, jObj);
            } else {
                DBG("Unknown type %s received in %s", type, json_object_to_json_string(jObj));
            }
        }
    } else {
        DBG("Failed to parse json (%s): %s", json_tokener_error_desc(jErr), msg);
    }
    json_object_put(jObj);

}


static void process_query_package(ua_routine_t * uar, json_object * jsonObj) {

    char * pkgType;
    char * pkgName;
    char * replyId;
    char * installedVer;

    if (!get_pkg_type_from_json(jsonObj, &pkgType) &&
            !get_pkg_name_from_json(jsonObj, &pkgName) &&
            !get_replyid_from_json(jsonObj, &replyId)) {

        (*uar->on_get_version)(pkgName, &installedVer);

        DBG("DMClient is querying version info of : %s Returning %s", pkgName, NULL_STR(installedVer));

        json_object * pkgObject = json_object_new_object();
        json_object_object_add(pkgObject, "type", json_object_new_string(pkgType));
        json_object_object_add(pkgObject, "name", json_object_new_string(pkgName));
        json_object_object_add(pkgObject, "version", json_object_new_string(NULL_STR(installedVer)));

        json_object * bodyObject = json_object_new_object();
        json_object_object_add(bodyObject, "package", pkgObject);

        json_object * jObject = json_object_new_object();
        json_object_object_add(jObject, "type", json_object_new_string(QUERY_PACKAGE));
        json_object_object_add(jObject, "reply-to", json_object_new_string(replyId));
        json_object_object_add(jObject, "body", bodyObject);

        send_message(jObject);

        json_object_put(pkgObject);
        json_object_put(bodyObject);
        json_object_put(jObject);
    }

}


static void process_ready_download(ua_routine_t * uar, json_object * jsonObj) {

    char * pkgType;
    char * pkgName;
    char * pkgVersion;

    if (!get_pkg_type_from_json(jsonObj, &pkgType) &&
            !get_pkg_name_from_json(jsonObj, &pkgName) &&
            !get_pkg_version_from_json(jsonObj, &pkgVersion)) {

        DBG("DMClient informs that package %s having version %s is available for download", pkgName, pkgVersion);
        DBG("Instructing DMClient to download it");

        install_state_t state = INSTALL_PENDING;
        send_update_status(pkgType, pkgName, pkgVersion, state);
    }

}


static void process_ready_update(ua_routine_t * uar, json_object * jsonObj) {

    char * pkgType;
    char * pkgName;
    char * pkgVersion;
    char * pkgFile;

    if (!get_pkg_type_from_json(jsonObj, &pkgType) &&
            !get_pkg_name_from_json(jsonObj, &pkgName) &&
            !get_pkg_version_from_json(jsonObj, &pkgVersion) &&
            !get_file_from_json(jsonObj, pkgVersion, &pkgFile)) {

        pre_update_action(uar, pkgType, pkgName, pkgVersion);
        update_action(uar, pkgType, pkgName, pkgVersion, pkgFile);
        post_update_action(uar);
    }

}


static void process_download_report(ua_routine_t * uar, json_object * jsonObj) {

    char * pkgName;
    char * pkgVersion;
    int64_t downloadedBytes;
    int64_t totalBytes;

    if (!get_pkg_name_from_json(jsonObj, &pkgName) &&
                !get_pkg_version_from_json(jsonObj, &pkgVersion) &&
                !get_downloaded_bytes_from_json(jsonObj, &downloadedBytes) &&
                !get_total_bytes_from_json(jsonObj, &totalBytes)) {

        DBG("Download in Progress %s : %s [%" PRId64 " / %" PRId64 "]", pkgName, pkgVersion, downloadedBytes, totalBytes);

    }
}


static void process_sota_report(ua_routine_t * uar, json_object * jsonObj) {

}


static void process_log_report(ua_routine_t * uar, json_object * jsonObj) {

}


static void pre_update_action(ua_routine_t * uar, char * pkgType, char * pkgName, char * pkgVersion) {

    install_state_t state = INSTALL_INPROGRESS;

    if (uar->on_pre_install) {
        state = (*uar->on_pre_install)(pkgName, pkgVersion);
    }

    send_update_status(pkgType, pkgName, pkgVersion, state);

}


static void update_action(ua_routine_t * uar, char * pkgType, char * pkgName, char * pkgVersion, char * downloadFile) {

    install_state_t state = (*uar->on_install)(pkgName, pkgVersion, downloadFile);

    if ((state == INSTALL_COMPLETED) && uar->on_set_version) {
        (*uar->on_set_version)(pkgName, pkgVersion);
    }

    send_update_status(pkgType, pkgName, pkgVersion, state);

}


static void post_update_action(ua_routine_t * uar) {

    if (uar->on_post_install) {
        (*uar->on_post_install)();
    }

}

static void send_update_status(char * pkgType, char * pkgName, char * pkgVersion, install_state_t state) {

    json_object * pkgObject = json_object_new_object();
    json_object_object_add(pkgObject, "version", json_object_new_string(pkgVersion));
    json_object_object_add(pkgObject, "status", json_object_new_string(install_state_string(state)));
    json_object_object_add(pkgObject, "name", json_object_new_string(pkgName));
    json_object_object_add(pkgObject, "type", json_object_new_string(pkgType));

    json_object * bodyObject = json_object_new_object();
    json_object_object_add(bodyObject, "package", pkgObject);

    json_object * jObject = json_object_new_object();
    json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
    json_object_object_add(jObject, "body", bodyObject);

    send_message(jObject);

    json_object_put(pkgObject);
    json_object_put(bodyObject);
    json_object_put(jObject);

}

static char * install_state_string(install_state_t state) {

    char * str = NULL;
    switch (state) {
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

static char * log_type_string(log_type_t log) {

    char * str = NULL;
    switch (log) {
        case LOG_EVENT      : str = "EVENT";    break;
        case LOG_INFO       : str = "INFO"; break;
        case LOG_WARN       : str = "WARN";  break;
        case LOG_ERROR      : str = "ERROR";     break;
        case LOG_SEVERE     : str = "SEVERE";  break;
    }
    return str;

}
