/*
 * xl4busclient.h
 */

#ifndef _UA_XL4BUSCLIENT_H_
#define _UA_XL4BUSCLIENT_H_

#include "common.h"


int xl4bus_client_init(ua_cfg_t * cfg);

int xl4bus_client_stop(void);

int xl4bus_client_send_msg(const char * message);

#endif /* _UA_XL4BUSCLIENT_H_ */