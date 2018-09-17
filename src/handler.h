/*
 * handler.h
 */

#ifndef _UA_HANDLER_H_
#define _UA_HANDLER_H_

#include "common.h"

#define MSG_TIMEOUT         10

#define QUERY_PACKAGE       "xl4.query-package"
#define READY_DOWNLOAD      "xl4.ready-download"
#define READY_UPDATE        "xl4.ready-update"
#define PREPARE_UPDATE      "xl4.prepare-update"
#define CONFIRM_UPDATE      "xl4.confirm-update"
#define DOWNLOAD_REPORT     "xl4.download-report"
#define SOTA_REPORT         "xl4.sota-report"
#define LOG_REPORT          "xl4.log-report"
#define UPDATE_STATUS       "xl4.update-status"
#define UPDATE_REPORT       "xl4.update-report"


typedef struct pkg_info {

    char * type;
    char * name;
    char * version;
    char * rollback_version;
    json_object * rollback_versions;

} pkg_info_t;

typedef struct pkg_file {

    char * version;
    char * file;
    char sha256b64[SHA256_B64_LENGTH];
    int downloaded;

    struct pkg_file * next;
    struct pkg_file * prev;

} pkg_file_t;

typedef struct incoming_msg {

    char * msg;
    size_t msg_len;
    uint64_t msg_ts;

    struct incoming_msg * next;
    struct incoming_msg * prev;

} incoming_msg_t;

typedef struct runner_info {

    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int run;
    incoming_msg_t * queue;
    char * type;
    ua_routine_t * uar;
    UT_hash_handle hh;

} runner_info_t;

typedef struct ua_internal {

    int delta;
    char * cache_dir;
    char * backup_dir;

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
void handle_delivered(const char * msg, int ok);
void handle_presence(int connected, int disconnected);
void handle_message(const char * type, const char * msg, size_t len);

void free_pkg_file(pkg_file_t * pkgFile);

#endif /* _UA_HANDLER_H_ */
