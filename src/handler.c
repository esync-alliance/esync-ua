/*
 * hander.c
 */

#include "handler.h"

#include <xl4ua.h>
#include <pthread.h>
#include "utils.h"
#include "delta.h"
#include "misc.h"
#include "xml.h"
#include "xl4busclient.h"
#include "ua_version.h"

static void process_message(ua_unit_t * ui, const char * msg, size_t len);
static void process_run(ua_unit_t * ui, process_f func, json_object * jObj, int turnover);
static void process_query_package(ua_routine_t * uar, json_object * jsonObj);
static void process_ready_download(ua_routine_t * uar, json_object * jsonObj);
static void process_prepare_update(ua_routine_t * uar, json_object * jsonObj);
static void process_ready_update(ua_routine_t * uar, json_object * jsonObj);
static void process_confirm_update(ua_routine_t * uar, json_object * jsonObj);
static void process_download_report(ua_routine_t * uar, json_object * jsonObj);
static void process_sota_report(ua_routine_t * uar, json_object * jsonObj);
static void process_log_report(ua_routine_t * uar, json_object * jsonObj);
static int transfer_file_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile);
static install_state_t prepare_install_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile, int bck, pkg_file_t * updateFile, update_err_t * ue);
static download_state_t prepare_download_action(ua_routine_t * uar, pkg_info_t * pkgInfo);
static install_state_t pre_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile);
static install_state_t update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile);
static void post_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo);
static void send_install_status(pkg_info_t * pkgInfo, install_state_t state, pkg_file_t * pkgFile, update_err_t ue);
static void send_download_status(pkg_info_t * pkgInfo, download_state_t state);
static int send_update_report(const char * pkgName, const char * version, int indeterminate, int percent, update_stage_t us);
static int backup_package(pkg_info_t * pkgInfo, pkg_file_t * pkgFile);
static int patch_delta(char * pkgManifest, char * version, char * diffFile, char * newFile);
static char * install_state_string(install_state_t state);
static char * download_state_string(download_state_t state);
static char * update_stage_string(update_stage_t stage);
static char * log_type_string(log_type_t log);
void * runner_loop(void * arg);
static void query_hash_tree(runner_info_hash_tree_t * current, runner_info_t * ri, const char * ua_type, int is_delete, UT_array * gather, int tip);

int ua_debug = 0;
ua_internal_t ua_intl = {0};
runner_info_hash_tree_t * ri_tree = 0;


int ua_init(ua_cfg_t * uaConfig) {

    int err = E_UA_OK;

    do {

        ua_debug = uaConfig->debug;

        BOLT_IF(!uaConfig || !S(uaConfig->url)
#ifdef USE_XL4BUS_TRUST
                || !S(uaConfig->ua_type)
#else
                || !S(uaConfig->cert_dir)
#endif
                , E_UA_ARG, "configuration error");

        BOLT_IF(uaConfig->delta && (!S(uaConfig->cache_dir) || !S(uaConfig->backup_dir)), E_UA_ARG, "cache and backup directory are must for delta");
        BOLT_IF(uaConfig->delta && delta_init(uaConfig->cache_dir, uaConfig->delta_config), E_UA_ARG, "delta initialization error");

        memset(&ua_intl, 0, sizeof(ua_internal_t));
        ua_intl.delta = uaConfig->delta;
        ua_intl.cache_dir = S(uaConfig->cache_dir) ? f_strdup(uaConfig->cache_dir) : NULL;
        ua_intl.backup_dir = S(uaConfig->backup_dir) ? f_strdup(uaConfig->backup_dir) : NULL;

        BOLT_SUB(xl4bus_client_init(uaConfig->url, uaConfig->cert_dir));

        if (uaConfig->rw_buffer_size) ua_rw_buff_size = uaConfig->rw_buffer_size * 1024;

    } while (0);

    if (err) {
        ua_stop();
    }

    return err;
}


int ua_stop() {

    if (ua_intl.cache_dir) { free(ua_intl.cache_dir); ua_intl.cache_dir = NULL; }
    if (ua_intl.backup_dir) { free(ua_intl.backup_dir); ua_intl.backup_dir = NULL; }
    delta_stop();
    return xl4bus_client_stop();

}


