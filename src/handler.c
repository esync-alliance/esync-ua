/*
 * hander.c
 */

#include "handler.h"

#include <xl4ua.h>
#include <pthread.h>
#include "utils.h"
#include "xml.h"
#include "xl4busclient.h"

typedef struct update_err {
    int incremental_failed;
    int update_incapable;
    int terminal_failure;
} update_err_t;

static void process_message(ua_routine_t * uar, const char * msg, size_t len);
static void process_query_package(ua_routine_t * uar, json_object * jsonObj);
static void process_ready_download(ua_routine_t * uar, json_object * jsonObj);
static void process_ready_update(ua_routine_t * uar, json_object * jsonObj);
static void process_download_report(ua_routine_t * uar, json_object * jsonObj);
static void process_sota_report(ua_routine_t * uar, json_object * jsonObj);
static void process_log_report(ua_routine_t * uar, json_object * jsonObj);
static install_state_t pre_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile);
static install_state_t update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile);
static void post_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo);
static void send_update_status(pkg_info_t * pkgInfo, pkg_file_t * pkgFile, install_state_t state, update_err_t * ue);
static int backup_package(pkg_info_t * pkgInfo, pkg_file_t * pkgFile);
static int patch_delta(char * pkgName, char * version, char * diffFile, char ** patchedFile);
static char * install_state_string(install_state_t state);
static char * log_type_string(log_type_t log);
static int send_message(json_object * jsonObj);
void * runner_loop(void * arg);


ua_cfg_t ua_cfg = {0};
delta_stg_t delta_stg = {0};
runner_info_t * registered_updater = 0;


