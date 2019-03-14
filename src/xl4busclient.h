/*
 * xl4busclient.h
 */

#ifndef _UA_XL4BUSCLIENT_H_
#define _UA_XL4BUSCLIENT_H_

#include "common.h"


int xl4bus_client_init(char* url, char* cert_dir);

int xl4bus_client_stop(void);

int xl4bus_client_send_msg(const char* message);

const char* xl4bus_get_version(void);

#endif /* _UA_XL4BUSCLIENT_H_ */
