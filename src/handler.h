/*
 * handler.h
 */

#ifndef _UA_HANDLER_H_
#define _UA_HANDLER_H_

#include "common.h"

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


typedef struct pkg_info {
	char* type;
	char* name;
	char* version;
	char* rollback_version;
	json_object* rollback_versions;

} pkg_info_t;

typedef struct pkg_file {
	char* version;
	char* file;
	char sha256b64[SHA256_B64_LENGTH];
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

typedef void (*process_f)(ua_routine_t*, json_object*);

typedef struct worker_info {
	pthread_t worker_thread;
	int worker_running;
	process_f worker_func;
	json_object* worker_jobj;

} worker_info_t;

typedef struct ua_unit {
	char* type;
	ua_routine_t* uar;
	worker_info_t worker;

} ua_unit_t;

typedef struct runner_info {
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int run;
	incoming_msg_t* queue;
	ua_unit_t unit;

} runner_info_t;

typedef struct runner_info_hash_tree {
	char* key;
	UT_hash_handle hh;
	UT_array items;
	struct runner_info_hash_tree* nodes;
	struct runner_info_hash_tree* parent;

} runner_info_hash_tree_t;

typedef enum ua_state {
	UA_STATE_UNKNOWN,
	UA_STATE_IDLE_INIT,
	UA_STATE_DOWNLOADING_UPDATE,
	UA_STATE_PREPARING_UPDATE,
	UA_STATE_UPDATE_STARTED,
	UA_STATE_UPDATE_DONE,

}ua_state_t;

typedef struct async_update_status {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	char* reply_id;
	int successful;

}async_update_status_t;

typedef struct ua_internal {
	ua_state_t state;
	int delta;
	char* cache_dir;
	char* backup_dir;
	char* prepare_version;
	async_update_status_t update_status_info;

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


void handle_status(int status);
void handle_delivered(const char* msg, int ok);
void handle_presence(int connected, int disconnected);
void handle_message(const char* type, const char* msg, size_t len);

void free_pkg_file(pkg_file_t* pkgFile);

#endif /* _UA_HANDLER_H_ */