int ua_init(ua_cfg_t * ua_config) {

    int err = E_UA_OK;

    do {

        BOLT_IF(!ua_config || !ua_config->url
#ifdef USE_XL4BUS_TRUST
                || !ua_config->ua_type
#else
                || !ua_config->cert_dir
#endif
                , E_UA_ARG, "configuration error");

        BOLT_IF(ua_config->delta && (!S(ua_config->cache_dir) || !S(ua_config->backup_dir)), E_UA_ARG, "delta config error");

        memcpy(&ua_cfg, ua_config, sizeof(ua_cfg_t));

        if (ua_cfg.delta) {
            delta_stg.delta_cap = (ua_cfg.delta_config && S(ua_cfg.delta_config->delta_cap)) ?
                    f_strdup(ua_cfg.delta_config->delta_cap) : get_delta_capability();
        }

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
            ri->type = f_strdup((uah + i)->type_handler);
            ri->uar = (*(uah + i)->get_routine)();
            ri->run = 1;
            BOLT_IF(!ri->uar || !ri->uar->on_get_version || !ri->uar->on_install || !S(ri->type), E_UA_ARG, "registration error");
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
    for (int i=0; i<len; i++) {
        do {
            runner_info_t * ri = 0;
            const char * type = (uah + i)->type_handler;
            HASH_FIND_STR(registered_updater, type, ri);
            if (ri) {
                ri->run = 0;
                BOLT_SYS(pthread_cond_broadcast(&ri->cond), "cond broadcast");
                BOLT_SYS(pthread_mutex_unlock(&ri->lock), "lock unlock");
                BOLT_SYS(pthread_cond_destroy(&ri->cond), "cond destroy");
                BOLT_SYS(pthread_mutex_destroy(&ri->lock), "lock destroy");
                free(ri->type);
                free(ri);
                DBG("Unregistered: %s", type);
            }
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

        if (S(logdata->timestamp)) {
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
    runner_info_t * info = 0;
    do {
        DBG("Incoming message : %s", msg);
        HASH_FIND_STR(registered_updater, type, info);
        if (info) {
            BOLT_SYS(pthread_mutex_lock(&info->lock), "lock failed");
            incoming_msg_t * im = f_malloc(sizeof(incoming_msg_t));
            im->msg = f_strdup(msg);
            im->msg_len = len;
            im->msg_ts = currentms();
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

        while (info->run && !(im = info->queue))
            pthread_cond_wait(&info->cond, &info->lock);

        if (im) {
            DL_DELETE(info->queue, im);
            pthread_mutex_unlock(&info->lock);

            uint64_t tnow = currentms();
            uint64_t tout = im->msg_ts + MSG_TIMEOUT * 1000;

            if (tnow < tout)
                process_message(info->uar, im->msg, im->msg_len);
            else
                DBG("message timed out: %s", im->msg);

            free(im->msg);
            free(im);
        }
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

    pkg_info_t pkgInfo = {0};
    char * installedVer = 0;
    char * replyId;

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_replyid_from_json(jsonObj, &replyId)) {

        (*uar->on_get_version)(pkgInfo.name, &installedVer);

        DBG("DMClient is querying version info of : %s Returning %s", pkgInfo.name, NULL_STR(installedVer));

        json_object * pkgObject = json_object_new_object();
        json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo.type));
        json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo.name));
        json_object_object_add(pkgObject, "version", S(installedVer) ? json_object_new_string(installedVer) : NULL);

        if (ua_cfg.delta) {

            pkg_file_t *pf, *aux, *pkgFile = NULL;

            json_object_object_add(pkgObject, "delta-cap", json_object_new_string(delta_stg.delta_cap));

            char * pkgManifest = JOIN(ua_cfg.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG);

            if (!parse_pkg_manifest(pkgManifest, &pkgFile)) {

                json_object * verListObject = json_object_new_object();
                DL_FOREACH_SAFE(pkgFile, pf, aux) {

                    json_object * versionObject = json_object_new_object();
                    json_object_object_add(versionObject, "file", json_object_new_string(pf->file));
                    json_object_object_add(versionObject, "sha-256", json_object_new_string(pf->sha256b64));
                    json_object_object_add(versionObject, "downloaded", json_object_new_boolean(pf->downloaded? 1:0));
                    json_object_object_add(verListObject, pf->version, versionObject);

                    DL_DELETE(pkgFile, pf);
                    free(pf->version);
                    free(pf->file);
                    free(pf);
                }

                json_object_object_add(pkgObject, "version-list", verListObject);
            }

            free (pkgManifest);
        }

        json_object * bodyObject = json_object_new_object();
        json_object_object_add(bodyObject, "package", pkgObject);

        json_object * jObject = json_object_new_object();
        json_object_object_add(jObject, "type", json_object_new_string(QUERY_PACKAGE));
        json_object_object_add(jObject, "reply-to", json_object_new_string(replyId));
        json_object_object_add(jObject, "body", bodyObject);

        send_message(jObject);

        json_object_put(jObject);
    }

}


static void process_ready_download(ua_routine_t * uar, json_object * jsonObj) {

    pkg_info_t pkgInfo = {0};

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {

        DBG("DMClient informs that package %s having version %s is available for download", pkgInfo.name, pkgInfo.version);
        DBG("Instructing DMClient to download it");

        install_state_t state = INSTALL_PENDING;
        send_update_status(&pkgInfo, 0, state, 0);
    }

}


static void process_ready_update(ua_routine_t * uar, json_object * jsonObj) {

    char *installedVer;
    pkg_info_t pkgInfo = {0};
    pkg_file_t updateFile, pkgFile = {0};
    update_err_t updateErr = {0};
    install_state_t state;
    int bck = 0;

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_pkg_version_from_json(jsonObj, &pkgInfo.version) &&
            !get_pkg_rollback_version_from_json(jsonObj, &pkgInfo.rollback_version) &&
            !get_pkg_rollback_versions_from_json(jsonObj, &pkgInfo.rollback_versions)) {

        char * pkgManifest = JOIN(ua_cfg.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG);

        do {
            updateFile.version = pkgFile.version = S(pkgInfo.rollback_version) ? pkgInfo.rollback_version : pkgInfo.version;

            if ((!get_pkg_file_from_json(jsonObj, updateFile.version, &pkgFile.file) &&
                    !get_pkg_sha256_from_json(jsonObj, updateFile.version, pkgFile.sha256b64) &&
                    !get_pkg_downloaded_from_json(jsonObj, updateFile.version, &pkgFile.downloaded)) ||
                    ((!get_pkg_file_manifest(pkgManifest, updateFile.version, &pkgFile)) && (bck = 1))) {

                if (S(pkgInfo.rollback_version)) {
                    send_update_status(&pkgInfo, &(pkg_file_t){.version = pkgInfo.rollback_version, .downloaded = 1}, state = INSTALL_ROLLBACK, 0);
                }

                if (pre_update_action(uar, &pkgInfo, &pkgFile) == INSTALL_INPROGRESS) {

                    if (ua_cfg.delta && is_delta_package(pkgFile.file)) {

                        (*uar->on_get_version)(pkgInfo.name, &installedVer);

                        if (patch_delta(pkgInfo.name, installedVer, pkgFile.file, &updateFile.file)) {

                            updateErr.incremental_failed = 1;
                            state = INSTALL_FAILED;
                            break;
                        }

                        updateFile.downloaded = 0;

                    } else {

                        updateFile.file = f_strdup(pkgFile.file);
                        updateFile.downloaded = 1;
                    }

                    if((state = update_action(uar, &pkgInfo, &updateFile)) == INSTALL_COMPLETED) {

                        backup_package(&pkgInfo, &updateFile);

                    }

                    post_update_action(uar, &pkgInfo);

                    if (!updateFile.downloaded) remove(updateFile.file);
                    free(updateFile.file);
                }

                if (bck) {
                    free(pkgFile.version);
                    free(pkgFile.file);
                    bck = 0;
                }

            } else {
                if (S(pkgInfo.rollback_version)) {
                    send_update_status(&pkgInfo, &(pkg_file_t){.version = pkgInfo.rollback_version, .downloaded = 0}, state = INSTALL_ROLLBACK, 0);
                    break;
                }
            }

        } while ((state == INSTALL_FAILED) &&
                !get_pkg_next_rollback_version(pkgInfo.rollback_versions, updateFile.version, &pkgInfo.rollback_version) &&
                S(pkgInfo.rollback_version));

        if ((state == INSTALL_FAILED) && (updateErr.incremental_failed || updateErr.update_incapable ||
                (pkgInfo.rollback_versions && (updateErr.terminal_failure = 1)))) {
            send_update_status(&pkgInfo, 0, INSTALL_FAILED, &updateErr);
        }

        free(pkgManifest);
    }
}


