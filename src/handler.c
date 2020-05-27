/*
 * hander.c
 */

#include "handler.h"
#include "utils.h"
#include "delta.h"
#include "debug.h"
#include "xml.h"
#include "xl4busclient.h"
#include "ua_version.h"
#include "updater.h"
#include "component.h"

#ifdef SUPPORT_UA_DOWNLOAD
#include <sys/types.h>
#include <dirent.h>
#include "ua_download.h"
#endif

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
static void process_sequence_info(ua_component_context_t* uacc, json_object* jsonObj);
static download_state_t prepare_download_action(ua_component_context_t* uacc);
static int transfer_file_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile);
static void send_download_status(ua_component_context_t* uacc, download_state_t state);
static int send_update_report(const char* pkgName, const char* version, int indeterminate, int percent, update_stage_t us);
static int send_current_report_version(ua_component_context_t* uacc, pkg_info_t* pkgInfo, char* report_version);
static int send_sequence_query(void);
static int backup_package(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile);
static int patch_delta(char* pkgManifest, char* version, char* diffFile, char* newFile);
static char* install_state_string(install_state_t state);
static char* download_state_string(download_state_t state);
static char* update_stage_string(update_stage_t stage);
static char* log_type_string(log_type_t log);
static void release_comp_sequence(comp_sequence_t* seq);
void* runner_loop(void* arg);

#ifdef SUPPORT_UA_DOWNLOAD
static void ua_verify_ca_file_init(const char* path);
static void process_start_download(ua_component_context_t* uacc, json_object* jsonObj);
static void process_query_trust(ua_component_context_t* uacc, json_object* jsonObj);
#endif

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

		if (uaConfig->delta) {
			if (delta_init(uaConfig->cache_dir, uaConfig->delta_config)) {
				DBG("delta initialization failed, disabling delta feature. ");
				uaConfig->delta = 0;
			}
		}

		memset(&ua_intl, 0, sizeof(ua_internal_t));
		ua_intl.delta                         = uaConfig->delta;
		ua_intl.reboot_support                = uaConfig->reboot_support;
		ua_intl.cache_dir                     = S(uaConfig->cache_dir) ? f_strdup(uaConfig->cache_dir) : NULL;
		ua_intl.backup_dir                    = S(uaConfig->backup_dir) ? f_strdup(uaConfig->backup_dir) : NULL;
		ua_intl.record_file                   = S(ua_intl.backup_dir) ? JOIN(ua_intl.backup_dir, "backup", UPDATE_REC_FILE) : NULL;
		ua_intl.backup_source                 = uaConfig->backup_source;
		ua_intl.package_verification_disabled = uaConfig->package_verification_disabled;
		ua_intl.enable_fake_rb_ver            = uaConfig->enable_fake_rb_ver;

		#ifdef SUPPORT_UA_DOWNLOAD
		ua_intl.ua_download_required      = uaConfig->ua_download_required;
		ua_intl.ua_dl_dir                 = S(uaConfig->ua_dl_dir) ? f_strdup(uaConfig->ua_dl_dir) : NULL;
		ua_intl.ua_dl_connect_timout_ms   = 3*60*1000; // 3 minutes
		ua_intl.ua_dl_download_timeout_ms = 3*60*1000; // 3 minutes
		if (S(uaConfig->sigca_dir)) {
			ua_verify_ca_file_init(uaConfig->sigca_dir);
		}
		#endif

		BOLT_SUB(xl4bus_client_init(uaConfig->url, uaConfig->cert_dir));
		BOLT_SYS(pthread_mutex_init(&ua_intl.backup_lock, 0), "backup lock init");

		ua_intl.state = UAI_STATE_INITIALIZED;

		if (uaConfig->rw_buffer_size) ua_rw_buff_size = uaConfig->rw_buffer_size * 1024;

	} while (0);

	if (err) {
		ua_stop();
	}

	return err;
}


int ua_stop(void)
{
	int rc = 0;

	Z_FREE(ua_intl.cache_dir);
	Z_FREE(ua_intl.backup_dir);
	Z_FREE(ua_intl.record_file);

	#ifdef SUPPORT_UA_DOWNLOAD
	if (ua_intl.ua_dl_dir) { free(ua_intl.ua_dl_dir); ua_intl.ua_dl_dir = NULL; }
	int index = 0;
	for (index = 0; index < MAX_VERIFY_CA_COUNT; ++index) {
		if (ua_intl.verify_ca_file[index]) {
			free(ua_intl.verify_ca_file[index]);
			ua_intl.verify_ca_file[index] = NULL;
		}
	}
	#endif

	delta_stop();
	xmlCleanupParser();
	if (ua_intl.state >= UAI_STATE_INITIALIZED) {
		rc            = xl4bus_client_stop();
		ua_intl.state = UAI_STATE_NOT_KNOWN;
	}
	pthread_mutex_destroy(&ua_intl.backup_lock);
	return rc;
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
			ri                 = f_malloc(sizeof(runner_info_t));
			ri->component.type = f_strdup((uah + i)->type_handler);
			ri->component.uar  = (*(uah + i)->get_routine)();
			ri->run            = 1;
			BOLT_IF(!ri->component.uar || !ri->component.uar->on_get_version || !ri->component.uar->on_install || !S(ri->component.type), E_UA_ARG, "registration error");
			BOLT_SYS(pthread_mutex_init(&ri->lock, 0), "lock init");
			BOLT_SYS(pthread_cond_init(&ri->cond, 0), "cond init");
			BOLT_SYS(pthread_create(&ri->thread, 0, runner_loop, ri), "pthread create");
			query_hash_tree(ri_tree, ri, ri->component.type, 0, 0, 0);

			BOLT_SYS(pthread_mutex_init(&ri->component.update_status_info.lock, NULL), "update status lock init");
			BOLT_SYS(pthread_cond_init(&ri->component.update_status_info.cond, 0), "update status cond init");
			ri->component.update_status_info.reply_id = NULL;
			ri->component.record_file                 = ua_intl.record_file;
			ri->component.enable_fake_rb_ver          = ua_intl.enable_fake_rb_ver;
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
					void* res;
					BOLT_SYS(pthread_join(ri->thread, &res), "thread join");
					BOLT_SYS(pthread_cond_destroy(&ri->cond), "cond destroy");
					BOLT_SYS(pthread_mutex_destroy(&ri->lock), "lock destroy");
					release_comp_sequence(ri->component.seq_in);
					release_comp_sequence(ri->component.seq_out);
					ri->component.record_file = NULL;
					comp_release_state_info(ri->component.st_info);
					Z_FREE(ri->component.update_manifest);
					Z_FREE(ri->component.backup_manifest);
					Z_FREE(ri->component.type);
					Z_FREE(ri->component.update_status_info.reply_id);
					Z_FREE(ri);
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

	Z_FREE(ri_tree);

	return ret;
}

