/*
 * handler.h
 */

#ifndef _UA_HANDLER_H_
#define _UA_HANDLER_H_

#include "common.h"


#define QUERY_PACKAGE       "xl4.query-package"
#define READY_DOWNLOAD      "xl4.ready-download"
#define READY_UPDATE        "xl4.ready-update"
#define DOWNLOAD_REPORT     "xl4.download-report"
#define SOTA_REPORT         "xl4.sota-report"
#define LOG_REPORT          "xl4.log-report"
#define UPDATE_STATUS       "xl4.update-status"


typedef struct incoming_msg {

    char * msg;
    size_t msg_len;

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


void handle_status(int status);
void handle_delivered(const char * msg, int ok);
void handle_presence(int connected, int disconnected);
void handle_message(const char * type, const char * msg, size_t len);

#endif /* _UA_HANDLER_H_ */
