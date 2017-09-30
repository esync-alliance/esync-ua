/*
 * client.c
 */

#include "config.h"
#include "client.h"
#include "debug.h"
#include "handler.h"

static void on_xl4bus_message(struct xl4bus_client *client, xl4bus_message_t *msg);
static void on_xl4bus_status(struct xl4bus_client * client, xl4bus_client_condition_t cond);
static void on_xl4bus_delivered(struct xl4bus_client * client, xl4bus_message_t * msg, void * arg, int ok);
static void on_xl4bus_presence(struct xl4bus_client * client, xl4bus_address_t * connected, xl4bus_address_t * disconnected);

xl4bus_client_t xl4bus_client;
char *groups[] = {};


void dbg_print(const char * msg) {

	DBG("%s\n", msg);
}


int xl4bus_client_start(char * url, char * uaType) {

    xl4bus_ll_cfg_t ll_cfg;
    memset(&ll_cfg, 0, sizeof(xl4bus_ll_cfg_t));
    ll_cfg.debug_f = dbg_print;
    xl4bus_init_ll(&ll_cfg);

    memset(&xl4bus_client, 0, sizeof(xl4bus_client_t));

    xl4bus_client.use_internal_thread = 1;
    xl4bus_client.on_status = on_xl4bus_status;
    xl4bus_client.on_delivered = on_xl4bus_delivered;
    xl4bus_client.on_message = on_xl4bus_message;
    xl4bus_client.on_presence = on_xl4bus_presence;

    xl4bus_client.identity.type = XL4BIT_TRUST;
    xl4bus_client.identity.trust.groups = groups;
    xl4bus_client.identity.trust.group_cnt = sizeof(groups) / sizeof(groups[0]);
    xl4bus_client.identity.trust.is_broker = 0;
    xl4bus_client.identity.trust.is_dm_client = 0;
    xl4bus_client.identity.trust.update_agent = uaType;
    return xl4bus_init_client(&xl4bus_client, url);
}


void xl4bus_client_stop(void) {

    xl4bus_stop_client(&xl4bus_client);
}


int xl4bus_client_send_msg(const char * message) {

	xl4bus_message_t xl4bus_message;

    xl4bus_address_t addr = {
            .type = XL4BAT_SPECIAL,
            .special = XL4BAS_DM_CLIENT,
            .next = 0
    };

    xl4bus_message.address = &addr;
    xl4bus_message.content_type = "application/json";
    xl4bus_message.data = message;
    xl4bus_message.data_len = strlen((char *) xl4bus_message.data) + 1;

    return xl4bus_send_message(&xl4bus_client, &xl4bus_message, 0);
}


char * addr_to_string(xl4bus_address_t * addr) {

	int rc = 0;
	char *ret = 0;
    switch (addr->type) {
        case XL4BAT_SPECIAL:
            switch (addr->special) {
                case XL4BAS_DM_CLIENT:
                    rc = asprintf(&ret,"%s", "<DM-CLIENT>");
                    break;
                case XL4BAS_DM_BROKER:
                	rc = asprintf(&ret,"%s", "<BROKER>");
                	break;
                default:
                	rc = asprintf(&ret,"<UNKNOWN SPECIAL %d>", addr->special);
                	break;
            }
            break;
        case XL4BAT_UPDATE_AGENT:
        	rc = asprintf(&ret, "<UA: %s> ", addr->update_agent);
        	break;
        case XL4BAT_GROUP:
        	rc = asprintf(&ret,"<GRP: %s>", addr->group);
        	break;
        default:
        	rc = asprintf(&ret,"<UNKNOWN TYPE %d>", addr->type);
        	break;
    }
    if(rc > 0)
    	return ret;
    else
    	return NULL;
}


static void on_xl4bus_message(struct xl4bus_client *client, xl4bus_message_t *msg) {

    if(msg && msg->data)
    {
        handle_message(msg->data);
    }
}


static void on_xl4bus_status(struct xl4bus_client * client, xl4bus_client_condition_t cond) {

    handle_status((int) cond);
}


static void on_xl4bus_delivered(struct xl4bus_client * client, xl4bus_message_t * msg, void * arg, int ok) {

    handle_delivered(msg->data, ok);
}


static void on_xl4bus_presence(struct xl4bus_client * client, xl4bus_address_t * connected, xl4bus_address_t * disconnected) {

    int num_connected = 0;
    int num_disconnected = 0;
    char * as;
    for (xl4bus_address_t * a = connected; a; a=a->next) {
        as = addr_to_string(a);
        DBG("CONNECTED: %s\n", as);
        num_connected++;
        free(as);
    }
    for (xl4bus_address_t * a = disconnected; a; a=a->next) {
        as = addr_to_string(a);
        DBG("DISCONNECTED: %s\n", as);
        num_disconnected++;
        free(as);
    }
    handle_presence(num_connected, num_disconnected);
}