void* ua_handle_dmc_presence(void* arg)
{
	ua_routine_t* uar = (ua_routine_t*)arg;

	if (uar && uar->on_dmc_presence)
		(*uar->on_dmc_presence)(NULL);

	while (!ua_intl.seq_info_valid) {
		send_sequence_query();
		sleep(5);
	}

	return NULL;
}

int ua_send_message(json_object* jsonObj)
{
	char* msg = (char*)json_object_to_json_string(jsonObj);

	DBG("Sending to DMC : %s", msg);
	return xl4bus_client_send_msg(msg);

}

int ua_send_message_string(char* message)
{
	DBG("Sending to DMC : %s", message);
	return xl4bus_client_send_msg(message);
}

XL4_PUB int ua_send_message_with_address(json_object* jsonObj, xl4bus_address_t* xl4_address)
{
	char* msg = (char*)json_object_to_json_string(jsonObj);

	DBG("Sending with bus addr: %s", msg);
	return xl4bus_client_send_msg_to_addr(msg, xl4_address);
}

int ua_send_message_string_with_address(char* message,  xl4bus_address_t* xl4_address)
{
	DBG("Sending with bus addr: %s", message);
	return xl4bus_client_send_msg_to_addr(message, xl4_address);
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
			strncpy(timestamp, logdata->timestamp, sizeof(timestamp) - 1);
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
		"CLIENT_STOPPED",
		"CLIENT_START"
	};

	DBG("eSync Bus Status (%d): %s", status, status < sizeof(sts_str)/sizeof(sts_str[0]) ? sts_str[status] : "NULL");
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


