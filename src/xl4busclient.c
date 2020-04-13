/*
 * xl4busclient.c
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libxl4bus/low_level.h>
#include <libxl4bus/high_level.h>
#include <libxl4bus/types.h>
#include "xl4busclient.h"
#include "utarray.h"
#include "uthash.h"
#include "handler.h"
#include "debug.h"

#define DEBUG 5

static void on_xl4bus_message(struct xl4bus_client* client, xl4bus_message_t* msg);
static void on_xl4bus_status(struct xl4bus_client* client, xl4bus_client_condition_t cond);
static void on_xl4bus_delivered(struct xl4bus_client* client, xl4bus_message_t* msg, void* arg, int ok);
static void on_xl4bus_presence(struct xl4bus_client* client, xl4bus_address_t* connected, xl4bus_address_t* disconnected);
static void on_xl4bus_reconnect(struct xl4bus_client* client);
static int load_xl4_x509_creds(xl4bus_identity_t* identity, char* dir);
static int load_simple_x509_creds(xl4bus_identity_t* identity, char* p_key_path,
                                  char* cert_path, char* ca_path, char* password);
static void release_identity(xl4bus_identity_t* identity);
static xl4bus_asn1_t* load_full(char* path);
static char* simple_password (struct xl4bus_X509v3_Identity* id);

static xl4bus_client_t m_xl4bus_clt = {0};
static char* m_xl4bus_url = NULL;

void debug_print(const char* msg)
{
	if (ua_debug == DEBUG)
		printf("%s\n", msg);

}

int xl4bus_client_init(char* url, char* cert_dir)
{
	int err = E_UA_OK;
	xl4bus_ll_cfg_t ll_cfg;

	do {
		m_xl4bus_url = f_strdup(url);

		memset(&ll_cfg, 0, sizeof(xl4bus_ll_cfg_t));
		ll_cfg.debug_f = debug_print;

		BOLT_IF(xl4bus_init_ll(&ll_cfg), E_UA_ERR, "failed to initialize xl4bus");

		memset(&m_xl4bus_clt, 0, sizeof(xl4bus_client_t));
#if XL4_PROVIDE_THREADS
		m_xl4bus_clt.use_internal_thread = 1;
#endif
		m_xl4bus_clt.on_status           = on_xl4bus_status;
		m_xl4bus_clt.on_delivered        = on_xl4bus_delivered;
		m_xl4bus_clt.on_message          = on_xl4bus_message;
		m_xl4bus_clt.on_presence         = on_xl4bus_presence;
		m_xl4bus_clt.on_release          = on_xl4bus_reconnect;

#ifdef USE_XL4BUS_TRUST
		char* groups[] = {};

		m_xl4bus_clt.identity.type               = XL4BIT_TRUST;
		m_xl4bus_clt.identity.trust.groups       = groups;
		m_xl4bus_clt.identity.trust.group_cnt    = sizeof(groups) / sizeof(groups[0]);
		m_xl4bus_clt.identity.trust.is_broker    = 0;
		m_xl4bus_clt.identity.trust.is_dm_client = 0;
		m_xl4bus_clt.identity.trust.update_agent = cfg->ua_type;
#else

		A_INFO_MSG("Loading x509 credentials from : %s", cert_dir);
		BOLT_IF(load_xl4_x509_creds(&m_xl4bus_clt.identity, cert_dir), E_UA_ERR, "x509 credentials load failed!");

#endif
		if (m_xl4bus_url)
			BOLT_SUB(xl4bus_init_client(&m_xl4bus_clt, m_xl4bus_url));

	} while (0);

	if (err) {
		Z_FREE(m_xl4bus_url);
		m_xl4bus_url = NULL;
	}

	return err;

}


int xl4bus_client_stop(void)
{
	Z_FREE(m_xl4bus_url);
	return xl4bus_stop_client(&m_xl4bus_clt);

}


int xl4bus_client_send_msg(const char* message)
{
	int err = E_XL4BUS_OK;
	char* msg;
	xl4bus_message_t* xl4bus_msg;
	xl4bus_address_t* addr = 0;

	do {
		BOLT_MEM(msg = f_strdup(message));

		BOLT_SUB(xl4bus_chain_address(&addr, XL4BAT_SPECIAL, XL4BAS_DM_CLIENT));
		BOLT_SUB(xl4bus_chain_address(&addr, XL4BAT_GROUP, "update-listener", 1));

		BOLT_MALLOC(xl4bus_msg, sizeof(xl4bus_message_t));
		xl4bus_msg->address      = addr;
		xl4bus_msg->content_type = "application/json";
		xl4bus_msg->data         = msg;
		xl4bus_msg->data_len     = strlen((char*) xl4bus_msg->data) + 1;

		BOLT_SUB(xl4bus_send_message(&m_xl4bus_clt, xl4bus_msg, 0));

	} while (0);

	if (err != E_XL4BUS_OK) {
		xl4bus_free_address(addr, 1);
		A_ERROR_MSG("Error sending message: %s", message);
	}

	return err;

}


int xl4bus_client_send_msg_to_addr(const char* message, xl4bus_address_t* xl4_address)
{
	int err = E_XL4BUS_OK;
	char* msg;
	xl4bus_message_t* xl4bus_msg = 0;

	do {
		BOLT_MEM(msg = f_strdup(message));

		BOLT_MALLOC(xl4bus_msg, sizeof(xl4bus_message_t));
		xl4bus_msg->address      = xl4_address;
		xl4bus_msg->content_type = "application/json";
		xl4bus_msg->data         = msg;
		xl4bus_msg->data_len     = strlen((char*) xl4bus_msg->data) + 1;

		BOLT_SUB(xl4bus_send_message(&m_xl4bus_clt, xl4bus_msg, 0));

	} while (0);

	if (err != E_XL4BUS_OK) {
		xl4bus_free_address(xl4_address, 1);
		A_ERROR_MSG("Error sending message: %s", message);
	}

	return err;
}

char* addr_to_string(xl4bus_address_t* addr)
{
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


static void on_xl4bus_message(xl4bus_client_t* client, xl4bus_message_t* msg)
{
	if (msg && msg->data) {
		if (!strcmp(msg->content_type, "application/json")) {
			if (msg->data_len <= 0) {
				A_INFO_MSG("Empty incoming message (%d)?", msg->data_len);
			} else {
				for (xl4bus_address_t* a = msg->address; a; a=a->next) {
					if (a->type == XL4BAT_UPDATE_AGENT)
						handle_message(a->update_agent, msg->data, msg->data_len);
				}
			}
		} else {
			A_INFO_MSG("Skipping message with an unsupported content type %s", NULL_STR(msg->content_type));
		}
	}

}


static void on_xl4bus_status(xl4bus_client_t* client, xl4bus_client_condition_t cond)
{
	handle_status((int) cond);

}


static void on_xl4bus_delivered(xl4bus_client_t* client, xl4bus_message_t* msg, void* arg, int ok)
{
	handle_delivered(msg->data, ok);

	xl4bus_free_address(msg->address, 1);
	free((char*) msg->data);
	free(msg);

}


static void on_xl4bus_presence(xl4bus_client_t* client, xl4bus_address_t* connected, xl4bus_address_t* disconnected)
{
	int num_connected    = 0;
	int num_disconnected = 0;
	char* as;
	esync_bus_conn_state_t connection_state = 0;

	/* connection_state is mainly used for detecting whether eSync Client is
	   connected to eSync bus to support sending installation status if reboot
	   is required after update. In the context of eSync bus design, the
	   value of connection_state will be set appropriately in either loop for
	   handle_presence.
	 */
	for (xl4bus_address_t* a = connected; a; a=a->next) {
		as = addr_to_string(a);
		A_INFO_MSG("CONNECTED: %s", as);
		num_connected++;
		free(as);
		if (a->type == XL4BAT_SPECIAL) {
			if (a->special == XL4BAS_DM_CLIENT)
				connection_state = BUS_CONN_DMC_CONNECTED;
			else if (a->special == XL4BAS_DM_BROKER)
				connection_state = BUS_CONN_BROKER_CONNECTED;

		}

	}

	for (xl4bus_address_t* a = disconnected; a; a=a->next) {
		as = addr_to_string(a);
		A_INFO_MSG("DISCONNECTED: %s", as);
		num_disconnected++;
		free(as);
		if (a->type == XL4BAT_SPECIAL) {
			if (a->special == XL4BAS_DM_CLIENT)
				connection_state = BUS_CONN_DMC_NOT_CONNECTED;
			else if (a->special == XL4BAS_DM_BROKER)
				connection_state = BUS_CONN_BROKER_NOT_CONNECTED;

		}

	}

	handle_presence(num_connected, num_disconnected, connection_state);

}