int ua_register(ua_handler_t * uah, int len) {

    int err = E_UA_OK;
    runner_info_t * ri = 0;

    if (!ri_tree) {
        ri_tree = f_malloc(sizeof(runner_info_hash_tree_t));
        utarray_init(&ri_tree->items, &ut_ptr_icd);
    }

    for (int i = 0; i < len; i++) {

        do {
            ri = f_malloc(sizeof(runner_info_t));
            ri->unit.type = f_strdup((uah + i)->type_handler);
            ri->unit.uar = (*(uah + i)->get_routine)();
            ri->run = 1;
            BOLT_IF(!ri->unit.uar || !ri->unit.uar->on_get_version || !ri->unit.uar->on_install || !S(ri->unit.type), E_UA_ARG, "registration error");
            BOLT_SYS(pthread_mutex_init(&ri->lock, 0), "lock init");
            BOLT_SYS(pthread_cond_init(&ri->cond, 0), "cond init");
            BOLT_SYS(pthread_create(&ri->thread, 0, runner_loop, ri), "pthread create");
            query_hash_tree(ri_tree, ri, ri->unit.type, 0, 0, 0);
            DBG("Registered: %s", ri->unit.type);
        } while (0);

        if (err) {
            DBG("Failed to register: %s", (uah + i)->type_handler);
            free(ri);
            ua_unregister(uah, len);
            break;
        }
    }

    return err;
}


int ua_unregister(ua_handler_t * uah, int len) {

    int ret = E_UA_OK;
    UT_array ri_list;

    for (int i = 0; i < len; i++) {

        const char * type = (uah + i)->type_handler;
        ua_routine_t * uar = (*(uah + i)->get_routine)();

        utarray_init(&ri_list, &ut_ptr_icd);
        query_hash_tree(ri_tree, 0, type, 0, &ri_list, 1);

        int l = utarray_len(&ri_list);
        for (int j = 0; j < l; j++) {

            int err = E_UA_OK;
            do {
                runner_info_t * ri = *(runner_info_t **) utarray_eltptr(&ri_list, j);
                if (ri->unit.uar == uar) {
                    query_hash_tree(ri_tree, ri, type, 1, 0, 0);
                    ri->run = 0;
                    BOLT_SYS(pthread_cond_broadcast(&ri->cond), "cond broadcast");
                    BOLT_SYS(pthread_mutex_unlock(&ri->lock), "lock unlock");
                    BOLT_SYS(pthread_cond_destroy(&ri->cond), "cond destroy");
                    BOLT_SYS(pthread_mutex_destroy(&ri->lock), "lock destroy");
                    free(ri->unit.type);
                    free(ri);
                    DBG("Unregistered: %s", type);
                }
            } while (0);

            if (err) {
                DBG("Failed to unregister: %s", type);
                ret = err;
            }
        }

        utarray_done(&ri_list);
    }

    return ret;
}


int ua_send_message(json_object * jsonObj) {

    DBG("Sending : %s", json_object_to_json_string(jsonObj));
    return xl4bus_client_send_msg(json_object_to_json_string(jsonObj));

}


int ua_send_install_progress(const char * pkgName, const char * version, int indeterminate, int percent) {

    return send_update_report(pkgName, version, indeterminate, percent, US_INSTALL);

}


int ua_send_transfer_progress(const char * pkgName, const char * version, int indeterminate, int percent) {

    return send_update_report(pkgName, version, indeterminate, percent, US_TRANSFER);

}


int ua_send_log_report(char * pkgType, log_type_t logtype, log_data_t * logdata) {

    int err = E_UA_OK;
    char timestamp[30];

    do {
        BOLT_IF(!S(pkgType) || !(logdata->message || logdata->binary), E_UA_ARG, "log report invalid");

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
        } else if (logdata->binary) {
            json_object_object_add(bodyObject, "binary", json_object_new_string(logdata->binary));
        }

        json_object * jObject = json_object_new_object();
        json_object_object_add(jObject, "type", json_object_new_string(LOG_REPORT));
        json_object_object_add(jObject, "body", bodyObject);

        err = ua_send_message(jObject);

        json_object_put(jObject);

    } while(0);

    return err;
}


void handle_status(int status) {

    DBG("Status : %d", status);

}


void handle_delivered(const char * msg, int ok) {

    DBG("Message delivered %s : %s", ok ? "OK" : "NOT OK", msg);

}


void handle_presence(int connected, int disconnected) {

    DBG("Connected : %d,  Disconnected : %d", connected, disconnected);

}