void handle_presence(int connected, int disconnected, esync_bus_conn_state_t connection_state)
{
	DBG("Connected : %d,  Disconnected : %d", connected, disconnected);
	switch (connection_state)
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

			if (ua_intl.uah) {
				for (int i = 0; i < ua_intl.n_uah; i++) {
					ua_routine_t* uar = (*(ua_intl.uah + i)->get_routine)();
					if (uar) {
						pthread_t thread_dmc_presence;
						pthread_attr_t attr;
						pthread_attr_init(&attr);
						pthread_attr_setdetachstate(&attr, 1);
						if (pthread_create(&thread_dmc_presence, &attr, ua_handle_dmc_presence, (void*)uar)) {
							DBG("Failed to spawn thread_dmc_presence.");
						}
						pthread_attr_destroy(&attr);
					}
				}
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
	if (!l)
		DBG("Incoming message for non-registered handler %s : %s", type, msg);
	//else
	//	DBG("Registered handlers found for %s : %d", type, l);

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
		incoming_msg_t* im = NULL;
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

static int handler_update_outgoing_seq_num(comp_sequence_t** seq, char* name, int update_num)
{
	int num            = update_num > 0 ? update_num : 0;
	comp_sequence_t* s = NULL;

	HASH_FIND_STR(*seq, name, s);
	if (s) {
		num = update_num > 0 ? update_num : s->num + 1;
		HASH_DEL(*seq, s);
		f_free(s->name);
		free(s);
	}

	s       = (comp_sequence_t*)malloc(sizeof(comp_sequence_t));
	s->name = f_strdup(name);
	s->num  = num;
	HASH_ADD_KEYPTR( hh, *seq, s->name, strlen(s->name), s );

	return num;
}

static int handler_chk_incoming_seq_outdated(comp_sequence_t** seq, char* name, int num)
{
	int outdated       = 0;
	comp_sequence_t* s = NULL;

	HASH_FIND_STR(*seq, name, s);
	if (s) {
		//DBG("found %s in seq num: %d", s->name, s->num);
		if (s->num >= num) {
			DBG("Got outdated command sequence number : %d for %s", seq, name);
			outdated = 1;
		}
		HASH_DEL(*seq, s);
		f_free(s->name);
		free(s);
	}

	s       = (comp_sequence_t*)malloc(sizeof(comp_sequence_t));
	s->name = f_strdup(name);;
	s->num  = num;
	HASH_ADD_KEYPTR( hh, *seq, s->name, strlen(s->name), s );

	return outdated;
}

static void release_comp_sequence(comp_sequence_t* seq)
{
	comp_sequence_t* cs, * tmp;

	HASH_ITER(hh, seq, cs, tmp) {
		f_free(cs->name);
		HASH_DEL(seq, cs);
		free(cs);
	}
}

static void process_message(ua_component_context_t* uacc, const char* msg, size_t len)
{
	char* type     = NULL;
	char* pkg_name = NULL;
	enum json_tokener_error jErr;
	int seq = -1;

	json_object* jObj = json_tokener_parse_verbose(msg, &jErr);

	if (jErr == json_tokener_success) {
		if ( !get_seq_num_from_json(jObj, &seq) &&
		     !get_pkg_name_from_json(jObj, &pkg_name) ) {
			if (handler_chk_incoming_seq_outdated(&uacc->seq_in, pkg_name, seq)) {
				DBG("Skipping the installation command due to updated sequence");
				if (!jErr) json_object_put(jObj);
				return;
			}
		}

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
				} else if (!strcmp(type, QUERY_SEQUENCE)) {
					process_run(uacc, process_sequence_info, jObj, 0);
				} else if (!strcmp(type, UPDATE_STATUS)) {
					process_run(uacc, process_update_status, jObj, 0);
				#ifdef SUPPORT_UA_DOWNLOAD
				} else if (!strcmp(type, START_DOWNLOAD)) {
					process_run(uacc, process_start_download, jObj, 1);
				} else if (!strcmp(type, QUERY_TRUST)) {
					process_run(uacc, process_query_trust, jObj, 0);
				#endif
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

			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, 1);
			if (pthread_create(&uacc->worker.worker_thread, &attr, worker_action, uacc)) {
				DBG_SYS("pthread create");
				json_object_put(uacc->worker.worker_jobj);
				uacc->worker.worker_running = 0;
			}
			pthread_attr_destroy(&attr);

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
	int fake_rb_ver    = 0;

	if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
	    !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
	    !get_replyid_from_json(jsonObj, &replyId)) {
		json_object* bodyObject = json_object_new_object();

		if (comp_get_update_stage(uacc->st_info, pkgInfo.name) == UA_STATE_READY_UPDATE_STARTED) {
			json_object_object_add(bodyObject, "do-not-disturb", json_object_new_boolean(1));
		}else {
			json_object* pkgObject = json_object_new_object();
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

			#ifdef SUPPORT_UA_DOWNLOAD
			if (ua_intl.ua_download_required) {
				json_object_object_add(pkgObject, "user-agent-download", json_object_new_boolean(1));
			}
			#endif

			if (uae != E_UA_OK) {
				json_object_object_add(pkgObject, "update-incapable", json_object_new_boolean(1));
			}

			fake_rb_ver = delta_use_external_algo() || ua_rollback_disabled(pkgInfo.name);

			if ( !(S(ua_intl.backup_dir) && (uacc->backup_manifest = JOIN(ua_intl.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG))) ) {
				fake_rb_ver = 1;
			}

			if (!fake_rb_ver &&  uae == E_UA_OK ) {
				pkg_file_t* pf, * aux, * pkgFile = NULL;
				json_object* verListObject = json_object_new_object();
				json_object* rbVersArray   = json_object_new_array();

				if (!parse_pkg_manifest(uacc->backup_manifest, &pkgFile)) {
					DL_FOREACH_SAFE(pkgFile, pf, aux) {
						json_object* versionObject = json_object_new_object();

						json_object_object_add(versionObject, "file", json_object_new_string(pf->file));
						json_object_object_add(versionObject, "downloaded", json_object_new_boolean(pf->downloaded ? 1 : 0));
						json_object_object_add(versionObject, "rollback-order", json_object_new_int(pf->rollback_order));

						if (ua_intl.delta) {
							json_object_object_add(versionObject, "sha-256", json_object_new_string(pf->sha_of_sha));
						}

						json_object_object_add(verListObject, pf->version, versionObject);
						json_object_array_add(rbVersArray, json_object_new_string(pf->version));

						DL_DELETE(pkgFile, pf);
						free_pkg_file(pf);
					}
				}

				if (json_object_array_length(rbVersArray) > 0) {
					json_object_object_add(pkgObject, "version-list", verListObject);
					json_object_object_add(pkgObject, "rollback-versions", rbVersArray);
				} else {
					fake_rb_ver = 1;
					json_object_put(verListObject);
					json_object_put(rbVersArray);
				}
			}

			if (uacc->enable_fake_rb_ver && fake_rb_ver && uae == E_UA_OK ) {
				json_object* verListObject = json_object_new_object();
				json_object* rbVersArray   = json_object_new_array();

				json_object* versionObject = json_object_new_object();
				json_object_object_add(versionObject, "file", json_object_new_string(""));
				json_object_object_add(versionObject, "downloaded", json_object_new_boolean(1));
				json_object_object_add(versionObject, "rollback-order", json_object_new_int(0));
				json_object_object_add(versionObject, "sha-256", json_object_new_string(NULL_STR(installedVer)));
				json_object_object_add(verListObject, NULL_STR(installedVer), versionObject);

				json_object_array_add(rbVersArray, json_object_new_string(NULL_STR(installedVer)));

				json_object_object_add(pkgObject, "version-list", verListObject);
				json_object_object_add(pkgObject, "rollback-versions", rbVersArray);

				comp_set_fake_rb_version(&uacc->st_info, pkgInfo.name, NULL_STR(installedVer));

			}

			json_object_object_add(bodyObject, "package", pkgObject);
			Z_FREE(uacc->backup_manifest);
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
	if (!get_pkg_type_from_json(jsonObj, &uacc->update_pkg.type) &&
	    !get_pkg_name_from_json(jsonObj, &uacc->update_pkg.name) &&
	    !get_pkg_version_from_json(jsonObj, &uacc->update_pkg.version)) {
		prepare_download_action(uacc);

	}

	memset(&uacc->update_pkg, 0, sizeof(pkg_info_t));
}


static void process_prepare_update(ua_component_context_t* uacc, json_object* jsonObj)
{
	int bck                = 0;
	pkg_file_t pkgFile     = {0}, updateFile = {0};
	update_err_t updateErr = UE_NONE;
	install_state_t state  = INSTALL_READY;

	#ifdef SUPPORT_UA_DOWNLOAD
	char tmp_filename[PATH_MAX] = {0};
	#endif

	if (!get_pkg_type_from_json(jsonObj, &uacc->update_pkg.type) &&
	    !get_pkg_name_from_json(jsonObj, &uacc->update_pkg.name) &&
	    !get_pkg_version_from_json(jsonObj, &uacc->update_pkg.version)) {
		ua_stage_t st = comp_get_update_stage(uacc->st_info, uacc->update_pkg.name);
		if ( st == UA_STATE_PREPARE_UPDATE_STARTED ) {
			DBG("skipping, still prcocessing prepare-update for version %s of %s", uacc->update_pkg.version, uacc->update_pkg.name);
			return;

		}

		if ( st == UA_STATE_PREPARE_UPDATE_DONE ) {
			char* prepared_ver = comp_get_prepared_version(uacc->st_info, uacc->update_pkg.name);

			if (prepared_ver) {
				if (!strcmp(prepared_ver, uacc->update_pkg.version)) {
					DBG("version %s of %s has been marked prepared, sending back INSTALL_READY with no further processing", uacc->update_pkg.version, uacc->update_pkg.name);
					send_install_status(uacc, INSTALL_READY, 0, UE_NONE);
					return;
				} else {
					DBG("%s prepare-update has different version(%s) than the saved prepared_ver: %s ",
					    uacc->update_pkg.name, uacc->update_pkg.version, prepared_ver);
				}
			} else {
				DBG("version %s has been marked prepared, but found no version record.", uacc->update_pkg.version);
			}
		}

		comp_set_update_stage(&uacc->st_info, uacc->update_pkg.name, UA_STATE_PREPARE_UPDATE_STARTED);

		get_pkg_rollback_version_from_json(jsonObj, &uacc->update_pkg.rollback_version);
		pkgFile.version       = S(uacc->update_pkg.rollback_version) ? uacc->update_pkg.rollback_version : uacc->update_pkg.version;
		uacc->backup_manifest = JOIN(ua_intl.backup_dir, "backup", uacc->update_pkg.name, MANIFEST_PKG);

		if (( (!get_pkg_file_from_json(jsonObj, pkgFile.version, &pkgFile.file)
				#ifdef SUPPORT_UA_DOWNLOAD
		       || ua_intl.ua_download_required
				#endif
		       ) &&
		      !get_pkg_downloaded_from_json(jsonObj, pkgFile.version, &pkgFile.downloaded)) ||
		    ((!get_pkg_file_manifest(uacc->backup_manifest, pkgFile.version, &pkgFile)) && (bck = 1))) {
			get_pkg_sha256_from_json(jsonObj, pkgFile.version, pkgFile.sha256b64);
			get_pkg_delta_sha256_from_json(jsonObj, pkgFile.version, pkgFile.delta_sha256b64);

			#ifdef SUPPORT_UA_DOWNLOAD
			if (ua_intl.ua_download_required && bck != 1) {
				snprintf(tmp_filename, PATH_MAX, "%s/%s/%s/%s.e",
				         ua_intl.ua_dl_dir,
				         uacc->update_pkg.name, uacc->update_pkg.version,
				         uacc->update_pkg.version);
				pkgFile.file = tmp_filename;
				DBG("ua download changes pkf file path[%s]=", pkgFile.file);
			}
			#endif

			uacc->update_manifest = JOIN(ua_intl.cache_dir, uacc->update_pkg.name, MANIFEST_PKG);

			state = prepare_install_action(uacc, &pkgFile, bck, &updateFile, &updateErr);
			send_install_status(uacc, state, &pkgFile, updateErr);

			Z_FREE(updateFile.version);
			Z_FREE(updateFile.file);
			Z_FREE(uacc->update_manifest);

		} else {
			DBG("prepare-update msg doesn't have the expected info, returning INSTALL_FAILED");
			state = INSTALL_FAILED;
			send_install_status(uacc, INSTALL_FAILED, 0, UE_NONE);

		}

		if (bck) {
			free(pkgFile.version);
			free(pkgFile.file);
		}
		if (state == INSTALL_READY) {
			comp_set_prepared_version(&uacc->st_info, uacc->update_pkg.name, uacc->update_pkg.version);
			comp_set_update_stage(&uacc->st_info, uacc->update_pkg.name, UA_STATE_PREPARE_UPDATE_DONE);
		}
		else
			comp_set_update_stage(&uacc->st_info, uacc->update_pkg.name, st);

		Z_FREE(uacc->backup_manifest);
		memset(&uacc->update_pkg, 0, sizeof(pkg_info_t));
	}
}

static void process_ready_update(ua_component_context_t* uacc, json_object* jsonObj)
{
	install_state_t update_sts = INSTALL_READY;
	json_object* jo            = json_object_get(jsonObj);

	uacc->cur_msg = jo;

	if (uacc && jo && update_parse_json_ready_update(uacc, jo, ua_intl.cache_dir, ua_intl.backup_dir) == E_UA_OK) {
		comp_set_update_stage(&uacc->st_info, uacc->update_pkg.name, UA_STATE_READY_UPDATE_STARTED);
		comp_set_prepared_version(&uacc->st_info, uacc->update_pkg.name, NULL); //reset saved prepared_ver

		update_set_rollback_info(uacc);

		if (uacc->update_pkg.rollback_version) {
			update_sts = update_start_rollback_operations(uacc, uacc->update_pkg.rollback_version, ua_intl.reboot_support);

		}else {
			if ((update_sts = update_start_install_operations(uacc, ua_intl.reboot_support)) == INSTALL_FAILED &&
			    uacc->update_pkg.rollback_versions) {
				char* rb_version = update_get_next_rollback_version(uacc, uacc->update_file_info.version);
				if (rb_version) {
					update_sts = INSTALL_ROLLBACK;
					update_send_rollback_status(uacc, rb_version);
				} else {
					DBG("Failed to locate rollback info, informing terminal-failure.");
					uacc->update_error = UE_TERMINAL_FAILURE;
					send_install_status(uacc, INSTALL_FAILED, &uacc->update_file_info, uacc->update_error);
				}
			}

			if (update_sts != INSTALL_COMPLETED && !access(uacc->update_manifest, F_OK)) {
				DBG("Removing temp manifest %s", uacc->update_manifest);
				remove(uacc->update_manifest);
			}
		}

		if ( update_sts == INSTALL_COMPLETED &&
		     !ua_rollback_disabled(uacc->update_pkg.name) &&
		     !is_prepared_delta_package(uacc->update_file_info.file) ) {
			handler_backup_actions(uacc, uacc->update_pkg.name,  uacc->update_file_info.version);
		}

		comp_set_update_stage(&uacc->st_info, uacc->update_pkg.name, UA_STATE_READY_UPDATE_DONE);
		uacc->cur_msg = NULL;
		json_object_put(jo);
		Z_FREE(uacc->backup_manifest);
		Z_FREE(uacc->update_manifest);
		update_release_comp_context(uacc);

	}else {
		if (uacc == NULL || jsonObj == NULL)
			DBG("Error: null pointer(s) detected: uacc(%p), jsonObj(%p)", uacc, jo);
		else {
			DBG("Error: parsing ready-update, or getting info form temp manifest.");
			send_install_status(uacc, INSTALL_FAILED, &uacc->update_file_info, uacc->update_error);
		}
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
		ua_stage_t st = comp_get_update_stage(uacc->st_info, pkgInfo.name);

		if (!(st == UA_STATE_READY_UPDATE_DONE ||  st == UA_STATE_UNKNOWN)) {
			DBG("skipping confirm-update for version %s of %s", pkgInfo.version, pkgInfo.name);
			return;
		}

		char* update_manifest = JOIN(ua_intl.cache_dir, pkgInfo.name, MANIFEST_PKG);
		char* backup_manifest = JOIN(ua_intl.backup_dir, "backup", pkgInfo.name, MANIFEST_PKG);

		if (backup_manifest && update_manifest) {
			if (!access(update_manifest, F_OK)) {
				comp_set_update_stage(&uacc->st_info, pkgInfo.name, UA_STATE_CONFIRM_UPDATE_STARTED);

				if (!get_body_rollback_from_json(jsonObj, &rollback)
				    && rollback
				    && !get_pkg_rollback_version_from_json(jsonObj, &pkgInfo.rollback_version))
					remove_old_backup(backup_manifest, pkgInfo.rollback_version);
				else
					remove_old_backup(backup_manifest, pkgInfo.version);

				DBG("Removing update_manifest %s", update_manifest);
				remove(update_manifest);
				comp_set_update_stage(&uacc->st_info, pkgInfo.name, UA_STATE_CONFIRM_UPDATE_DONE);
			} else {
				DBG("skipping confirm-update, did not find temp manifest %s", update_manifest);
			}

		}else
			DBG("Could not form manifest file path.");

		Z_FREE(backup_manifest);
		Z_FREE(update_manifest);

	} else
		DBG("Could not parse info from confirm-update.");
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

static void process_sequence_info(ua_component_context_t* uacc, json_object* jsonObj)
{
	char* replyTo;

	if (!get_replyto_from_json(jsonObj, &replyTo) &&
	    ua_intl.query_reply_id ) {
		if (!strcmp(replyTo, ua_intl.query_reply_id)) {
			DBG("sequence-info with reply-id: %s", replyTo);
			ua_intl.seq_info_valid = 1;
			json_object* jo_seq = NULL;
			get_seq_info_per_update_from_json(jsonObj, &jo_seq);
			json_object_object_foreach(jo_seq, key, val) {
				if (json_object_get_type(val) == json_type_int) {
					int seq = json_object_get_int64(val);
					DBG("Update component \"%s\" sequence num to %d", key, seq);
					handler_update_outgoing_seq_num(&uacc->seq_out, key, seq);
				} else {
					DBG("json value is not interger");
				}
			}

		} else {
			DBG("sequence-info with mismatched reply id: %s, expected: %s", replyTo, ua_intl.query_reply_id);
		}

		Z_FREE(ua_intl.query_reply_id);
	}
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

install_state_t prepare_install_action(ua_component_context_t* uacc, pkg_file_t* pkgFile, int bck, pkg_file_t* updateFile, update_err_t* ue)
{
	ua_routine_t* uar     = (uacc != NULL) ? uacc->uar : NULL;
	install_state_t state = INSTALL_READY;
	char* newFile         = 0;
	char* installedVer    = 0;
	int err               = E_UA_OK;
	pkg_info_t* pkgInfo   = (uacc != NULL) ? &uacc->update_pkg : NULL;

	updateFile->version = f_strdup(pkgFile->version);

	if (!bck && uar->on_transfer_file) {
		err = transfer_file_action(uacc, pkgInfo, pkgFile);
		if (err == E_UA_ERR)
			DBG("UA:on_transfer_file returned error");
	}

	if (err == E_UA_OK && !ua_intl.package_verification_disabled) {
		if (S(pkgFile->delta_sha256b64))
			err =  verify_file_hash_b64(pkgFile->file, pkgFile->delta_sha256b64);
		else
			err =  verify_file_hash_b64(pkgFile->file, pkgFile->sha256b64);
	}

	if (err == E_UA_OK && ua_intl.delta && is_delta_package(pkgFile->file) && !is_prepared_delta_package(pkgFile->file)) {
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
		if (err == E_UA_OK)
			calculate_sha256_b64(updateFile->file, updateFile->sha256b64);
		updateFile->downloaded = 0;

	} else {
		updateFile->file = f_strdup(pkgFile->file);
		memcpy(updateFile->sha256b64, pkgFile->sha256b64, sizeof(updateFile->sha256b64));
		if (err == E_UA_OK)
			updateFile->downloaded = 1;
	}

	if (err == E_UA_OK) {
		if (uar->on_prepare_install) {
			state = (*uar->on_prepare_install)(pkgInfo->type, pkgInfo->name, updateFile->version, updateFile->file, &newFile);
			if (S(newFile)) {
				DBG("UA indicated to update using this new file: %s", newFile);
				free(updateFile->file);
				updateFile->file = f_strdup(newFile);
			}
		}

	} else {
		DBG("Error in prepare_install_action, returning INSTALL_FAILED");
		state = INSTALL_FAILED;
	}

	if (state == INSTALL_READY) {
		if (uacc->update_manifest != NULL) {
			if (!calc_sha256_x(updateFile->file, updateFile->sha_of_sha)) {
				add_pkg_file_manifest(uacc->update_manifest, updateFile);
			} else {
				DBG("Error in calculating sha_of_sha, returning INSTALL_FAILED");
				state = INSTALL_FAILED;
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

	if (uar && uar->on_transfer_file && pkgInfo) {
		ret = (*uar->on_transfer_file)(pkgInfo->type, pkgInfo->name, pkgFile->version, pkgFile->file, &newFile);
		if (!ret && S(newFile)) {
			DBG("UA transfered file %s to new path %s", pkgFile->file, newFile);
			pkgFile->file = newFile;
		} else {
			DBG("UA returned error(%d) in callback on_transfer_file.", ret);
		}
	}

	return ret;
}


static download_state_t prepare_download_action(ua_component_context_t* uacc)
{
	ua_routine_t* uar      = (uacc != NULL) ? uacc->uar : NULL;
	download_state_t state = DOWNLOAD_CONSENT;

	if (uar->on_prepare_download) {
		state = (*uar->on_prepare_download)(uacc->update_pkg.type, uacc->update_pkg.name, uacc->update_pkg.version);
		send_download_status(uacc, state);
	}

	return state;
}

install_state_t pre_update_action(ua_component_context_t* uacc)
{
	ua_routine_t* uar     = (uacc != NULL) ? uacc->uar : NULL;
	install_state_t state = INSTALL_IN_PROGRESS;
	pkg_info_t* pkgInfo   = &uacc->update_pkg;
	pkg_file_t* pkgFile   = &uacc->update_file_info;

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
				Z_FREE(uacc->update_status_info.reply_id);
			}

			//Check if update-status reponse was set to successful.
			if (!uacc->update_status_info.successful)
				state = INSTALL_FAILED;
			else
				uacc->update_status_info.successful = 0;

			pthread_mutex_unlock(&uacc->update_status_info.lock);
		}

	}

	if (state == INSTALL_IN_PROGRESS)
		send_install_status(uacc, state, 0, UE_NONE);

	return state;
}


install_state_t update_action(ua_component_context_t* uacc)
{
	ua_routine_t* uar     = (uacc != NULL) ? uacc->uar : NULL;
	install_state_t state = INSTALL_FAILED;
	pkg_info_t* pkgInfo   = &uacc->update_pkg;
	pkg_file_t* pkgFile   = &uacc->update_file_info;

	if (uar) {
		DBG("Asking UA to install version %s with file %s.", pkgFile->version, pkgFile->file);
		state = (*uar->on_install)(pkgInfo->type, pkgInfo->name, pkgFile->version, pkgFile->file);

		if ((state == INSTALL_COMPLETED) && uar->on_set_version) {
			if ((*uar->on_set_version)(pkgInfo->type, pkgInfo->name, pkgFile->version))
				DBG("set version for %s failed!", pkgInfo->name);
		}

		if (!(pkgInfo->rollback_versions && (state == INSTALL_FAILED))) {
			send_install_status(uacc, state, 0, UE_NONE);
		}
	}
	return state;
}


void post_update_action(ua_component_context_t* uacc)
{
	ua_routine_t* uar = (uacc != NULL) ? uacc->uar : NULL;

	if (uar->on_post_install) {
		(*uar->on_post_install)(uacc->update_pkg.type, uacc->update_pkg.name);
	}

}


void send_install_status(ua_component_context_t* uacc, install_state_t state, pkg_file_t* pkgFile, update_err_t ue)
{
	json_object* pkgObject = json_object_new_object();
	pkg_info_t* pkgInfo    = &uacc->update_pkg;

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
	if (ua_intl.seq_info_valid)
		json_object_object_add(bodyObject, "sequence", json_object_new_int(handler_update_outgoing_seq_num(&uacc->seq_out, pkgInfo->name, 0)));
	json_object_object_add(bodyObject, "package", pkgObject);

	json_object* jObject = json_object_new_object();
	json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
	json_object_object_add(jObject, "body", bodyObject);

	ua_send_message(jObject);

	json_object_put(jObject);

}


static void send_download_status(ua_component_context_t* uacc, download_state_t state)
{
	json_object* pkgObject = json_object_new_object();
	pkg_info_t* pkgInfo    = &uacc->update_pkg;

	json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
	json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo->type));
	json_object_object_add(pkgObject, "version", json_object_new_string(pkgInfo->version));
	json_object_object_add(pkgObject, "status", json_object_new_string(download_state_string(state)));

	json_object* bodyObject = json_object_new_object();
	if (ua_intl.seq_info_valid)
		json_object_object_add(bodyObject, "sequence", json_object_new_int(handler_update_outgoing_seq_num(&uacc->seq_out, pkgInfo->name, 0)));
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
	if (ua_intl.seq_info_valid)
		json_object_object_add(bodyObject, "sequence", json_object_new_int(handler_update_outgoing_seq_num(&uacc->seq_out, pkgInfo->name, 0)));
	json_object_object_add(bodyObject, "package", pkgObject);

	json_object* jObject = json_object_new_object();
	json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
	json_object_object_add(jObject, "body", bodyObject);

	Z_FREE(uacc->update_status_info.reply_id);

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
			strcpy(backupFile->sha_of_sha, pkgFile->sha_of_sha);

			if (!strcmp(pkgFile->file, backupFile->file) ||
			    !sha256xcmp(backupFile->file, backupFile->sha256b64)) {
				DBG("Back up already exists: %s", backupFile->file);

			} else {
				char* src_file = pkgFile->file;
				if (ua_intl.delta && ua_intl.backup_source == 1) {
					char* bname = f_basename(pkgFile->file);
					src_file = JOIN(ua_intl.cache_dir, "delta", bname);
					free(bname);

					if (src_file ) {
						if (access(src_file, F_OK)) {
							DBG("Reconstructed file %s is not available", src_file);
							src_file = pkgFile->file;
						}
					}else {
						DBG("Error generating reconstructed filepath, try using installation path for backup");
						src_file = pkgFile->file;
					}

				}

				if (make_file_hard_link(src_file, backupFile->file) != E_UA_OK)
					err = copy_file(src_file, backupFile->file);
				if (err == E_UA_OK)
					add_pkg_file_manifest(uacc->backup_manifest, backupFile);
				else
					DBG("Backing up failed: %s", backupFile->file);

				if (src_file != pkgFile->file) {
					DBG("Removing installation file after backup: %s", src_file);
					unlink(src_file);
					Z_FREE(src_file);

				}
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
	Z_FREE(pkgFile->version);
	Z_FREE(pkgFile->file);
	Z_FREE(pkgFile);

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

	//No error check, try to proceed with backup if failed to lock.
	pthread_mutex_lock(&ua_intl.backup_lock);

	if (uacc->update_manifest != NULL) {
		if (!get_pkg_file_manifest(uacc->update_manifest,  pkgInfo.version, &updateFile)) {
			if (!is_delta_package(updateFile.file))
				backup_package(uacc, &pkgInfo, &updateFile);

			free(updateFile.file);
			free(updateFile.version);
		} else {
			DBG("Could not parse info from update_manifest %s.", uacc->update_manifest);
		}
	} else {
		DBG("update_manifest is not set.");
		ret = E_UA_ERR;
	}

	pthread_mutex_unlock(&ua_intl.backup_lock);
	return ret;

}

int ua_backup_package(char* type, char* pkgName, char* version)
{
	int ret;
	int free_update = 0;
	int free_backup = 0;

	UT_array ri_list;
	ua_component_context_t* uacc = NULL;

	for (int i = 0; i < ua_intl.n_uah; i++) {
		const char* type = (ua_intl.uah + i)->type_handler;
		utarray_init(&ri_list, &ut_ptr_icd);
		query_hash_tree(ri_tree, 0, type, 0, &ri_list, 1);
		int l = utarray_len(&ri_list);
		for (int j = 0; j < l; j++) {
			runner_info_t* ri          = *(runner_info_t**) utarray_eltptr(&ri_list, j);
			ua_component_context_t* cc = &ri->component;
			if (!strcmp(cc->type, type)) {
				uacc = cc;
				break;
			}
		}
	}

	if (!uacc) {
		DBG("Couldn't find ua component with type %s", type);
		return E_UA_ERR;
	}

	if (!uacc->update_manifest && S(ua_intl.cache_dir)) {
		uacc->update_manifest = JOIN(ua_intl.cache_dir, pkgName, MANIFEST_PKG);
		free_update           = 1;
	}

	if (!uacc->backup_manifest && S(ua_intl.backup_dir)) {
		uacc->backup_manifest = JOIN(ua_intl.backup_dir, "backup", pkgName, MANIFEST_PKG);
		free_backup           = 1;
	}

	ret = handler_backup_actions(uacc, pkgName, version);

	if (free_update)
		Z_FREE(uacc->update_manifest);
	if (free_backup)
		Z_FREE(uacc->backup_manifest);

	return ret;

}

void handler_set_internal_state(ua_internal_state_t st)
{
	ua_intl.state = st;
}

int ua_rollback_disabled(const char* pkgName)
{
	rollback_crtl_t* rbc = NULL;

	HASH_FIND_STR(ua_intl.rb_crtl, pkgName, rbc);
	if (rbc) {
		DBG("rollback_disabled flag for %s is set (%d)", rbc->pkg_name, rbc->rb_disable);
		return rbc->rb_disable;
	}

	return 0;
}

static int send_sequence_query(void)
{
	int err                 = E_UA_OK;
	json_object* jObject    = json_object_new_object();
	json_object* bodyObject = json_object_new_object();

	if (ua_intl.query_reply_id == NULL)
		ua_intl.query_reply_id = randstring(REPLY_ID_STR_LEN);

	if (ua_intl.query_reply_id) {
		json_object_object_add(jObject, "type", json_object_new_string(QUERY_SEQUENCE));
		json_object_object_add(jObject, "body", bodyObject);
		json_object_object_add(jObject, "reply-id", json_object_new_string(ua_intl.query_reply_id ));
		json_object_object_add(bodyObject, "domain", json_object_new_string("change-notifications" ));
		err = ua_send_message(jObject);
	}

	json_object_put(jObject);

	return err;
}

void ua_rollback_control(const char* pkgName, int disable)
{
	rollback_crtl_t* rbc = NULL;

	HASH_FIND_STR(ua_intl.rb_crtl, pkgName, rbc);
	if (rbc) {
		if (!disable) {
			DBG("Re-enable rollback for %s", rbc->pkg_name);
			HASH_DEL(ua_intl.rb_crtl, rbc);
			f_free(rbc->pkg_name);
			free(rbc);
		}

	} else {
		if (disable) {
			DBG("Disable rollback for %s", pkgName);
			rbc             = (rollback_crtl_t*)malloc(sizeof(rollback_crtl_t));
			rbc->pkg_name   = f_strdup(pkgName);;
			rbc->rb_disable = disable;
			HASH_ADD_KEYPTR( hh, ua_intl.rb_crtl, rbc->pkg_name, strlen(rbc->pkg_name), rbc);
		}
	}

}

#ifdef SUPPORT_UA_DOWNLOAD
void ua_verify_ca_file_init(const char* path)
{
	struct dirent* ent = 0;
	DIR* dir           = 0;
	int count          = 0;

	if (!path) {
		return;
	}

	if (0 != access(path, F_OK)) {
		return;
	}

	dir = opendir(path);
	if (!dir) {
		return;
	}

	while ((ent = readdir(dir))) {
		if (strcmp(ent->d_name,".") == 0 || strcmp(ent->d_name,"..") == 0) {
			continue;
		}

		if (ent->d_type == DT_DIR) {
			continue;
		} else {
			ua_intl.verify_ca_file[count] = f_asprintf("%s/%s", path, ent->d_name);
			DBG("ua_verify_ca_file_init ca file: %s", ua_intl.verify_ca_file[count]);
			++count;
			if (count >= MAX_VERIFY_CA_COUNT) {
				break;
			}
		}
	}

	closedir(dir);
}

static void process_start_download(ua_component_context_t* uacc, json_object* jsonObj)
{
	pkg_info_t pkgInfo = {0};

	if (!get_pkg_type_from_json(jsonObj, &pkgInfo.type) &&
	    !get_pkg_name_from_json(jsonObj, &pkgInfo.name) &&
	    !get_pkg_version_from_json(jsonObj, &pkgInfo.version) &&
	    !get_pkg_version_item_from_json(jsonObj, pkgInfo.version, &pkgInfo.vi)) {
		ua_dl_start_download(&pkgInfo);
	}
}

static void process_query_trust(ua_component_context_t* uacc, json_object* jsonObj)
{
	char* replyTo = NULL;

	if (!get_replyto_from_json(jsonObj, &replyTo) &&
	    ua_intl.update_status_info.reply_id &&
	    !strcmp(replyTo, ua_intl.update_status_info.reply_id)) {
		free(ua_intl.update_status_info.reply_id);
		ua_intl.update_status_info.reply_id = NULL;

		ua_dl_trust_t dlt = {0};
		if (E_UA_OK != get_trust_info_from_json(jsonObj, &dlt)) {
			DBG("Error parsing query trust.");
		}

		ua_dl_set_trust_info(&dlt);
	}
}

int send_dl_report(pkg_info_t* pkgInfo, ua_dl_info_t dl_info, int is_done)
{
	int err = E_UA_OK;

	do {
		BOLT_IF(!pkgInfo || !S(pkgInfo->name) || !S(pkgInfo->version) || !S(pkgInfo->type),
		        E_UA_ARG, "download report info invalid");

		json_object* pkgObject = json_object_new_object();
		json_object_object_add(pkgObject, "name", json_object_new_string(pkgInfo->name));
		json_object_object_add(pkgObject, "version", json_object_new_string(pkgInfo->version));
		json_object_object_add(pkgObject, "type", json_object_new_string(pkgInfo->type));
		json_object_object_add(pkgObject, "status", json_object_new_string("DOWNLOAD_REPORT"));

		json_object* versionObject = json_object_new_object();
		json_object* verListObject = json_object_new_object();
		json_object* dlInfoObject  = json_object_new_object();

		/*
		   Completed: no "error" property, "no-download" set to false or missing, "completed-download" set to true
		   Failed: "error" property must be set, "no-download" and "completed-download" must be set to false or missing
		   Failed, and should no longer be attempted: "error" property must be set, "no-download" property must be set to true,
		     "completed-download" must be set to false or missing
		 */
		json_object_object_add(dlInfoObject, "total-bytes", json_object_new_int(dl_info.total_bytes));
		json_object_object_add(dlInfoObject, "downloaded-bytes", json_object_new_int(dl_info.downloaded_bytes));
		json_object_object_add(dlInfoObject, "expected-bytes", json_object_new_int(dl_info.expected_bytes));
		json_object_object_add(dlInfoObject, "no-download", json_object_new_boolean(dl_info.no_download));
		json_object_object_add(dlInfoObject, "completed-download",
		                       json_object_new_boolean(dl_info.completed_download && is_done));
		if (dl_info.error != NULL)
			json_object_object_add(dlInfoObject, "error", json_object_new_string(dl_info.error));

		json_object_object_add(versionObject, "download-report", dlInfoObject);

		json_object_object_add(verListObject, pkgInfo->version, versionObject);
		json_object_object_add(pkgObject, "version-list", verListObject);

		json_object* bodyObject = json_object_new_object();
		json_object_object_add(bodyObject, "package", pkgObject);


		json_object* jObject = json_object_new_object();
		json_object_object_add(jObject, "type", json_object_new_string(UPDATE_STATUS));
		json_object_object_add(jObject, "body", bodyObject);

		DBG("Sending : %s", json_object_to_json_string(jObject));

		if ((err = xl4bus_client_send_msg(json_object_to_json_string(jObject))) != E_UA_OK)
			DBG("Failed to send download-report to dmc");

		json_object_put(jObject);
	} while (0);

	return err;
}

int send_query_trust(void)
{
	int err                 = E_UA_OK;
	json_object* jObject    = json_object_new_object();
	json_object* bodyObject = json_object_new_object();

	ua_intl.update_status_info.reply_id = randstring(REPLY_ID_STR_LEN);
	if (ua_intl.update_status_info.reply_id) {
		json_object_object_add(jObject, "type", json_object_new_string(QUERY_TRUST));
		json_object_object_add(jObject, "body", bodyObject);
		json_object_object_add(jObject, "reply-id", json_object_new_string(ua_intl.update_status_info.reply_id ));
		err = ua_send_message(jObject);
	}
	json_object_put(jObject);

	return err;
}
#endif