static int patch_delta(char * pkgName, char * version, char * diffFile, char ** patchedFile) {

    int err = E_UA_OK;
    pkg_file_t pkgFile = {0};

    char * bname = f_basename(diffFile);
    char * pkgManifest = JOIN(ua_cfg.backup_dir, "backup", pkgName, MANIFEST_PKG);
    char * newFile = JOIN(ua_cfg.cache_dir, "delta", bname);

    if ((!get_pkg_file_manifest(pkgManifest, version, &pkgFile)) &&
            !delta_reconstruct(pkgFile.file, diffFile, newFile)) {

        *patchedFile = newFile;

    } else {

        err = E_UA_ERR;
        free(newFile);

    }

    free(bname);
    free(pkgManifest);

    return err;
}


static void process_download_report(ua_routine_t * uar, json_object * jsonObj) {

    pkg_info_t pkgInfo = {0};
    int64_t downloadedBytes, totalBytes;

    if (!get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
                !get_pkg_version_from_json(jsonObj, &pkgInfo.version) &&
                !get_downloaded_bytes_from_json(jsonObj, &downloadedBytes) &&
                !get_total_bytes_from_json(jsonObj, &totalBytes)) {

        DBG("Download in Progress %s : %s [%" PRId64 " / %" PRId64 "]", pkgInfo.name, pkgInfo.version, downloadedBytes, totalBytes);

    }
}


static void process_sota_report(ua_routine_t * uar, json_object * jsonObj) {

}


static void process_log_report(ua_routine_t * uar, json_object * jsonObj) {

}


static install_state_t pre_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile) {

    install_state_t state = INSTALL_INPROGRESS;

    if (uar->on_pre_install) {
    	char * newFile = 0;
        state = (*uar->on_pre_install)(pkgInfo->name, pkgFile->version, pkgFile->file, &newFile);
        if(S(newFile)) {
        	pkgFile->file = newFile;
        }
    }

    send_update_status(pkgInfo, 0, state, 0);

    return state;
}


static install_state_t update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile) {

    install_state_t state = (*uar->on_install)(pkgInfo->name, pkgFile->version, pkgFile->file);

    if ((state == INSTALL_COMPLETED) && uar->on_set_version) {
        (*uar->on_set_version)(pkgInfo->name, pkgFile->version);
    }

    if (!(pkgInfo->rollback_versions && (state == INSTALL_FAILED))) {
        send_update_status(pkgInfo, 0, state, 0);
    }

    return state;
}


static void post_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo) {

    if (uar->on_post_install) {
        (*uar->on_post_install)(pkgInfo->name);
    }

}