void handle_message(const char * type, const char * msg, size_t len) {

    int err = E_UA_OK;
    UT_array ri_list;

    DBG("Incoming message : %s", msg);

    utarray_init(&ri_list, &ut_ptr_icd);
    query_hash_tree(ri_tree, 0, type, 0, &ri_list, 0);

    int l = utarray_len(&ri_list);

    if (l)
        DBG("Registered handlers found for %s : %d", type, l);
    else
        DBG("Incoming message for non-registered handler %s : %s", type, msg);

    for (int j = 0; j < l; j++) {

        runner_info_t * ri = *(runner_info_t **) utarray_eltptr(&ri_list, j);
        BOLT_SYS(pthread_mutex_lock(&ri->lock), "lock failed");
        incoming_msg_t * im = f_malloc(sizeof(incoming_msg_t));
        im->msg = f_strdup(msg);
        im->msg_len = len;
        im->msg_ts = currentms();
        DL_APPEND(ri->queue, im);
        BOLT_SYS(pthread_cond_signal(&ri->cond), "cond signal");
        BOLT_SYS(pthread_mutex_unlock(&ri->lock), "unlock failed");

    }

    utarray_done(&ri_list);

    if (err) DBG("Error while appending message to queue for %s : %s", type, msg);

}


static int void_cmp(void const * a, void const * b) {

    void * const * ls = a;
    void * const * rs = b;

    return (*ls == *rs) ? 0 : ((uintptr_t)*ls > (uintptr_t)*rs) ? 1 : -1;
}


static void query_hash_tree(runner_info_hash_tree_t * current, runner_info_t * ri, const char * ua_type, int is_delete, UT_array * gather, int tip) {

    if (!current || (!ri && !gather)) { return; }

    while (*ua_type && (*ua_type == '/')) { ua_type++; }

    if (gather && utarray_len(&current->items) && (!tip || !*ua_type)) {
        utarray_concat(gather, &current->items);
    }

    if (!*ua_type) {

        if (is_delete) {
            //remove from array
            void * addr = utarray_find(&current->items, &ri, void_cmp);
            if (addr) {
                long idx = (long)utarray_eltidx(&current->items, addr);
                if (idx >= 0) {
                    utarray_erase(&current->items, idx, 1);
                }
            }

        } else if (!gather) {
            //add to array once
            if (!utarray_find(&current->items, &ri, void_cmp)) {
                utarray_push_back(&current->items, &ri);
                utarray_sort(&current->items, void_cmp);
            }
        }

        return;
    }

    char * ua_type_sep = strchr(ua_type, '/');
    size_t key_len = ua_type_sep ? (size_t)(ua_type_sep - ua_type) : strlen(ua_type);

    runner_info_hash_tree_t * child;
    HASH_FIND(hh, current->nodes, ua_type, key_len, child);
    if (!child) {
        if (is_delete) {
            DBG("sub-node not found to delete sub-tree %s", ua_type);
        } else if (!gather) {
            child = f_malloc(sizeof(runner_info_hash_tree_t));
            child->key = f_strndup(ua_type, key_len);
            child->parent = current;
            HASH_ADD_KEYPTR(hh, current->nodes, child->key, key_len, child);
            utarray_init(&child->items, &ut_ptr_icd);
        }
    }

    ua_type += key_len;

    if (child) {
        query_hash_tree(child, ri, ua_type, is_delete, gather, tip);
    }

    if (is_delete && !utarray_len(&child->items) && !HASH_COUNT(child->nodes)) {
        if (child->parent) {
            HASH_DEL(child->parent->nodes, child);
            free(child->key);
        }
        utarray_done(&child->items);
        free(child);
    }
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
                process_message(&info->unit, im->msg, im->msg_len);
            else
                DBG("message timed out: %s", im->msg);

            free(im->msg);
            free(im);
        }
    }

    return NULL;
}


