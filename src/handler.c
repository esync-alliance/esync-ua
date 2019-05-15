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
#include "updater.h"

static void process_message(ua_component_context_t* uacc, const char* msg, size_t len);
static void process_run(ua_component_context_t* uacc, process_f func, json_object* jObj, int turnover);
static void process_query_package(ua_component_context_t* uacc, json_object* jsonObj);
static void process_ready_download(ua_component_context_t* uacc, json_object* jsonObj);
static void process_prepare_update(ua_component_context_t* uacc, json_object* jsonObj);
static void process_ready_update(ua_component_context_t* uacc, json_object* jsonObj);
static void process_confirm_update(ua_component_context_t* uacc, json_object* jsonObj);
static void process_download_report(ua_component_context_t* uacc, json_object* jsonObj);
static void process_sota_report(ua_component_context_t* uacc, json_object* jsonObj);
static void process_log_report(ua_component_context_t* uacc, json_object* jsonObj);
static void process_update_status(ua_component_context_t* uacc, json_object* jsonObj);
static download_state_t prepare_download_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo);
static int transfer_file_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile);
static void send_download_status(pkg_info_t* pkgInfo, download_state_t state);
static int send_update_report(const char* pkgName, const char* version, int indeterminate, int percent, update_stage_t us);
static int send_current_report_version(ua_component_context_t* uacc, pkg_info_t* pkgInfo, char* report_version);
static int backup_package(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile);
static int patch_delta(char* pkgManifest, char* version, char* diffFile, char* newFile);
static char* install_state_string(install_state_t state);
static char* download_state_string(download_state_t state);
static char* update_stage_string(update_stage_t stage);
static char* log_type_string(log_type_t log);
void* runner_loop(void* arg);

int ua_debug          = 1;
ua_internal_t ua_intl = {0};
runner_info_hash_tree_t* ri_tree = NULL;

#ifdef HAVE_INSTALL_LOG_HANDLER
ua_log_handler_f ua_log_handler = 0;

void ua_install_log_handler(ua_log_handler_f handler)
{
	ua_log_handler = handler;
}
#endif

int ua_init(ua_cfg_t* uaConfig)
{
	int err = E_UA_OK;

	do {
		ua_debug = uaConfig->debug + 1;

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
		ua_intl.delta          = uaConfig->delta;
		ua_intl.reboot_support = uaConfig->reboot_support;
		ua_intl.cache_dir      = S(uaConfig->cache_dir) ? f_strdup(uaConfig->cache_dir) : NULL;
		ua_intl.backup_dir     = S(uaConfig->backup_dir) ? f_strdup(uaConfig->backup_dir) : NULL;
		ua_intl.record_file    = S(ua_intl.backup_dir) ? JOIN(ua_intl.backup_dir, "backup", UPDATE_REC_FILE) : NULL;
		BOLT_SUB(xl4bus_client_init(uaConfig->url, uaConfig->cert_dir));

		ua_intl.state = UAI_STATE_INITIALIZED;

		if (uaConfig->rw_buffer_size) ua_rw_buff_size = uaConfig->rw_buffer_size * 1024;

	} while (0);

	if (err) {
		ua_stop();
	}

	return err;
}


int ua_stop()
{
	if (ua_intl.cache_dir) { free(ua_intl.cache_dir); ua_intl.cache_dir = NULL; }
	if (ua_intl.backup_dir) { free(ua_intl.backup_dir); ua_intl.backup_dir = NULL; }
	if (ua_intl.record_file) { free(ua_intl.record_file); ua_intl.record_file = NULL; }
	delta_stop();
	if (ua_intl.state >= UAI_STATE_INITIALIZED)
		return xl4bus_client_stop();

	return 0;
}


