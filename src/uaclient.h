/*
 * uaclient.h
 */

#ifndef _UA_UACLIENT_H_
#define _UA_UACLIENT_H_

#include "common.h"


int xl4bus_client_start(char * url, char * certDir, char * uaType);

void xl4bus_client_stop(void);

int xl4bus_client_send_msg(const char * message);

#endif /* _UA_UACLIENT_H_ */