static void process_message(ua_unit_t * ui, const char * msg, size_t len) {

    char * type;
    enum json_tokener_error jErr;

    json_object * jObj = json_tokener_parse_verbose(msg, &jErr);
    if(jErr == json_tokener_success) {
        if (get_type_from_json(jObj, &type) == E_UA_OK) {
            if (!ui->uar->on_message || !(*ui->uar->on_message)(type, jObj)) {
                if (!strcmp(type, QUERY_PACKAGE)) {
                    process_run(ui, process_query_package, jObj, 0);
                } else if (!strcmp(type, READY_DOWNLOAD)) {
                    process_run(ui, process_ready_download, jObj, 0);
                } else if (!strcmp(type, READY_UPDATE)) {
                    process_run(ui, process_ready_update, jObj, 1);
                } else if (!strcmp(type, PREPARE_UPDATE)) {
                    process_run(ui, process_prepare_update, jObj, 1);
                } else if (!strcmp(type, CONFIRM_UPDATE)) {
                    process_run(ui, process_confirm_update, jObj, 0);
                } else if (!strcmp(type, DOWNLOAD_REPORT)) {
                    process_run(ui, process_download_report, jObj, 0);
                } else if (!strcmp(type, SOTA_REPORT)) {
                    process_run(ui, process_sota_report, jObj, 0);
                } else if (!strcmp(type, LOG_REPORT)) {
                    process_run(ui, process_log_report, jObj, 0);
                } else {
                    DBG("Nothing to do for type %s : %s", type, json_object_to_json_string(jObj));
                }
            } else {
                DBG("Not processing message for type %s : %s", type, json_object_to_json_string(jObj));
            }
        }
    } else {
        DBG("Failed to parse json (%s): %s", json_tokener_error_desc(jErr), msg);
    }

    if (!jErr) json_object_put(jObj);
}


static void * worker_action(void * arg) {

    ua_unit_t * ui = arg;

    ui->worker.worker_func(ui->uar, ui->worker.worker_jobj);

    json_object_put(ui->worker.worker_jobj);

    ui->worker.worker_running = 0;

    return NULL;
}


static void process_run(ua_unit_t * ui, process_f func, json_object * jObj, int turnover) {

    if (!turnover) {

        func(ui->uar, jObj);

    } else {

        if (!ui->worker.worker_running) {
            ui->worker.worker_running = 1;
            ui->worker.worker_func = func;
            ui->worker.worker_jobj = json_object_get(jObj);

            if (pthread_create(&ui->worker.worker_thread, 0, worker_action, ui)) {

                DBG_SYS("pthread create");
                json_object_put(ui->worker.worker_jobj);
                ui->worker.worker_running = 0;
            }

        } else {
            DBG("error! Worker busy");
        }
    }
}


static void process_query_package(ua_routine_t * uar, json_object * jsonObj) {

    pkg_info_t pkgInfo = {0};
    char * installedVer = 0;
    char * replyId;

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_replyid_from_json(jsonObj, &replyId)) {

        if (!(*uar->on_get_version)(pkgInfo.type, pkgInfo.name, &installedVer)) {

            DBG("DMClient is querying version info of : %s Returning %s", pkgInfo.name, NULL_STR(installedVer));

            json_object * pkgObject = json_object_new_object();
            json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo.type));
            json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo.name));
            json_object_object_add(pkgObject, "version", S(installedVer) ? json_object_new_string(installedVer) : NULL);

            if (ua_intl.delta) {
                json_object_object_add(pkgObject, "delta-cap", json_object_new_string(get_delta_capability()));
            }

            if (S(ua_intl.backup_dir)) {

                pkg_file_t *pf, *aux, *pkgFile = NULL;
                char * pkgManifest = JOIN(ua_intl.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG);

                if (!parse_pkg_manifest(pkgManifest, &pkgFile)) {

                    json_object * verListObject = json_object_new_object();
                    json_object * rbVersArray = json_object_new_array();

                    DL_FOREACH_SAFE(pkgFile, pf, aux) {

                        json_object * versionObject = json_object_new_object();
                        json_object_object_add(versionObject, "file", json_object_new_string(pf->file));
                        json_object_object_add(versionObject, "downloaded", json_object_new_boolean(pf->downloaded? 1:0));

                        if (ua_intl.delta) {
                            json_object_object_add(versionObject, "sha-256", json_object_new_string(pf->sha256b64));
                        }

                        json_object_object_add(verListObject, pf->version, versionObject);
                        json_object_array_add(rbVersArray, json_object_new_string(pf->version));

                        DL_DELETE(pkgFile, pf);
                        free_pkg_file(pf);

                    }

                    json_object_object_add(pkgObject, "version-list", verListObject);
                    json_object_object_add(pkgObject, "rollback-versions", rbVersArray);
                }

                free (pkgManifest);
            }

            json_object * bodyObject = json_object_new_object();
            json_object_object_add(bodyObject, "package", pkgObject);

            json_object * jObject = json_object_new_object();
            json_object_object_add(jObject, "type", json_object_new_string(QUERY_PACKAGE));
            json_object_object_add(jObject, "reply-to", json_object_new_string(replyId));
            json_object_object_add(jObject, "body", bodyObject);

            ua_send_message(jObject);

            json_object_put(jObject);

        } else {
            DBG("get version for %s failed!", pkgInfo.name);
        }

    }
}