int ua_register(ua_handler_t* uah, int len)
{
	int err           = E_UA_OK;
	runner_info_t* ri = 0;

	ua_intl.uah   = uah;
	ua_intl.n_uah = len;

	if (!ri_tree) {
		ri_tree = f_malloc(sizeof(runner_info_hash_tree_t));
		utarray_init(&ri_tree->items, &ut_ptr_icd);
	}

	for (int i = 0; i < len; i++) {
		do {
			ri                  = f_malloc(sizeof(runner_info_t));
			ri->component.type  = f_strdup((uah + i)->type_handler);
			ri->component.uar   = (*(uah + i)->get_routine)();
			ri->component.state = UA_STATE_IDLE_INIT;
			ri->run             = 1;
			BOLT_IF(!ri->component.uar || !ri->component.uar->on_get_version || !ri->component.uar->on_install || !S(ri->component.type), E_UA_ARG, "registration error");
			BOLT_SYS(pthread_mutex_init(&ri->lock, 0), "lock init");
			BOLT_SYS(pthread_cond_init(&ri->cond, 0), "cond init");
			BOLT_SYS(pthread_create(&ri->thread, 0, runner_loop, ri), "pthread create");
			query_hash_tree(ri_tree, ri, ri->component.type, 0, 0, 0);

			BOLT_SYS(pthread_mutex_init(&ri->component.update_status_info.lock, NULL), "update status lock init");
			BOLT_SYS(pthread_cond_init(&ri->component.update_status_info.cond, 0), "update status cond init");
			ri->component.update_status_info.reply_id = NULL;
			ri->component.record_file                 = ua_intl.record_file;
			DBG("Registered: %s", ri->component.type);
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


int ua_unregister(ua_handler_t* uah, int len)
{
	int ret = E_UA_OK;
	UT_array ri_list;

	for (int i = 0; i < len; i++) {
		const char* type  = (uah + i)->type_handler;
		ua_routine_t* uar = (*(uah + i)->get_routine)();

		utarray_init(&ri_list, &ut_ptr_icd);
		query_hash_tree(ri_tree, 0, type, 0, &ri_list, 1);

		int l = utarray_len(&ri_list);
		for (int j = 0; j < l; j++) {
			int err = E_UA_OK;
			do {
				runner_info_t* ri = *(runner_info_t**) utarray_eltptr(&ri_list, j);
				if (ri->component.uar == uar) {
					query_hash_tree(ri_tree, ri, type, 1, 0, 0);
					ri->run = 0;
					BOLT_SYS(pthread_cond_broadcast(&ri->cond), "cond broadcast");
					BOLT_SYS(pthread_mutex_unlock(&ri->lock), "lock unlock");
					BOLT_SYS(pthread_cond_destroy(&ri->cond), "cond destroy");
					BOLT_SYS(pthread_mutex_destroy(&ri->lock), "lock destroy");
					ri->component.record_file = NULL;
					f_free(ri->component.update_manifest);
					f_free(ri->component.backup_manifest);
					f_free(ri->component.type);
					f_free(ri);
					DBG("Unregistered: %s", type);
				}

				if (ri->component.update_status_info.reply_id) {
					free(ri->component.update_status_info.reply_id);
					ri->component.update_status_info.reply_id = NULL;
				}

			} while (0);

			if (err) {
				DBG("Failed to unregister: %s", type);
				ret = err;
			}
		}

		utarray_done(&ri_list);
	}


	if (ri_tree) {
		free(ri_tree);
		ri_tree = NULL;
	}
	return ret;
}

void* ua_handle_dmc_presence(void* arg)
{
	if (ua_intl.uah) {
		for (int i = 0; i < ua_intl.n_uah; i++) {
			ua_routine_t* uar = (*(ua_intl.uah + i)->get_routine)();
			if (uar && uar->on_dmc_presence)
				(*uar->on_dmc_presence)(NULL);

		}
	}

	return NULL;
}

int ua_send_message(json_object* jsonObj)
{
	char* msg = (char*)json_object_to_json_string(jsonObj);

	DBG("Sending to DMC : %s", msg);
	return xl4bus_client_send_msg(msg);

}

XL4_PUB int ua_send_message_with_address(json_object* jsonObj, xl4bus_address_t* xl4_address)
{
	char* msg = (char*)json_object_to_json_string(jsonObj);

	DBG("Sending with bus addr: %s", msg);
	return xl4bus_client_send_msg_to_addr(msg, xl4_address);
}

int ua_send_install_progress(const char* pkgName, const char* version, int indeterminate, int percent)
{
	return send_update_report(pkgName, version, indeterminate, percent, US_INSTALL);

}


int ua_send_transfer_progress(const char* pkgName, const char* version, int indeterminate, int percent)
{
	return send_update_report(pkgName, version, indeterminate, percent, US_TRANSFER);

}


int ua_send_log_report(char* pkgType, log_type_t logtype, log_data_t* logdata)
{
	int err            = E_UA_OK;
	char timestamp[64] = {0};

	do {
		BOLT_IF(!S(pkgType) || !(logdata->message || logdata->binary), E_UA_ARG, "log report invalid");

		if (S(logdata->timestamp)) {
			strncpy(timestamp, logdata->timestamp, sizeof(timestamp));
		} else {
			time_t t          = time(NULL);
			struct tm* cur_tm = gmtime(&t);
			if (cur_tm)
				strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", cur_tm);
		}

		json_object* bodyObject = json_object_new_object();
		json_object_object_add(bodyObject, "type", json_object_new_string(pkgType));
		json_object_object_add(bodyObject, "level", json_object_new_string(log_type_string(logtype)));
		json_object_object_add(bodyObject, "timestamp", json_object_new_string(timestamp));

		if (logdata->message) {
			json_object_object_add(bodyObject, "message", json_object_get((json_object*)logdata->message));
			json_object_object_add(bodyObject, "compound", json_object_new_boolean(logdata->compound ? 1 : 0));
		} else if (logdata->binary) {
			json_object_object_add(bodyObject, "binary", json_object_new_string(logdata->binary));
		}

		json_object* jObject = json_object_new_object();
		json_object_object_add(jObject, "type", json_object_new_string(LOG_REPORT));
		json_object_object_add(jObject, "body", bodyObject);

		err = ua_send_message(jObject);

		json_object_put(jObject);

	} while (0);

	return err;
}

void handle_status(int status)
{
	char* sts_str[] = {
		"RUNNING",
		"RESOLUTION_FAILED",
		"CONNECTION_FAILED",
		"REGISTRATION_FAILED",
		"CONNECTION_BROKE",
	};

	DBG("eSync Bus Status (%d): %s", status, status < sizeof(sts_str)/sizeof(sts_str[0]) ? sts_str[status] : NULL);
	ua_intl.esync_bus_conn_status = status;
}


void handle_delivered(const char* msg, int ok)
{
	char* msg_type = 0, * pkg_type = 0;
	enum json_tokener_error jErr;

	if (msg) {
		json_object* jObj = json_tokener_parse_verbose(msg, &jErr);
		if (jObj) {
			get_pkg_type_from_json(jObj, &pkg_type);
			get_type_from_json(jObj, &msg_type);
			if (!ok)
				DBG("Message delivered %s : %s for %s", ok ? "OK" : "NOT OK", NULL_STR(msg_type), NULL_STR(pkg_type));
			json_object_put(jObj);
		}
	}
}


void handle_presence(int connected, int disconnected, esync_bus_conn_state_t conn)
{
	DBG("Connected : %d,  Disconnected : %d", connected, disconnected);
	switch (conn)
	{
		case BUS_CONN_BROKER_NOT_CONNECTED:
			break;
		case BUS_CONN_BROKER_CONNECTED:
			break;
		case BUS_CONN_DMC_NOT_CONNECTED:
			break;
		case BUS_CONN_DMC_CONNECTED:
			if (ua_intl.reboot_support && ua_intl.state < UAI_STATE_RESUME_STARTED)
				update_handle_resume_from_reboot(ua_intl.record_file, ri_tree);

			pthread_t thread_dmc_presence;
			if (pthread_create(&thread_dmc_presence, 0, ua_handle_dmc_presence, NULL)) {
				DBG("Failed to spawn thread_dmc_presence.");

			}
			break;
		default:
			break;

	}

}


void handle_message(const char* type, const char* msg, size_t len)
{
	int err = E_UA_OK;
	UT_array ri_list;

	DBG("Incoming message : %s", msg);

	utarray_init(&ri_list, &ut_ptr_icd);
	query_hash_tree(ri_tree, 0, type, 0, &ri_list, 0);

	int l = utarray_len(&ri_list);
#if 0
	if (l)
		DBG("Registered handlers found for %s : %d", type, l);
	else
		DBG("Incoming message for non-registered handler %s : %s", type, msg);
#endif
	for (int j = 0; j < l; j++) {
		runner_info_t* ri = *(runner_info_t**) utarray_eltptr(&ri_list, j);
		BOLT_SYS(pthread_mutex_lock(&ri->lock), "lock failed");
		incoming_msg_t* im = f_malloc(sizeof(incoming_msg_t));
		im->msg     = f_strdup(msg);
		im->msg_len = len;
		im->msg_ts  = currentms();
		DL_APPEND(ri->queue, im);
		BOLT_SYS(pthread_cond_signal(&ri->cond), "cond signal");
		BOLT_SYS(pthread_mutex_unlock(&ri->lock), "unlock failed");

	}

	utarray_done(&ri_list);

	if (err) DBG("Error while appending message to queue for %s : %s", type, msg);

}

static int void_cmp(void const* a, void const* b)
{
	void* const* ls = a;
	void* const* rs = b;

	return (*ls == *rs) ? 0 : ((uintptr_t)*ls > (uintptr_t)*rs) ? 1 : -1;
}


void query_hash_tree(runner_info_hash_tree_t* current, runner_info_t* ri, const char* ua_type, int is_delete, UT_array* gather, int tip)
{
	if (!current || (!ri && !gather)) { return; }

	while (*ua_type && (*ua_type == '/')) { ua_type++; }

	if (gather && utarray_len(&current->items) && (!tip || !*ua_type)) {
		utarray_concat(gather, &current->items);
	}

	if (!*ua_type) {
		if (is_delete) {
			//remove from array
			void* addr = utarray_find(&current->items, &ri, void_cmp);
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

	char* ua_type_sep = strchr(ua_type, '/');
	size_t key_len    = ua_type_sep ? (size_t)(ua_type_sep - ua_type) : strlen(ua_type);

	runner_info_hash_tree_t* child;
	HASH_FIND(hh, current->nodes, ua_type, key_len, child);
	if (!child) {
		if (is_delete) {
			DBG("sub-node not found to delete sub-tree %s", ua_type);
		} else if (!gather) {
			child         = f_malloc(sizeof(runner_info_hash_tree_t));
			child->key    = f_strndup(ua_type, key_len);
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


void* runner_loop(void* arg)
{
	runner_info_t* info = arg;

	while (info->run) {
		incoming_msg_t* im;
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
				process_message(&info->component, im->msg, im->msg_len);
			else
				DBG("message timed out: %s", im->msg);

			free(im->msg);
			free(im);
		}
	}

	return NULL;
}

int get_local_next_rollback_version(char* manifest, char* currentVer, char** nextVer)
{
	int err                  = E_UA_OK;
	pkg_file_t* pf           = NULL, * aux = NULL, * pkgFile = NULL;
	json_object* rbVersArray = json_object_new_array();

	char* tmp_next_version = NULL;

	if (rbVersArray && manifest && !parse_pkg_manifest(manifest, &pkgFile)) {
		DL_FOREACH_SAFE(pkgFile, pf, aux) {
			json_object_array_add(rbVersArray, json_object_new_string(pf->version));
			free_pkg_file(pf);
		}
		err      = get_pkg_next_rollback_version(rbVersArray, currentVer, &tmp_next_version);
		*nextVer = f_strdup(tmp_next_version);

	} else{
		err = E_UA_ERR;
	}

	if (rbVersArray != NULL)
		json_object_put(rbVersArray);

	return err;
}



static void process_message(ua_component_context_t* uacc, const char* msg, size_t len)
{
	char* type;
	enum json_tokener_error jErr;

	json_object* jObj = json_tokener_parse_verbose(msg, &jErr);

	if (jErr == json_tokener_success) {
		if (get_type_from_json(jObj, &type) == E_UA_OK) {
			if (!uacc->uar->on_message || !(*uacc->uar->on_message)(type, jObj)) {
				if (!strcmp(type, QUERY_PACKAGE)) {
					process_run(uacc, process_query_package, jObj, 0);
				} else if (!strcmp(type, READY_DOWNLOAD)) {
					process_run(uacc, process_ready_download, jObj, 0);
				} else if (!strcmp(type, READY_UPDATE)) {
					process_run(uacc, process_ready_update, jObj, 1);
				} else if (!strcmp(type, PREPARE_UPDATE)) {
					process_run(uacc, process_prepare_update, jObj, 1);
				} else if (!strcmp(type, CONFIRM_UPDATE)) {
					process_run(uacc, process_confirm_update, jObj, 0);
				} else if (!strcmp(type, DOWNLOAD_REPORT)) {
					process_run(uacc, process_download_report, jObj, 0);
				} else if (!strcmp(type, SOTA_REPORT)) {
					process_run(uacc, process_sota_report, jObj, 0);
				} else if (!strcmp(type, LOG_REPORT)) {
					process_run(uacc, process_log_report, jObj, 0);
				} else if (!strcmp(type, UPDATE_STATUS)) {
					process_run(uacc, process_update_status, jObj, 0);
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


static void* worker_action(void* arg)
{
	ua_component_context_t* uacc = arg;

	uacc->worker.worker_func(uacc, uacc->worker.worker_jobj);

	json_object_put(uacc->worker.worker_jobj);

	uacc->worker.worker_running = 0;

	return NULL;
}

static void process_run(ua_component_context_t* uacc, process_f func, json_object* jObj, int turnover)
{
	if (!turnover) {
		func(uacc, jObj);

	} else {
		if (!uacc->worker.worker_running) {
			uacc->worker.worker_running = 1;
			uacc->worker.worker_func    = func;
			uacc->worker.worker_jobj    = json_object_get(jObj);

			if (pthread_create(&uacc->worker.worker_thread, 0, worker_action, uacc)) {
				DBG_SYS("pthread create");
				json_object_put(uacc->worker.worker_jobj);
				uacc->worker.worker_running = 0;
			}

		} else {
			DBG("UA worker busy, discarding this message.");
		}
	}
}


static void process_query_package(ua_component_context_t* uacc, json_object* jsonObj)
{
	ua_routine_t* uar  = (uacc != NULL) ? uacc->uar : NULL;
	pkg_info_t pkgInfo = {0};
	char* installedVer = NULL;
	char* replyId      = NULL;
	int uae            = E_UA_OK;

	if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
	    !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
	    !get_replyid_from_json(jsonObj, &replyId)) {
		json_object* bodyObject = json_object_new_object();
		json_object* pkgObject  = json_object_new_object();

		if (uacc->state == UA_STATE_READY_UPDATE_STARTED) {
			json_object_object_add(bodyObject, "do-not-disturb", json_object_new_boolean(1));
		}else {
			uae = (*uar->on_get_version)(pkgInfo.type, pkgInfo.name, &installedVer);
			if (uae == E_UA_OK)
				DBG("DMClient is querying version info of : %s Returning %s", pkgInfo.name, NULL_STR(installedVer));
			else
				DBG("get version for %s failed! err=%d", pkgInfo.name, uae);

			json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo.type));
			json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo.name));
			json_object_object_add(pkgObject, "version", S(installedVer) ? json_object_new_string(installedVer) : NULL);

			if (ua_intl.delta) {
				json_object_object_add(pkgObject, "delta-cap", S(get_delta_capability()) ? json_object_new_string(get_delta_capability()) : NULL);
			}

			if (uae != E_UA_OK) {
				json_object_object_add(pkgObject, "update-incapable", json_object_new_boolean(1));
			}

			if (S(ua_intl.backup_dir)) {
				pkg_file_t* pf, * aux, * pkgFile = NULL;

				if (uacc->backup_manifest == NULL)
					uacc->backup_manifest = JOIN(ua_intl.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG);

				if (uacc->backup_manifest != NULL) {
					if (!parse_pkg_manifest(uacc->backup_manifest, &pkgFile)) {
						json_object* verListObject = json_object_new_object();
						json_object* rbVersArray   = json_object_new_array();

						DL_FOREACH_SAFE(pkgFile, pf, aux) {
							json_object* versionObject = json_object_new_object();

							json_object_object_add(versionObject, "file", json_object_new_string(pf->file));
							json_object_object_add(versionObject, "downloaded", json_object_new_boolean(pf->downloaded ? 1 : 0));
							json_object_object_add(versionObject, "rollback-order", json_object_new_int(pf->rollback_order));

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

				}

			}

			json_object_object_add(bodyObject, "package", pkgObject);

		}

		json_object* jObject = json_object_new_object();
		json_object_object_add(jObject, "type", json_object_new_string(QUERY_PACKAGE));
		json_object_object_add(jObject, "reply-to", json_object_new_string(replyId));
		json_object_object_add(jObject, "body", bodyObject);

		ua_send_message(jObject);

		json_object_put(jObject);

	}
}


static void process_ready_download(ua_component_context_t* uacc, json_object* jsonObj)
{
	pkg_info_t pkgInfo = {0};

	if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
	    !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
	    !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {
		prepare_download_action(uacc, &pkgInfo);

	}
}


static void process_prepare_update(ua_component_context_t* uacc, json_object* jsonObj)
{
	int bck            = 0;
	pkg_info_t pkgInfo = {0};
	pkg_file_t pkgFile, updateFile = {0};
	update_err_t updateErr = UE_NONE;
	install_state_t state  = INSTALL_READY;

	if (uacc->state != UA_STATE_PREPARE_UPDATE_STARTED &&
	    !get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
	    !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
	    !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {
		get_pkg_rollback_version_from_json(jsonObj, &pkgInfo.rollback_version);

		if (uacc->backup_manifest == NULL)
			uacc->backup_manifest = S(ua_intl.backup_dir) ? JOIN(ua_intl.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG) : NULL;

		pkgFile.version = S(pkgInfo.rollback_version) ? pkgInfo.rollback_version : pkgInfo.version;
		uacc->state     = UA_STATE_PREPARE_UPDATE_STARTED;

		if ((!get_pkg_file_from_json(jsonObj, pkgFile.version, &pkgFile.file) &&
		     !get_pkg_sha256_from_json(jsonObj, pkgFile.version, pkgFile.sha256b64) &&
		     !get_pkg_downloaded_from_json(jsonObj, pkgFile.version, &pkgFile.downloaded)) ||
		    ((!get_pkg_file_manifest(uacc->backup_manifest, pkgFile.version, &pkgFile)) && (bck = 1))) {
			state = prepare_install_action(uacc, &pkgInfo, &pkgFile, bck, &updateFile, &updateErr);

			send_install_status(&pkgInfo, state, &pkgFile, updateErr);

			if (state == INSTALL_READY) {
				if (uacc->update_manifest == NULL)
					uacc->update_manifest = JOIN(ua_intl.cache_dir, pkgInfo.name, MANIFEST_PKG);

				if (uacc->update_manifest != NULL) {
					if (!calc_sha256_x(updateFile.file, updateFile.sha256b64)) {
						add_pkg_file_manifest(uacc->update_manifest, &updateFile);
					}
				}
			}

			free(updateFile.version);
			free(updateFile.file);
		}

		if (bck) {
			free(pkgFile.version);
			free(pkgFile.file);
		}

		uacc->state = UA_STATE_PREPARE_UPDATE_DONE;
	}
}

static void process_ready_update(ua_component_context_t* uacc, json_object* jsonObj)
{
	install_state_t update_sts = INSTALL_READY;
	json_object* jo            = json_object_get(jsonObj);

	if (uacc && jo && update_parse_json_ready_update(uacc, jo, ua_intl.cache_dir) == E_UA_OK) {
		uacc->state = UA_STATE_READY_UPDATE_STARTED;

		update_set_rollback_info(uacc);

		if (uacc->rb_type == URB_DMC_INITIATED) {
			update_sts = update_start_rollback_operations(uacc, uacc->update_pkg.rollback_version, ua_intl.reboot_support);

		}else {
			if ((update_sts = update_start_install_operations(uacc, ua_intl.reboot_support)) == INSTALL_FAILED &&
			    uacc->rb_type >= URB_UA_INITIATED) {
				char* rb_version = update_get_next_rollback_version(uacc, uacc->update_file_info.version);
				update_sts =  update_start_rollback_operations(uacc, rb_version, ua_intl.reboot_support);

			}
		}

		if (update_sts == INSTALL_COMPLETED) {
			handler_backup_actions(uacc, uacc->update_pkg.name,  uacc->update_file_info.version);
		}

		json_object_put(jo);
		uacc->state = UA_STATE_READY_UPDATE_DONE;
	}else {
		if (uacc == NULL || jsonObj == NULL)
			DBG("Error: null pointer(s) detected: uacc(%p), jsonObj(%p)", uacc, jo);
		else
			DBG("Error: parsing ready-update.");
	}
}

static int patch_delta(char* pkgManifest, char* version, char* diffFile, char* newFile)
{
	int err             = E_UA_OK;
	pkg_file_t* pkgFile = f_malloc(sizeof(pkg_file_t));

	if (pkgManifest == NULL || diffFile == NULL || newFile == NULL || pkgFile == NULL ||
	    (get_pkg_file_manifest(pkgManifest, version, pkgFile)) ||
	    delta_reconstruct(pkgFile->file, diffFile, newFile)) {
		err = E_UA_ERR;

	}
	if (pkgFile != NULL)
		free_pkg_file(pkgFile);

	return err;
}


static void process_confirm_update(ua_component_context_t* uacc, json_object* jsonObj)
{
	pkg_info_t pkgInfo = {0};
	int rollback       = 0;

	if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
	    !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
	    !get_pkg_version_from_json(jsonObj, &pkgInfo.version)) {
		if (uacc->backup_manifest != NULL) {
			if (!get_body_rollback_from_json(jsonObj, &rollback) && rollback && !get_pkg_rollback_version_from_json(jsonObj, &pkgInfo.rollback_version))
				remove_old_backup(uacc->backup_manifest, pkgInfo.rollback_version);
			else
				remove_old_backup(uacc->backup_manifest, pkgInfo.version);

			uacc->state = UA_STATE_IDLE_INIT;
			update_release_comp_context(uacc);
		}

	}
}


static void process_download_report(ua_component_context_t* uacc, json_object* jsonObj)
{
	pkg_info_t pkgInfo = {0};
	int64_t downloadedBytes, totalBytes;

	if (!get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
	    !get_pkg_version_from_json(jsonObj, &pkgInfo.version) &&
	    !get_downloaded_bytes_from_json(jsonObj, &downloadedBytes) &&
	    !get_total_bytes_from_json(jsonObj, &totalBytes)) {
		DBG("Download in Progress %s : %s [%" PRId64 " / %" PRId64 "]", pkgInfo.name, pkgInfo.version, downloadedBytes, totalBytes);

	}
}


static void process_sota_report(ua_component_context_t* uacc, json_object* jsonObj)
{
}

static void process_log_report(ua_component_context_t* uacc, json_object* jsonObj)
{
}

static void process_update_status(ua_component_context_t* uacc, json_object* jsonObj)
{
	char* replyTo;

	if (!get_replyto_from_json(jsonObj, &replyTo) &&
	    uacc->update_status_info.reply_id &&
	    !strcmp(replyTo, uacc->update_status_info.reply_id)) {
		free(uacc->update_status_info.reply_id);
		uacc->update_status_info.reply_id = NULL;

		if (pthread_mutex_lock(&uacc->update_status_info.lock)) {
			DBG_SYS("lock failed");
		}

		get_update_status_response_from_json(jsonObj, &uacc->update_status_info.successful);

		pthread_cond_signal(&uacc->update_status_info.cond);
		pthread_mutex_unlock(&uacc->update_status_info.lock);
	}

}

install_state_t prepare_install_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile, int bck, pkg_file_t* updateFile, update_err_t* ue)
{
	ua_routine_t* uar     = (uacc != NULL) ? uacc->uar : NULL;
	install_state_t state = INSTALL_READY;
	char* newFile         = 0;
	char* installedVer    = 0;
	int err               = E_UA_OK;

	updateFile->version = f_strdup(pkgFile->version);

	if (!bck && uar->on_transfer_file) {
		transfer_file_action(uacc, pkgInfo, pkgFile);
	}

	if (ua_intl.delta && is_delta_package(pkgFile->file)) {
		char* bname = f_basename(pkgFile->file);
		updateFile->file = JOIN(ua_intl.cache_dir, "delta", bname);
		free(bname);

		if ((*uar->on_get_version)(pkgInfo->type, pkgInfo->name, &installedVer)) {
			DBG("get version for %s failed!", pkgInfo->name);
		}

		if (uacc->backup_manifest == NULL || (err = patch_delta(uacc->backup_manifest, installedVer, pkgFile->file, updateFile->file)) == E_UA_ERR) {
			if (err == E_UA_ERR)
				*ue = UE_INCREMENTAL_FAILED;
			state = INSTALL_FAILED;
		}

		updateFile->downloaded = 0;

	} else {
		updateFile->file       = f_strdup(pkgFile->file);
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


static int transfer_file_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile)
{
	ua_routine_t* uar = (uacc != NULL) ? uacc->uar : NULL;
	int ret           = E_UA_OK;
	char* newFile     = 0;

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


static download_state_t prepare_download_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo)
{
	ua_routine_t* uar      = (uacc != NULL) ? uacc->uar : NULL;
	download_state_t state = DOWNLOAD_CONSENT;

	if (uar->on_prepare_download) {
		state = (*uar->on_prepare_download)(pkgInfo->type, pkgInfo->name, pkgInfo->version);
		send_download_status(pkgInfo, state);
	}

	return state;
}

install_state_t pre_update_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile)
{
	ua_routine_t* uar     = (uacc != NULL) ? uacc->uar : NULL;
	install_state_t state = INSTALL_IN_PROGRESS;

	if (uar->on_pre_install) {
		state = (*uar->on_pre_install)(pkgInfo->type, pkgInfo->name, pkgFile->version, pkgFile->file);
	}

	if (state == INSTALL_IN_PROGRESS) {
		//Send update-status with reply-id
		if (send_current_report_version(uacc, pkgInfo, NULL) == E_UA_OK) {
			//Wait for response of update-status.
			if (pthread_mutex_lock(&uacc->update_status_info.lock)) {
				DBG_SYS("lock failed");
			}

			int rc = 0;
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 300;
			rc         = pthread_cond_timedwait(&uacc->update_status_info.cond, &uacc->update_status_info.lock, &ts);

			if (rc == ETIMEDOUT) {
				DBG("Timed out waiting for update status response");
				if (uacc->update_status_info.reply_id) {
					free(uacc->update_status_info.reply_id);
					uacc->update_status_info.reply_id = NULL;
				}
			}

			//Check if update-status reponse was set to successful.
			if (!uacc->update_status_info.successful)
				state = INSTALL_FAILED;
			else
				uacc->update_status_info.successful = 0;

			pthread_mutex_unlock(&uacc->update_status_info.lock);
		}

	}

	send_install_status(pkgInfo, state, 0, 0);

	return state;
}


install_state_t update_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile)
{
	ua_routine_t* uar     = (uacc != NULL) ? uacc->uar : NULL;
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


void post_update_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo)
{
	ua_routine_t* uar = (uacc != NULL) ? uacc->uar : NULL;

	if (uar->on_post_install) {
		(*uar->on_post_install)(pkgInfo->type, pkgInfo->name);
	}

}


void send_install_status(pkg_info_t* pkgInfo, install_state_t state, pkg_file_t* pkgFile, update_err_t ue)
{
	json_object* pkgObject = json_object_new_object();

	json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
	json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo->type));
	json_object_object_add(pkgObject, "version", json_object_new_string(pkgInfo->version));
	json_object_object_add(pkgObject, "status", json_object_new_string(install_state_string(state)));
	if (pkgInfo->rollback_version) json_object_object_add(pkgObject, "rollback-version", json_object_new_string(pkgInfo->rollback_version));
	if (pkgInfo->rollback_versions) json_object_object_add(pkgObject, "rollback-versions", json_object_get(pkgInfo->rollback_versions));


	if ((state == INSTALL_FAILED) && (ue != UE_NONE) && pkgFile) {
		if (ue == UE_TERMINAL_FAILURE) {
			json_object_object_add(pkgObject, "terminal-failure", json_object_new_boolean(1));
		}
		if (ue == UE_UPDATE_INCAPABLE) {
			json_object_object_add(pkgObject, "update-incapable", json_object_new_boolean(1));
		}
		if (ue == UE_INCREMENTAL_FAILED) {
			json_object* versionObject = json_object_new_object();
			json_object* verListObject = json_object_new_object();
			json_object_object_add(versionObject, "incremental-failed", json_object_new_boolean(1));
			json_object_object_add(verListObject, pkgFile->version, versionObject);
			json_object_object_add(pkgObject, "version-list", verListObject);
		}
	}

	if ((state == INSTALL_ROLLBACK) && pkgFile) {
		json_object* versionObject = json_object_new_object();
		json_object* verListObject = json_object_new_object();
		json_object_object_add(versionObject, "downloaded", json_object_new_boolean(pkgFile->downloaded ? 1 : 0));
		json_object_object_add(verListObject, pkgFile->version, versionObject);
		json_object_object_add(pkgObject, "version-list", verListObject);
	}

	json_object* bodyObject = json_object_new_object();
	json_object_object_add(bodyObject, "package", pkgObject);

	json_object* jObject = json_object_new_object();
	json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
	json_object_object_add(jObject, "body", bodyObject);

	ua_send_message(jObject);

	json_object_put(jObject);

}


static void send_download_status(pkg_info_t* pkgInfo, download_state_t state)
{
	json_object* pkgObject = json_object_new_object();

	json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
	json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo->type));
	json_object_object_add(pkgObject, "version", json_object_new_string(pkgInfo->version));
	json_object_object_add(pkgObject, "status", json_object_new_string(download_state_string(state)));

	json_object* bodyObject = json_object_new_object();
	json_object_object_add(bodyObject, "package", pkgObject);

	json_object* jObject = json_object_new_object();
	json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
	json_object_object_add(jObject, "body", bodyObject);

	ua_send_message(jObject);

	json_object_put(jObject);
}

static int send_current_report_version(ua_component_context_t* uacc, pkg_info_t* pkgInfo, char* report_version)
{
	int err = E_UA_OK;

	json_object* pkgObject = json_object_new_object();

	json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
	json_object_object_add(pkgObject, "version", S(report_version) ? json_object_new_string(report_version) : NULL);
	json_object_object_add(pkgObject, "status", json_object_new_string("CURRENT_REPORT"));

	json_object* bodyObject = json_object_new_object();
	json_object_object_add(bodyObject, "package", pkgObject);

	json_object* jObject = json_object_new_object();
	json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
	json_object_object_add(jObject, "body", bodyObject);

	if (uacc->update_status_info.reply_id) {
		free(uacc->update_status_info.reply_id);
		uacc->update_status_info.reply_id = NULL;
	}

	uacc->update_status_info.reply_id = randstring(REPLY_ID_STR_LEN);
	if (uacc->update_status_info.reply_id)
		json_object_object_add(jObject, "reply-id", json_object_new_string(uacc->update_status_info.reply_id ));

	err = ua_send_message(jObject);

	json_object_put(jObject);

	return err;
}


static int send_update_report(const char* pkgName, const char* version, int indeterminate, int percent, update_stage_t us)
{
	int err = E_UA_OK;

	do {
		BOLT_IF(!S(pkgName) || !S(version) || (percent < 0) || (percent > 100), E_UA_ARG, "install progress invalid");
		json_object* pkgObject = json_object_new_object();
		json_object_object_add(pkgObject, "name", json_object_new_string(pkgName));
		json_object_object_add(pkgObject, "version", json_object_new_string(version));

		json_object* bodyObject = json_object_new_object();
		json_object_object_add(bodyObject, "package", pkgObject);
		json_object_object_add(bodyObject, "progress", json_object_new_int(percent));
		json_object_object_add(bodyObject, "indeterminate", json_object_new_boolean(indeterminate ? 1 : 0));
		json_object_object_add(bodyObject, "stage", json_object_new_string(update_stage_string(us)));

		json_object* jObject = json_object_new_object();
		json_object_object_add(jObject, "type", json_object_new_string(UPDATE_REPORT));
		json_object_object_add(jObject, "body", bodyObject);

		err = ua_send_message(jObject);

		json_object_put(jObject);

	} while (0);

	return err;
}

static int backup_package(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile)
{
	int err                = E_UA_OK;
	pkg_file_t* backupFile = f_malloc(sizeof(pkg_file_t));

	char* bname = f_basename(pkgFile->file);

	if (backupFile != NULL && bname != NULL && uacc->backup_manifest != NULL) {
		backupFile->file       = JOIN(ua_intl.backup_dir, "backup", pkgInfo->name, pkgFile->version, bname);
		backupFile->version    = f_strdup(pkgFile->version);
		backupFile->downloaded = 1;

		if (backupFile->file && backupFile->version) {
			strcpy(backupFile->sha256b64, pkgFile->sha256b64);

			if (!strcmp(pkgFile->file, backupFile->file)) {
				DBG("Back up already exists: %s", backupFile->file);

			} else if (!copy_file(pkgFile->file, backupFile->file) &&
			           !add_pkg_file_manifest(uacc->backup_manifest, backupFile)) {
				DBG("Backed up package: %s", backupFile->file);

			} else {
				DBG("Backing up failed: %s", backupFile->file);
				err = E_UA_ERR;

			}
		}

		free(bname);
		free_pkg_file(backupFile);

	}else {
		err = E_UA_ERR;
	}

	return err;
}


static char* install_state_string(install_state_t state)
{
	char* str = NULL;

	switch (state) {
		case INSTALL_READY: str       = "INSTALL_READY";       break;
		case INSTALL_IN_PROGRESS: str = "INSTALL_IN_PROGRESS"; break;
		case INSTALL_COMPLETED: str   = "INSTALL_COMPLETED";   break;
		case INSTALL_FAILED: str      = "INSTALL_FAILED";      break;
		case INSTALL_ABORTED: str     = "INSTALL_ABORTED";     break;
		case INSTALL_ROLLBACK: str    = "INSTALL_ROLLBACK";    break;
	}

	return str;
}


static char* download_state_string(download_state_t state)
{
	char* str = NULL;

	switch (state) {
		case DOWNLOAD_POSTPONED: str = "DOWNLOAD_POSTPONED"; break;
		case DOWNLOAD_CONSENT: str   = "DOWNLOAD_CONSENT";   break;
		case DOWNLOAD_DENIED: str    = "DOWNLOAD_DENIED";    break;
	}

	return str;
}

static char* update_stage_string(update_stage_t stage)
{
	char* str = NULL;

	switch (stage) {
		case US_TRANSFER: str = "US_TRANSFER"; break;
		case US_INSTALL: str  = "US_INSTALL";   break;
	}

	return str;
}


static char* log_type_string(log_type_t log)
{
	char* str = NULL;

	switch (log) {
		case LOG_EVENT: str  = "EVENT";  break;
		case LOG_INFO: str   = "INFO";   break;
		case LOG_WARN: str   = "WARN";   break;
		case LOG_ERROR: str  = "ERROR";  break;
		case LOG_SEVERE: str = "SEVERE"; break;
	}

	return str;
}


void free_pkg_file(pkg_file_t* pkgFile)
{
	f_free(pkgFile->version);
	f_free(pkgFile->file);
	f_free(pkgFile);

}


const char* ua_get_updateagent_version()
{
	return BUILD_VERSION;

}

const char* ua_get_xl4bus_version()
{
	return xl4bus_get_version();

}

int handler_backup_actions(ua_component_context_t* uacc, char* pkgName, char* version)
{
	int ret               = E_UA_OK;
	pkg_info_t pkgInfo    = {0};
	pkg_file_t updateFile = {0};

	pkgInfo.name    = pkgName;
	pkgInfo.version = version;

	DBG("Backup package: %s, version: %s", pkgName, version);

	if (uacc->update_manifest != NULL) {
		if (!get_pkg_file_manifest(uacc->update_manifest,  pkgInfo.version, &updateFile)) {
			backup_package(uacc, &pkgInfo, &updateFile);

			free(updateFile.file);
			free(updateFile.version);
		}

		remove(uacc->update_manifest);

	}else {
		ret = E_UA_ERR;
	}

	return ret;

}

int ua_backup_package(char* pkgName, char* version)
{
	UT_array ri_list;

	for (int i = 0; i < ua_intl.n_uah; i++) {
		const char* type = (ua_intl.uah + i)->type_handler;

		utarray_init(&ri_list, &ut_ptr_icd);
		query_hash_tree(ri_tree, 0, type, 0, &ri_list, 1);

		int l = utarray_len(&ri_list);
		for (int j = 0; j < l; j++) {
			runner_info_t* ri          = *(runner_info_t**) utarray_eltptr(&ri_list, j);
			ua_component_context_t* cc = &ri->component;
			if (!strcmp(cc->update_pkg.name, pkgName)) {
				return handler_backup_actions(cc, pkgName, version);

			}
		}
	}

	return E_UA_ERR;

}

void handler_set_internal_state(ua_internal_state_t st)
{
	ua_intl.state = st;
}