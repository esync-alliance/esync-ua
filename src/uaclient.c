/*
 * uaclient.c
 */

#include "config.h"
#include "uaclient.h"
#include "debug.h"
#include "handler.h"
#include "common.h"

static void on_xl4bus_message(struct xl4bus_client *client, xl4bus_message_t *msg);
static void on_xl4bus_status(struct xl4bus_client * client, xl4bus_client_condition_t cond);
static void on_xl4bus_delivered(struct xl4bus_client * client, xl4bus_message_t * msg, void * arg, int ok);
static void on_xl4bus_presence(struct xl4bus_client * client, xl4bus_address_t * connected, xl4bus_address_t * disconnected);
static void on_xl4bus_reconnect(struct xl4bus_client * client);

xl4bus_client_t m_xl4bus_clt;
char * m_xl4bus_url;


int xl4bus_client_start(char * url, char * certDir, char * uaType) {

    int err = E_UA_OK;
    xl4bus_ll_cfg_t ll_cfg;

    do {

    	m_xl4bus_url = url;

		memset(&ll_cfg, 0, sizeof(xl4bus_ll_cfg_t));
		ll_cfg.debug_f = debug_print;

		BOLT_IF(xl4bus_init_ll(&ll_cfg), E_UA_ERR, "failed to initialize xl4bus");

		memset(&m_xl4bus_clt, 0, sizeof(xl4bus_client_t));
		m_xl4bus_clt.use_internal_thread = 1;
		m_xl4bus_clt.on_status = on_xl4bus_status;
		m_xl4bus_clt.on_delivered = on_xl4bus_delivered;
		m_xl4bus_clt.on_message = on_xl4bus_message;
		m_xl4bus_clt.on_presence = on_xl4bus_presence;
		m_xl4bus_clt.on_release = on_xl4bus_reconnect;

#ifdef USE_XL4BUS_TRUST
		char *groups[] = {};

		m_xl4bus_clt.identity.type = XL4BIT_TRUST;
		m_xl4bus_clt.identity.trust.groups = groups;
		m_xl4bus_clt.identity.trust.group_cnt = sizeof(groups) / sizeof(groups[0]);
		m_xl4bus_clt.identity.trust.is_broker = 0;
		m_xl4bus_clt.identity.trust.is_dm_client = 0;
		m_xl4bus_clt.identity.trust.update_agent = uaType;
#else

		DBG("Loading x509 credentials from : %s", certDir);
		BOLT_IF(load_xl4_x509_creds(&m_xl4bus_clt.identity, certDir), E_UA_ERR, "x509 credentials load failed!");

#endif

		BOLT_SUB(xl4bus_init_client(&m_xl4bus_clt, m_xl4bus_url));

    } while (0);

    return err;

}


void xl4bus_client_stop(void) {

    xl4bus_stop_client(&m_xl4bus_clt);

}


int xl4bus_client_send_msg(const char * message) {

    int err = E_XL4BUS_OK;
    char * msg;
    xl4bus_message_t * xl4bus_msg;
    xl4bus_address_t * addr = 0;

    do {

		BOLT_MEM(msg = f_strdup(message));

		BOLT_SUB(xl4bus_chain_address(&addr, XL4BAT_SPECIAL, XL4BAS_DM_CLIENT));
		BOLT_SUB(xl4bus_chain_address(&addr, XL4BAT_GROUP, "update-listener", 1));

		BOLT_MALLOC(xl4bus_msg, sizeof(xl4bus_message_t));
		xl4bus_msg->address = addr;
		xl4bus_msg->content_type = "application/json";
		xl4bus_msg->data = msg;
		xl4bus_msg->data_len = strlen((char *) xl4bus_msg->data) + 1;

		BOLT_SUB(xl4bus_send_message(&m_xl4bus_clt, xl4bus_msg, 0));

    } while (0);

    if (err != E_XL4BUS_OK) {
    	xl4bus_free_address(addr, 1);
	}

	return err;

}


char *addr_to_string(xl4bus_address_t * addr) {

    switch (addr->type) {

        case XL4BAT_SPECIAL:

            switch (addr->special) {

                case XL4BAS_DM_CLIENT:
                    return f_strdup("<DM-CLIENT>");
                case XL4BAS_DM_BROKER:
                    return f_strdup("<BROKER>");
                default:
                    return f_asprintf("<UNKNOWN SPECIAL %d>", addr->special);
            }

            break;
        case XL4BAT_UPDATE_AGENT:
            return f_asprintf("<UA: %s>", addr->update_agent);
        case XL4BAT_GROUP:
            return f_asprintf("<GRP: %s>", addr->group);
        default:
            return f_asprintf("<UNKNOWN TYPE %d>", addr->type);
    }

}


static void on_xl4bus_message(struct xl4bus_client *client, xl4bus_message_t *msg) {

    if (msg && msg->data) {
        handle_message(msg->data);
    }

}


static void on_xl4bus_status(struct xl4bus_client * client, xl4bus_client_condition_t cond) {

    handle_status((int) cond);

}


static void on_xl4bus_delivered(struct xl4bus_client * client, xl4bus_message_t * msg, void * arg, int ok) {

    handle_delivered(msg->data, ok);

    xl4bus_free_address(msg->address, 1);
    free((char *) msg->data);
	free(msg);

}


static void on_xl4bus_presence(struct xl4bus_client * client, xl4bus_address_t * connected, xl4bus_address_t * disconnected) {

    int num_connected = 0;
    int num_disconnected = 0;
    char * as;

    for (xl4bus_address_t * a = connected; a; a=a->next) {
        as = addr_to_string(a);
        DBG("CONNECTED: %s", as);
        num_connected++;
        free(as);
    }
    for (xl4bus_address_t * a = disconnected; a; a=a->next) {
        as = addr_to_string(a);
        DBG("DISCONNECTED: %s", as);
        num_disconnected++;
        free(as);
    }

    handle_presence(num_connected, num_disconnected);

}

static void on_xl4bus_reconnect(struct xl4bus_client * client) {

    xl4bus_init_client(&m_xl4bus_clt, m_xl4bus_url);

}