static void process_ready_download(ua_routine_t * uar, json_object * jsonObj) {

    pkg_info_t pkgInfo = {0};

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {

        prepare_download_action(uar, &pkgInfo);

    }
}


static void process_prepare_update(ua_routine_t * uar, json_object * jsonObj) {

    int bck = 0;
    pkg_info_t pkgInfo = {0};
    pkg_file_t pkgFile, updateFile = {0};
    update_err_t updateErr = UE_NONE;
    install_state_t state = INSTALL_READY;

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {

        get_pkg_rollback_version_from_json(jsonObj, &pkgInfo.rollback_version);

        char * pkgManifest = S(ua_intl.backup_dir) ? JOIN(ua_intl.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG) : NULL;
        pkgFile.version = S(pkgInfo.rollback_version) ? pkgInfo.rollback_version : pkgInfo.version;

        if ((!get_pkg_file_from_json(jsonObj, pkgFile.version, &pkgFile.file) &&
                !get_pkg_sha256_from_json(jsonObj, pkgFile.version, pkgFile.sha256b64) &&
                !get_pkg_downloaded_from_json(jsonObj, pkgFile.version, &pkgFile.downloaded)) ||
                ((!get_pkg_file_manifest(pkgManifest, pkgFile.version, &pkgFile)) && (bck = 1))) {

            state = prepare_install_action(uar, &pkgInfo, &pkgFile, bck, &updateFile, &updateErr);
            send_install_status(&pkgInfo, state, &pkgFile, updateErr);

            if (state == INSTALL_READY) {
                char * prePkgManifest = JOIN(ua_intl.cache_dir, pkgInfo.name, MANIFEST_PKG);

                if (!calc_sha256_x(updateFile.file, updateFile.sha256b64)) {
                    add_pkg_file_manifest(prePkgManifest, &updateFile);
                }

                free(prePkgManifest);
            }

            free(updateFile.version);
            free(updateFile.file);
        }

        if (pkgManifest) free(pkgManifest);
        if (bck) {
            free(pkgFile.version);
            free(pkgFile.file);
        }
    }
}


static void process_ready_update(ua_routine_t * uar, json_object * jsonObj) {

    pkg_info_t pkgInfo = {0};
    pkg_file_t pkgFile, updateFile = {0};
    update_err_t updateErr = UE_NONE;
    install_state_t state;
    int bck = 0;

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {

        get_pkg_rollback_version_from_json(jsonObj, &pkgInfo.rollback_version);
        get_pkg_rollback_versions_from_json(jsonObj, &pkgInfo.rollback_versions);

        char * pkgManifest = S(ua_intl.backup_dir) ? JOIN(ua_intl.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG) : NULL;
        char * prePkgManifest = JOIN(ua_intl.cache_dir, pkgInfo.name, MANIFEST_PKG);

        do {
            pkgFile.version = S(pkgInfo.rollback_version) ? pkgInfo.rollback_version : pkgInfo.version;

            if ((!get_pkg_file_from_json(jsonObj, pkgFile.version, &pkgFile.file) &&
                    !get_pkg_sha256_from_json(jsonObj, pkgFile.version, pkgFile.sha256b64) &&
                    !get_pkg_downloaded_from_json(jsonObj, pkgFile.version, &pkgFile.downloaded)) ||
                    ((!get_pkg_file_manifest(pkgManifest, pkgFile.version, &pkgFile)) && (bck = 1))) {

                if (S(pkgInfo.rollback_version)) {
                    send_install_status(&pkgInfo, state = INSTALL_ROLLBACK, &(pkg_file_t){.version = pkgInfo.rollback_version, .downloaded = 1}, UE_NONE);
                }

                if (!get_pkg_file_manifest(prePkgManifest, pkgFile.version, &updateFile) ||
                        (state = prepare_install_action(uar, &pkgInfo, &pkgFile, bck, &updateFile, &updateErr)) == INSTALL_READY) {

                    if ((state = pre_update_action(uar, &pkgInfo, &updateFile)) == INSTALL_IN_PROGRESS) {

                        state = update_action(uar, &pkgInfo, &updateFile);

                        post_update_action(uar, &pkgInfo);

                    }

                    free(updateFile.file);
                    free(updateFile.version);

                } else if ((state == INSTALL_FAILED) && (updateErr != UE_NONE)) break;

                if (bck) {
                    bck = 0;
                    free(pkgFile.version);
                    free(pkgFile.file);
                }

            } else {
                if (S(pkgInfo.rollback_version)) {
                    send_install_status(&pkgInfo, state = INSTALL_ROLLBACK, &(pkg_file_t){.version = pkgInfo.rollback_version, .downloaded = 0}, UE_NONE);
                    break;
                }
            }

        } while ((state == INSTALL_FAILED) &&
                !get_pkg_next_rollback_version(pkgInfo.rollback_versions, updateFile.version, &pkgInfo.rollback_version) &&
                S(pkgInfo.rollback_version));

        if ((state == INSTALL_FAILED) && ((updateErr != UE_NONE) ||
                (pkgInfo.rollback_versions && (updateErr = UE_TERMINAL_FAILURE)))) {
            send_install_status(&pkgInfo, INSTALL_FAILED, &updateFile, updateErr);
        }

        free(pkgManifest);
        free(prePkgManifest);
    }
}