static void on_xl4bus_reconnect(xl4bus_client_t* client)
{
	if (m_xl4bus_url)
		xl4bus_init_client(&m_xl4bus_clt, m_xl4bus_url);

}


static int load_xl4_x509_creds(xl4bus_identity_t* identity, char* dir)
{
	char* p_key = f_asprintf("%s/private.pem", dir);
	char* cert  = f_asprintf("%s/cert.pem", dir);
	char* ca    = f_asprintf("%s/../ca/ca.pem", dir);

	int ret = load_simple_x509_creds(identity, p_key, cert, ca, 0);

	free(p_key);
	free(cert);
	free(ca);

	return ret;

}

static int load_simple_x509_creds(xl4bus_identity_t* identity, char* p_key_path,
                                  char* cert_path, char* ca_path, char* password)
{
	memset(identity, 0, sizeof(xl4bus_identity_t));
	identity->type = XL4BIT_X509;
	int ok = 0;

	do {
		identity->x509.private_key = load_full(p_key_path);
		if (!(identity->x509.trust = f_malloc(2 * sizeof(void*)))) {
			break;
		}
		if (!(identity->x509.chain = f_malloc(2 * sizeof(void*)))) {
			break;
		}
		if (!(identity->x509.trust[0] = load_full(ca_path))) {
			break;
		}
		if (!(identity->x509.chain[0] = load_full(cert_path))) {
			break;
		}
		if (password) {
			identity->x509.custom   = f_strdup(password);
			identity->x509.password = simple_password;
		}

		ok = 1;

	} while (0);

	if (!ok) {
		release_identity(identity);
	}

	return !ok;

}

