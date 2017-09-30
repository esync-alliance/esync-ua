/*
 * handler.h
 */

#ifndef _UA_HANDLER_H_
#define _UA_HANDLER_H_

#include "updateagent.h"

#define QUERY_PACKAGE       "xl4.query-package"
#define READY_DOWNLOAD      "xl4.ready-download"
#define READY_UPDATE        "xl4.ready-update"
#define DOWNLOAD_REPORT     "xl4.download-report"
#define SOTA_REPORT         "xl4.sota-report"
#define LOG_REPORT          "xl4.log-report"
#define UPDATE_STATUS       "xl4.update-status"


int ua_handler_start(update_agent_t * ua);
void ua_handler_stop(void);

void handle_status(int status);
void handle_delivered(const char * msg, int ok);
void handle_presence(int connected, int disconnected);
void handle_message(const char * msg);

#endif /* _UA_HANDLER_H_ */