static int patch_delta(char * pkgManifest, char * version, char * diffFile, char * newFile) {

    int err = E_UA_OK;
    pkg_file_t * pkgFile = f_malloc(sizeof(pkg_file_t));

    if ((get_pkg_file_manifest(pkgManifest, version, pkgFile)) ||
            delta_reconstruct(pkgFile->file, diffFile, newFile)) {

        err = E_UA_ERR;

    }

    free_pkg_file(pkgFile);

    return err;
}


static void process_confirm_update(ua_routine_t * uar, json_object * jsonObj) {

    pkg_info_t pkgInfo = {0};
    pkg_file_t pkgFile, updateFile = {0};

    if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
            !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
            !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {

        if ((!get_pkg_file_from_json(jsonObj, pkgInfo.version, &pkgFile.file) &&
                !get_pkg_sha256_from_json(jsonObj, pkgInfo.version, pkgFile.sha256b64) &&
                !get_pkg_downloaded_from_json(jsonObj, pkgInfo.version, &pkgFile.downloaded))) {
            //err! backing up only when file property exists due to unnecessary confirm update from dmclient

            char * prePkgManifest = JOIN(ua_intl.cache_dir, pkgInfo.name, MANIFEST_PKG);

            if (!get_pkg_file_manifest(prePkgManifest, pkgInfo.version, &updateFile)) {

                backup_package(&pkgInfo, &updateFile);

                free(updateFile.file);
                free(updateFile.version);
            }

            free(prePkgManifest);
        }
    }
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


static install_state_t prepare_install_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile, int bck, pkg_file_t * updateFile, update_err_t * ue) {

    install_state_t state = INSTALL_READY;
    char *newFile = 0;
    char *installedVer = 0;

    updateFile->version = f_strdup(pkgFile->version);

    if (!bck && uar->on_transfer_file) {
        transfer_file_action(uar, pkgInfo, pkgFile);
    }

    if (ua_intl.delta && is_delta_package(pkgFile->file)) {

        char * pkgManifest = S(ua_intl.backup_dir) ? JOIN(ua_intl.backup_dir, "backup", pkgInfo->name, MANIFEST_PKG) : NULL;

        char * bname = f_basename(pkgFile->file);
        updateFile->file = JOIN(ua_intl.cache_dir, "delta", bname);
        free(bname);

        if ((*uar->on_get_version)(pkgInfo->type, pkgInfo->name, &installedVer)) {
            DBG("get version for %s failed!", pkgInfo->name);
        }

        if (patch_delta(pkgManifest, installedVer, pkgFile->file, updateFile->file)) {

            *ue = UE_INCREMENTAL_FAILED;
            state = INSTALL_FAILED;
        }

        if (pkgManifest) free(pkgManifest);
        updateFile->downloaded = 0;

    } else {

        updateFile->file = f_strdup(pkgFile->file);
        updateFile->downloaded = 1;
    }

    if (state != INSTALL_FAILED) {

        if (uar->on_prepare_install) {
            state = (*uar->on_prepare_install)(pkgInfo->type, pkgInfo->name, updateFile->version, updateFile->file, &newFile);
            if (S(newFile)) {
                free(updateFile->file);
                updateFile->file = f_strdup(newFile);
            }
        }

    }

    return state;
}


