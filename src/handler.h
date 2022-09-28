/*
 * handler.h
 */

#ifndef UA_HANDLER_H_
#define UA_HANDLER_H_
#include "misc.h"
#include <json.h>

#ifdef LIBUA_VER_2_0
#include "esyncua.h"
#else
#include "xl4ua.h"
#endif //LIBUA_VER_2_0

#include "uthash.h"
#include "utlist.h"
#include "utarray.h"
#ifdef SUPPORT_LOGGING_INFO
#include "diagnostic.h"
#endif
#ifdef SUPPORT_UA_DOWNLOAD
#include "eua_json.h"
#endif
#include <pthread.h>
#define MSG_TIMEOUT 10


#if ESYNC_ALLIANCE
#define BMT_PREFIX "esync."
#else
#define BMT_PREFIX "xl4."
#endif

#define BMT_QUERY_PACKAGE   BMT_PREFIX "query-package"
#define BMT_READY_DOWNLOAD  BMT_PREFIX "ready-download"
#define BMT_READY_UPDATE    BMT_PREFIX "ready-update"
#define BMT_PREPARE_UPDATE  BMT_PREFIX "prepare-update"
#define BMT_CONFIRM_UPDATE  BMT_PREFIX "confirm-update"
#define BMT_DOWNLOAD_REPORT BMT_PREFIX "download-report"
#define BMT_SOTA_REPORT     BMT_PREFIX "sota-report"
#define BMT_LOG_REPORT      BMT_PREFIX "log-report"
#define BMT_UPDATE_REPORT   BMT_PREFIX "update-report"
#define BMT_UPDATE_STATUS   BMT_PREFIX "update-status"
#define BMT_QUERY_SEQUENCE  BMT_PREFIX "sequence-info"
#define BMT_SESSION_REQUEST BMT_PREFIX "session-request"
#define BMT_DOWNLOAD_POSTPONED BMT_PREFIX "download-postponed"
#define BMT_QUERY_UPDATES  BMT_PREFIX "query-updates"



#ifdef USE_XL4BUS_TRUST
	#define UACONF (uaConfig->ua_type)
#else
	#define UACONF (uaConfig->cert_dir)
#endif

#ifdef SUPPORT_UA_DOWNLOAD
#define BMT_START_DOWNLOAD BMT_PREFIX "start-download"
#define BMT_QUERY_TRUST    BMT_PREFIX "query-trust"
#endif

#ifdef SUPPORT_SIGNATURE_VERIFICATION
#define BMT_QUERY_KEY      BMT_PREFIX "query-pubkey"
#endif

typedef struct pkg_info {
	char* type;
	char* name;
	char* version;
	char* rollback_version;
	#ifdef SUPPORT_LOGGING_INFO
	char* id;
	char* stage;
	#endif
	json_object* rollback_versions;
	#ifdef SUPPORT_UA_DOWNLOAD
	char* id_dl;
	version_item_t vi;
	char* error;
	#endif
} pkg_info_t;

typedef struct pkg_file {
	struct pkg_file* next;
	struct pkg_file* prev;
	int downloaded;
	int rollback_order;
	char* version;
	char* file;
	char sha256b64[SHA256_B64_LENGTH];
	char delta_sha256b64[SHA256_B64_LENGTH];
	char sha_of_sha[SHA256_B64_LENGTH];
} pkg_file_t;

typedef struct incoming_msg {
	char* msg;
	size_t msg_len;
	uint64_t msg_ts;

	struct incoming_msg* next;
	struct incoming_msg* prev;

} incoming_msg_t;

//Forward declaration.
typedef struct ua_component_context ua_component_context_t;
typedef void (*process_f)(ua_component_context_t*, json_object*);

typedef struct worker_info {
	pthread_t worker_thread;
	int worker_running;
	process_f worker_func;
	json_object* worker_jobj;

} worker_info_t;

typedef enum esync_bus_conn_state {
	BUS_CONN_NONE,
	BUS_CONN_BROKER_NOT_CONNECTED,
	BUS_CONN_BROKER_CONNECTED,
	BUS_CONN_DMC_NOT_CONNECTED,
	BUS_CONN_DMC_CONNECTED,

}esync_bus_conn_state_t;

typedef enum ua_stage {
	UA_STATE_UNKNOWN,
	UA_STATE_IDLE_INIT,
	UA_STATE_READY_DOWNLOAD_STARTED,
	UA_STATE_READY_DOWNLOAD_DONE,
	UA_STATE_PREPARE_UPDATE_STARTED,
	UA_STATE_PREPARE_UPDATE_DONE,
	UA_STATE_READY_UPDATE_STARTED,
	UA_STATE_READY_UPDATE_DONE,
	UA_STATE_CONFIRM_UPDATE_STARTED,
	UA_STATE_CONFIRM_UPDATE_DONE,

}ua_stage_t;

typedef struct async_update_status {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	char* reply_id;
	int successful;
}async_update_status_t;