static void release_identity(xl4bus_identity_t* identity)
{
	if (identity->type == XL4BIT_X509) {
		if (identity->x509.trust) {
			for (xl4bus_asn1_t** buf = identity->x509.trust; *buf; buf++) {
				free((*buf)->buf.data);
			}
			free(identity->x509.trust);
		}

		if (identity->x509.chain) {
			for (xl4bus_asn1_t** buf = identity->x509.chain; *buf; buf++) {
				free((*buf)->buf.data);
			}
			free(identity->x509.chain);
		}

		free(identity->x509.custom);

		identity->type = XL4BIT_INVALID;

	}

}

static xl4bus_asn1_t* load_full(char* path)
{
	int fd = open(path, O_RDONLY);
	int ok = 0;

	if (fd < 0) {
		A_ERROR_MSG("Failed to open %s", path);
		return 0;
	}

	xl4bus_asn1_t* buf = f_malloc(sizeof(xl4bus_asn1_t));
	buf->enc = XL4BUS_ASN1ENC_PEM;

	do {
		off_t size = lseek(fd, 0, SEEK_END);
		if (size == (off_t)-1) {
			A_ERROR_MSG("Failed to seek %s", path);
			break;
		}
		if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
			A_ERROR_MSG("Failed to rewind %s", path);
			break;
		}

		buf->buf.len = (size_t) (size + 1);
		uint8_t* ptr = buf->buf.data = f_malloc(buf->buf.len);
		while (size) {
			ssize_t rd = read(fd, ptr, (size_t) size);
			if (rd < 0) {
				A_ERROR_MSG("Failed to read from %s", path);
				break;
			}
			if (!rd) {
				A_INFO_MSG("Premature EOF reading %d, file declared %d bytes, read %d bytes, remaining %d bytes",
				    path, buf->buf.len-1, ptr-buf->buf.data, size);
				break;
			}
			size -= rd;
			ptr  += rd;
		}

		if (!size) { ok = 1; }

	} while (0);

	close(fd);

	if (!ok) {
		free(buf->buf.data);
		free(buf);
		return 0;
	}

	return buf;

}

static char* simple_password (struct xl4bus_X509v3_Identity* id)
{
	return id->custom;

}

const char* xl4bus_get_version()
{
	return xl4bus_version();

}