static void send_update_status(pkg_info_t * pkgInfo, pkg_file_t * pkgFile, install_state_t state, update_err_t * ue) {

    json_object * pkgObject = json_object_new_object();
    json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
    json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo->type));
    json_object_object_add(pkgObject, "version", json_object_new_string(pkgInfo->version));
    json_object_object_add(pkgObject, "status", json_object_new_string(install_state_string(state)));
    json_object_object_add(pkgObject, "rollback-version", pkgInfo->rollback_version ? json_object_new_string(pkgInfo->rollback_version) : NULL);
    json_object_object_add(pkgObject, "rollback-versions", pkgInfo->rollback_versions ? json_object_get(pkgInfo->rollback_versions) : NULL);


    if ((state == INSTALL_FAILED) && ue) {

        if (ue->terminal_failure) {
            json_object_object_add(pkgObject, "terminal-failure", json_object_new_boolean(1));
        }
        if (ue->update_incapable) {
            json_object_object_add(pkgObject, "update-incapable", json_object_new_boolean(1));
        }
        if (ue->incremental_failed) {
            json_object * versionObject = json_object_new_object();
            json_object * verListObject = json_object_new_object();
            json_object_object_add(versionObject, "incremental-failed", json_object_new_boolean(1));
            json_object_object_add(verListObject, pkgInfo->version, versionObject);
            json_object_object_add(pkgObject, "version-list", verListObject);
        }
    }

    if ((state == INSTALL_ROLLBACK) && pkgFile) {
        json_object * versionObject = json_object_new_object();
        json_object * verListObject = json_object_new_object();
        json_object_object_add(versionObject, "downloaded", json_object_new_boolean(pkgFile->downloaded? 1:0));
        json_object_object_add(verListObject, pkgFile->version, versionObject);
        json_object_object_add(pkgObject, "version-list", verListObject);
    }

    json_object * bodyObject = json_object_new_object();
    json_object_object_add(bodyObject, "package", pkgObject);

    json_object * jObject = json_object_new_object();
    json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
    json_object_object_add(jObject, "body", bodyObject);

    send_message(jObject);

    json_object_put(jObject);

}


static int backup_package(pkg_info_t * pkgInfo, pkg_file_t * pkgFile) {

    int err = E_UA_OK;
    pkg_file_t backupFile = {0};

    char * bname = f_basename(pkgFile->file);
    char * pkgManifest = JOIN(ua_cfg.backup_dir, "backup", pkgInfo->name, MANIFEST_PKG);
    backupFile.file = JOIN(ua_cfg.backup_dir, "backup", pkgInfo->name, pkgFile->version, bname);
    backupFile.version = pkgFile->version;
    backupFile.downloaded = 1;

    if (!strcmp(pkgFile->file, backupFile.file)) {

        DBG("Back up already exists: %s", backupFile.file);

    } else if (!calc_sha256_b64(pkgFile->file, backupFile.sha256b64) &&
            !copy_file(pkgFile->file, backupFile.file) &&
            !add_pkg_file_manifest(pkgManifest, &backupFile)) {

        DBG("Backed up package: %s", backupFile.file);

    } else {

        DBG("Backing up failed: %s", backupFile.file);
        err = E_UA_ERR;

    }

    //Todo: limit the number of backups to avoid running out of space

    free(bname);
    free(pkgManifest);
    free(backupFile.file);

    return err;
}


static char * install_state_string(install_state_t state) {

    char * str = NULL;
    switch (state) {
        case INSTALL_PENDING    : str = "INSTALL_PENDING";     break;
        case INSTALL_INPROGRESS : str = "INSTALL_IN_PROGRESS"; break;
        case INSTALL_COMPLETED  : str = "INSTALL_COMPLETED";   break;
        case INSTALL_FAILED     : str = "INSTALL_FAILED";      break;
        case INSTALL_POSTPONED  : str = "INSTALL_POSTPONED";   break;
        case INSTALL_ABORTED    : str = "INSTALL_ABORTED";     break;
        case INSTALL_ROLLBACK   : str = "INSTALL_ROLLBACK";    break;
    }

    return str;
}


static char * log_type_string(log_type_t log) {

    char * str = NULL;
    switch (log) {
        case LOG_EVENT      : str = "EVENT";  break;
        case LOG_INFO       : str = "INFO";   break;
        case LOG_WARN       : str = "WARN";   break;
        case LOG_ERROR      : str = "ERROR";  break;
        case LOG_SEVERE     : str = "SEVERE"; break;
    }

    return str;
}

const char * ua_get_xl4bus_version() {

    return xl4bus_get_version();

}