static int transfer_file_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile) {

    int ret = E_UA_OK;
    char *newFile = 0;

    if (uar->on_transfer_file) {
        ret = (*uar->on_transfer_file)(pkgInfo->type, pkgInfo->name, pkgFile->version, pkgFile->file, &newFile);
        if (!ret && S(newFile)) {
            pkgFile->file = newFile;
        } else {
            //TODO: send transfer failed
        }
    }

    return ret;
}


static download_state_t prepare_download_action(ua_routine_t * uar, pkg_info_t * pkgInfo) {

    download_state_t state = DOWNLOAD_CONSENT;

    if (uar->on_prepare_download) {
        state = (*uar->on_prepare_download)(pkgInfo->type, pkgInfo->name, pkgInfo->version);
        send_download_status(pkgInfo, state);
    }

    return state;
}


static install_state_t pre_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile) {

    install_state_t state = INSTALL_IN_PROGRESS;

    if (uar->on_pre_install) {
        state = (*uar->on_pre_install)(pkgInfo->type, pkgInfo->name, pkgFile->version, pkgFile->file);
    }

    send_install_status(pkgInfo, state, 0, 0);

    return state;
}


static install_state_t update_action(ua_routine_t * uar, pkg_info_t * pkgInfo, pkg_file_t * pkgFile) {

    install_state_t state = (*uar->on_install)(pkgInfo->type, pkgInfo->name, pkgFile->version, pkgFile->file);

    if ((state == INSTALL_COMPLETED) && uar->on_set_version) {
        if ((*uar->on_set_version)(pkgInfo->type, pkgInfo->name, pkgFile->version))
            DBG("set version for %s failed!", pkgInfo->name);
    }

    if (!(pkgInfo->rollback_versions && (state == INSTALL_FAILED))) {
        send_install_status(pkgInfo, state, 0, 0);
    }

    return state;
}


static void post_update_action(ua_routine_t * uar, pkg_info_t * pkgInfo) {

    if (uar->on_post_install) {
        (*uar->on_post_install)(pkgInfo->type, pkgInfo->name);
    }

}


static void send_install_status(pkg_info_t * pkgInfo, install_state_t state, pkg_file_t * pkgFile, update_err_t ue) {

    json_object * pkgObject = json_object_new_object();
    json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
    json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo->type));
    json_object_object_add(pkgObject, "version", json_object_new_string(pkgInfo->version));
    json_object_object_add(pkgObject, "status", json_object_new_string(install_state_string(state)));
    if (pkgInfo->rollback_version)  json_object_object_add(pkgObject, "rollback-version", json_object_new_string(pkgInfo->rollback_version));
    if (pkgInfo->rollback_versions) json_object_object_add(pkgObject, "rollback-versions", json_object_get(pkgInfo->rollback_versions));


    if ((state == INSTALL_FAILED) && (ue != UE_NONE) && pkgFile) {

        if (ue == UE_TERMINAL_FAILURE) {
            json_object_object_add(pkgObject, "terminal-failure", json_object_new_boolean(1));
        }
        if (ue == UE_UPDATE_INCAPABLE) {
            json_object_object_add(pkgObject, "update-incapable", json_object_new_boolean(1));
        }
        if (ue == UE_INCREMENTAL_FAILED) {
            json_object * versionObject = json_object_new_object();
            json_object * verListObject = json_object_new_object();
            json_object_object_add(versionObject, "incremental-failed", json_object_new_boolean(1));
            json_object_object_add(verListObject, pkgFile->version, versionObject);
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

    ua_send_message(jObject);

    json_object_put(jObject);

}


static void send_download_status(pkg_info_t * pkgInfo, download_state_t state) {

    json_object * pkgObject = json_object_new_object();
    json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
    json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo->type));
    json_object_object_add(pkgObject, "version", json_object_new_string(pkgInfo->version));
    json_object_object_add(pkgObject, "status", json_object_new_string(download_state_string(state)));

    json_object * bodyObject = json_object_new_object();
    json_object_object_add(bodyObject, "package", pkgObject);

    json_object * jObject = json_object_new_object();
    json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
    json_object_object_add(jObject, "body", bodyObject);

    ua_send_message(jObject);

    json_object_put(jObject);
}


