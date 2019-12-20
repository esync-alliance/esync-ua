/*
 * handler.h
 */

#ifndef UA_HANDLER_H_
#define UA_HANDLER_H_

#include "misc.h"
#include "json-c-rename.h"
#include <json.h>
#include "xl4ua.h"
#include "uthash.h"
#include "utlist.h"
#include "utarray.h"
#ifdef SUPPORT_UA_DOWNLOAD
#include "eua_json.h"
#endif
#include <pthread.h>
#define MSG_TIMEOUT     10

#define QUERY_PACKAGE   "xl4.query-package"
#define READY_DOWNLOAD  "xl4.ready-download"
#define READY_UPDATE    "xl4.ready-update"
#define PREPARE_UPDATE  "xl4.prepare-update"
#define CONFIRM_UPDATE  "xl4.confirm-update"
#define DOWNLOAD_REPORT "xl4.download-report"
#define SOTA_REPORT     "xl4.sota-report"
#define LOG_REPORT      "xl4.log-report"
#define UPDATE_STATUS   "xl4.update-status"
#define UPDATE_REPORT   "xl4.update-report"

#ifdef SUPPORT_UA_DOWNLOAD
#define START_DOWNLOAD "xl4.start-download"
#define QUERY_TRUST    "xl4.query-trust"
#endif

typedef struct pkg_info {
	char* type;
	char* name;
	char* version;
	char* rollback_version;
	json_object* rollback_versions;
	#ifdef SUPPORT_UA_DOWNLOAD
	version_item_t vi;
	#endif
} pkg_info_t;

typedef struct pkg_file {
	char* version;
	char* file;
	char sha256b64[SHA256_B64_LENGTH];
	char delta_sha256b64[SHA256_B64_LENGTH];
	char sha_of_sha[SHA256_B64_LENGTH];
	int downloaded;
	int rollback_order;

	struct pkg_file* next;
	struct pkg_file* prev;

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

typedef enum ua_state {
	UA_STATE_UNKNOWN,
	UA_STATE_IDLE_INIT,
	UA_STATE_READY_DOWNLOAD_STARTED,
	UA_STATE_READY_DOWNLOAD_DONE,
	UA_STATE_PREPARE_UPDATE_STARTED,
	UA_STATE_PREPARE_UPDATE_DONE,
	UA_STATE_READY_UPDATE_STARTED,
	UA_STATE_READY_UPDATE_DONE,

}ua_state_t;

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
	UAI_STATE_RESUME_STARTED,
	UAI_STATE_RESUME_DONE,

}ua_internal_state_t;

#ifdef SUPPORT_UA_DOWNLOAD
#define MAX_VERIFY_CA_COUNT 5
#endif
typedef struct ua_internal {
	int delta;
	char* cache_dir;
	char* backup_dir;
	char* record_file;
	int esync_bus_conn_status;
	ua_internal_state_t state;
	int reboot_support;
	ua_handler_t* uah;
	int n_uah;
	int package_verification_disabled;

	uint32_t backup_source;
	pthread_mutex_t backup_lock;

#ifdef SUPPORT_UA_DOWNLOAD
	int ua_download_required;
	char* ua_downloaded_filename;
	char* ua_dl_dir;
	int ua_dl_connect_timout_ms;
	int ua_dl_download_timeout_ms;
	char* ua_dl_ca_file;
	char* verify_ca_file[MAX_VERIFY_CA_COUNT];
	async_update_status_t update_status_info;
#endif
} ua_internal_t;

typedef enum update_err {
	UE_NONE,
	UE_INCREMENTAL_FAILED,
	UE_UPDATE_INCAPABLE,
	UE_TERMINAL_FAILURE
} update_err_t;

typedef enum update_stage {
	US_TRANSFER,
	US_INSTALL
} update_stage_t;

typedef enum update_rollback {
	URB_NONE,
	URB_DMC_INITIATED,  /* ready-update has rollback-version. */
	URB_UA_INITIATED,   /* ready-update has rollback-versions. */
	URB_UA_LOCAL_BACKUP /* local backup folder supports rollback. */

} update_rollback_t;

typedef struct ua_component_context {
	char* type; //Registered hanlder type.
	json_object* cur_msg;
	ua_state_t state;
	ua_routine_t* uar;
	worker_info_t worker;
	async_update_status_t update_status_info;
	pkg_file_t update_file_info;
	pkg_info_t update_pkg;
	update_rollback_t rb_type;
	update_err_t update_error;
	char* update_manifest;
	char* backup_manifest;
	char* record_file;
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

void free_pkg_file(pkg_file_t* pkgFile);

install_state_t prepare_install_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile, int bck, pkg_file_t* updateFile, update_err_t* ue);
install_state_t pre_update_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile);
install_state_t update_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo, pkg_file_t* pkgFile);
void post_update_action(ua_component_context_t* uacc, pkg_info_t* pkgInfo);

void handler_set_internal_state(ua_internal_state_t st);
void send_install_status(pkg_info_t* pkgInfo, install_state_t state, pkg_file_t* pkgFile, update_err_t ue);
int handler_backup_actions(ua_component_context_t* uacc, char* pkgName, char* version);
int get_local_next_rollback_version(char* manifest, char* currentVer, char** nextVer);
void query_hash_tree(runner_info_hash_tree_t* current, runner_info_t* ri, const char* ua_type, int is_delete, UT_array* gather, int tip);

#ifdef SUPPORT_UA_DOWNLOAD
int send_dl_report(pkg_info_t* pkgInfo, ua_dl_info_t dl_info, int is_done);
int send_query_trust(void);
#endif

#endif /* UA_HANDLER_H_ */