typedef struct runner_info_hash_tree {
	char* key;
	UT_hash_handle hh;
	UT_array items;
	struct runner_info_hash_tree* nodes;
	struct runner_info_hash_tree* parent;
} runner_info_hash_tree_t;

typedef enum ua_internal_state {
	UAI_STATE_NOT_KNOWN,
	UAI_STATE_INITIALIZED,
	UAI_STATE_HANDLER_REGISTERED,
	UAI_STATE_RESUME_STARTED,
	UAI_STATE_RESUME_DONE,

}ua_internal_state_t;


typedef struct comp_ctrl {
	char* pkg_name;
	int rb_disable;
	update_rollback_t rb_type;
	int enable_fake_rb_version;
	char* custom_msg;

	UT_hash_handle hh;

}comp_ctrl_t;


typedef struct comp_sequence {
	char* name;
	int num;
	UT_hash_handle hh;

}comp_sequence_t;

#ifdef SUPPORT_UA_DOWNLOAD
#define MAX_VERIFY_CA_COUNT 1
#endif
typedef struct ua_internal {
	int delta;
	char* cache_dir;
	char* backup_dir;
	char* record_file;
	#ifdef SUPPORT_LOGGING_INFO
	const char *diag_file;
	char *diag_dir;
	#endif
	int esync_bus_conn_status;
	ua_internal_state_t state;
	int reboot_support;
	ua_handler_t* uah;
	int n_uah;
	int package_verification_disabled;

	uint32_t backup_source;
	pthread_mutex_t lock;
	pthread_mutex_t backup_lock;
	comp_ctrl_t* component_ctrl;
	char* query_reply_id;
	int seq_info_valid;
	comp_sequence_t* seq_out;
	json_object* query_updates;
	char* cur_campaign_id;
	int max_retry;
	int dmc_handler_done;
	int async_query_package;
	int qp_failure_response;

#if defined(SUPPORT_UA_DOWNLOAD) || defined(SUPPORT_SIGNATURE_VERIFICATION)
	async_update_status_t update_status_info;
	char* ua_dl_dir;
#endif

#ifdef SUPPORT_UA_DOWNLOAD
	int ua_download_required;
	char* ua_downloaded_filename;
	int ua_dl_connect_timout_ms;
	int ua_dl_download_timeout_ms;
	char* ua_dl_ca_file;
	char* verify_ca_file[MAX_VERIFY_CA_COUNT];
#endif

} ua_internal_t;

typedef enum update_stage {
	US_TRANSFER,
	US_INSTALL
} update_stage_t;


typedef struct comp_state_info {
	char* pkg_name;
	char* prepared_ver;
	ua_stage_t stage;
	char* fake_rb_ver;
	char* campaign_id;
	UT_hash_handle hh;

}comp_state_info_t;

typedef struct ua_component_context {
	char* type; //Registered hanlder type.
	json_object* cur_msg;
	ua_routine_t* uar;
	worker_info_t worker;
	async_update_status_t update_status_info;
	pkg_file_t update_file_info;
	pkg_info_t update_pkg;
	update_err_t update_error;
	char* update_manifest;
	char* backup_manifest;
	char* record_file;
	comp_sequence_t* seq_in;
	comp_state_info_t* st_info;
	int is_rollback_installation;
	int cleanup_ok_after_update;
	int max_retry;
	int retry_cnt;

	void* usr_ref;

} ua_component_context_t;


typedef struct runner_info {
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int run;
	incoming_msg_t* queue;
	ua_component_context_t component;
} runner_info_t;

void handle_status(int status);
void handle_delivered(const char* msg, int ok);
void handle_presence(int connected, int disconnected, esync_bus_conn_state_t conn);
void handle_message(const char* type, const char* msg, size_t len);
install_state_t prepare_install_action(ua_component_context_t* uacc, pkg_file_t* pkgFile, int bck, pkg_file_t* updateFile, update_err_t* ue);
install_state_t pre_update_action(ua_component_context_t* uacc);
install_state_t update_action(ua_component_context_t* uacc);
void post_update_action(ua_component_context_t* uacc);

void handler_set_internal_state(ua_internal_state_t st);
int send_install_status(ua_component_context_t* uacc, install_state_t state, pkg_file_t* pkgFile, update_err_t ue);
int handler_backup_actions(ua_component_context_t* uacc, char* pkgName, char* version);
int get_local_next_rollback_version(char* manifest, char* currentVer, char** nextVer);
void query_hash_tree(runner_info_hash_tree_t* current, runner_info_t* ri, const char* ua_type, int is_delete, UT_array* gather, int tip);
int handler_chk_incoming_seq_outdated(comp_sequence_t** seq, char* name, int num);

#ifdef SUPPORT_UA_DOWNLOAD
int send_dl_report(pkg_info_t* pkgInfo, ua_dl_info_t dl_info, int is_done);
int send_query_trust(void);
#endif

#ifdef SUPPORT_SIGNATURE_VERIFICATION
int send_query_publickey(void);
#endif



#endif /* UA_HANDLER_H_ */