static int send_update_report(const char * pkgName, const char * version, int indeterminate, int percent, update_stage_t us) {

    int err = E_UA_OK;

    do {
        BOLT_IF(!S(pkgName) || !S(version) || (percent < 0) || (percent > 100), E_UA_ARG, "install progress invalid");
        json_object * pkgObject = json_object_new_object();
        json_object_object_add(pkgObject, "name", json_object_new_string(pkgName));
        json_object_object_add(pkgObject, "version", json_object_new_string(version));

        json_object * bodyObject = json_object_new_object();
        json_object_object_add(bodyObject, "package", pkgObject);
        json_object_object_add(bodyObject, "progress", json_object_new_int(percent));
        json_object_object_add(bodyObject, "indeterminate", json_object_new_boolean(indeterminate ? 1:0));
        json_object_object_add(bodyObject, "stage", json_object_new_string(update_stage_string(us)));

        json_object * jObject = json_object_new_object();
        json_object_object_add(jObject, "type", json_object_new_string(UPDATE_REPORT));
        json_object_object_add(jObject, "body", bodyObject);

        err = ua_send_message(jObject);

        json_object_put(jObject);

    } while (0);

    return err;
}


static int backup_package(pkg_info_t * pkgInfo, pkg_file_t * pkgFile) {

    int err = E_UA_OK;
    pkg_file_t * backupFile = f_malloc(sizeof(pkg_file_t));

    char * bname = f_basename(pkgFile->file);
    char * pkgDir = JOIN(ua_intl.backup_dir, "backup", pkgInfo->name);
    char * pkgManifest = JOIN(ua_intl.backup_dir, "backup", pkgInfo->name, MANIFEST_PKG);
    backupFile->file = JOIN(ua_intl.backup_dir, "backup", pkgInfo->name, pkgFile->version, bname);
    backupFile->version = f_strdup(pkgFile->version);
    backupFile->downloaded = 1;
    strcpy(backupFile->sha256b64, pkgFile->sha256b64);

    if (!strcmp(pkgFile->file, backupFile->file)) {

        DBG("Back up already exists: %s", backupFile->file);

    } else if (!copy_file(pkgFile->file, backupFile->file) &&
            !add_pkg_file_manifest(pkgManifest, backupFile)) {

        DBG("Backed up package: %s", backupFile->file);

    } else {

        DBG("Backing up failed: %s", backupFile->file);
        err = E_UA_ERR;

    }

    //Todo: limit the number of backups to avoid running out of space

    free(bname);
    free(pkgDir);
    free(pkgManifest);
    free_pkg_file(backupFile);

    return err;
}


static char * install_state_string(install_state_t state) {

    char * str = NULL;
    switch (state) {
        case INSTALL_READY       : str = "INSTALL_READY";       break;
        case INSTALL_IN_PROGRESS : str = "INSTALL_IN_PROGRESS"; break;
        case INSTALL_COMPLETED   : str = "INSTALL_COMPLETED";   break;
        case INSTALL_FAILED      : str = "INSTALL_FAILED";      break;
        case INSTALL_ABORTED     : str = "INSTALL_ABORTED";     break;
        case INSTALL_ROLLBACK    : str = "INSTALL_ROLLBACK";    break;
    }

    return str;
}


static char * download_state_string(download_state_t state) {

    char * str = NULL;
    switch (state) {
        case DOWNLOAD_POSTPONED  : str = "DOWNLOAD_POSTPONED"; break;
        case DOWNLOAD_CONSENT    : str = "DOWNLOAD_CONSENT";   break;
        case DOWNLOAD_DENIED     : str = "DOWNLOAD_DENIED";    break;
    }

    return str;
}

static char * update_stage_string(update_stage_t stage) {

    char * str = NULL;
    switch (stage) {
        case US_TRANSFER   : str = "US_TRANSFER"; break;
        case US_INSTALL    : str = "US_INSTALL";   break;
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


void free_pkg_file(pkg_file_t * pkgFile) {

    f_free(pkgFile->version);
    f_free(pkgFile->file);
    f_free(pkgFile);

}


const char * ua_get_updateagent_version() {

    return BUILD_VERSION;

}

const char * ua_get_xl4bus_version() {

    return xl4bus_get_version();

}
