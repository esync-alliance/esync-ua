/*
 * client.h
 */

#ifndef _UA_CLIENT_H_
#define _UA_CLIENT_H_

#include <libxl4bus/low_level.h>
#include <libxl4bus/high_level.h>

int xl4bus_client_start(char * url, char * uaType);
void xl4bus_client_stop(void);
int xl4bus_client_send_msg(char * message);

#endif /* _UA_CLIENT_H_ */
