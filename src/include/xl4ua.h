/*
 * xl4ua.h
 */

#ifndef XL4UA_H_
#define XL4UA_H_

#include <libxl4bus/types.h>
#include <libxl4bus/build_config.h>

typedef enum install_state {
	INSTALL_READY,
	INSTALL_IN_PROGRESS,
	INSTALL_COMPLETED,
	INSTALL_FAILED,
	INSTALL_ABORTED,
	INSTALL_ROLLBACK
} install_state_t;

typedef enum download_state {
	DOWNLOAD_POSTPONED,
	DOWNLOAD_CONSENT,
	DOWNLOAD_DENIED
} download_state_t;


#define E_UA_OK     ( 0)
#define E_UA_ERR    (-1)
#define E_UA_MEMORY (-2)
#define E_UA_ARG    (-3)
#define E_UA_SYS    (-4)

typedef enum dmclient_state {
	DMCLIENT_NOT_CONNECTED,
	DMCLIENT_CONNECTED
} dmclient_state_t;

typedef struct dmc_presence {
	int size; /* reserved for future use */
	dmclient_state_t state;
}dmc_presence_t;

/**
 * callback function for UA to return installed version string.
 * @param type, UA component handler type.
 * @param pkgName, UA component package name.
 * @param version, UA shall return current installed version of UA,
 *        libary does not change/release memory pointed by version.
 *
 * @return When UA returns E_UA_OK, library sends the version string to eSync
 *         client. If version is NULL, json null is used in version reporting.
 *         When UA returns E_UA_ERR, library sends "update-incapable"
 *         to eSync Client.
 */
typedef int (*ua_on_get_version)(const char* type, const char* pkgName, char** version);

typedef int (*ua_on_set_version)(const char* type, const char* pkgName, const char* version);

typedef install_state_t (*ua_on_pre_install)(const char* type, const char* pkgName, const char* version, const char* pkgFile);

typedef install_state_t (*ua_on_install)(const char* type, const char* pkgName, const char* version, const char* pkgFile);

typedef void (*ua_on_post_install)(const char* type, const char* pkgName);

typedef int (*ua_on_transfer_file)(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile);

typedef install_state_t (*ua_on_prepare_install)(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile);

typedef download_state_t (*ua_on_prepare_download)(const char* type, const char* pkgName, const char* version);

/**
 * callback function when eSync Client is connected to bus.
 * @param dp, pointer to dmc_presence_t structure, reserved for future use.
 *
 * @return UA returns E_UA_OK, or E_UA_ERR.
 */
typedef int (*ua_on_dmc_presence)(dmc_presence_t* dp);

#ifdef _json_h_

typedef int (*ua_on_message)(const char* msgType, json_object* message);

#else

typedef int (*ua_on_message)(const char* msgType, void* message);

#endif /* _json_h_ */

typedef struct ua_routine {
	// gets the version of package installed
	ua_on_get_version on_get_version;

	// sets the new version of the package
	ua_on_set_version on_set_version;

	// (optional) to perform pre install actions
	ua_on_pre_install on_pre_install;

	// to perform install operation
	ua_on_install on_install;

	// (optional) to perform additional actions after install
	ua_on_post_install on_post_install;

	// (optional) to transfer file from another location
	ua_on_transfer_file on_transfer_file;

	// (optional) to prepare for install
	ua_on_prepare_install on_prepare_install;

	// (optional) to prepare for download
	ua_on_prepare_download on_prepare_download;

	// (optional) called on xl4bus messages
	ua_on_message on_message;

	// (optional) called when dmc is connnected to xl4bus.
	ua_on_dmc_presence on_dmc_presence;

} ua_routine_t;


typedef struct delta_tool {
	char* algo;
	char* path;
	char* args;
	int intl;

} delta_tool_t;

typedef struct delta_cfg {
	char* delta_cap;
	delta_tool_t* patch_tools;
	int patch_tool_cnt;
	delta_tool_t* decomp_tools;
	int decomp_tool_cnt;

} delta_cfg_t;

typedef enum update_rollback {
	URB_NONE,

	// UA signals INSTALL_FAILED after update failure. 
	// If next rollback version is available, it's included in the status message.
	// If no available rollback version, terminal-failure is set to true.
	// This is the default when rollback-versions is included in ready-update message.
	URB_DMC_INITIATED_WITH_UA_INTENT,

	// UA signals INSTALL_FAILED after update failure. 
	// No rollback version is included in the status message, regardless its availablity.
	URB_DMC_INITIATED_NO_UA_INTENT,

	// UA signals INSTALL_ROLLBACK after update failure.
	// If next rollback version is available, it's included in the status message.
	// If not available, signal INSTALL_FAILED with terminal-failure set to true.
	URB_UA_INITIATED,

} update_rollback_t;

typedef struct ua_cfg {
	// specifies the URL of the broker
	char* url;

	// specifies certificate directory
	char* cert_dir;

	// specifies the cache directory
	char* cache_dir;

	// specifies the backup directory
	char* backup_dir;

	// enables delta support
	int delta;

	// delta configuration
	delta_cfg_t* delta_config;

	// enables debug messages
	int debug;

	// specifies the buffer size for read/write, in kilobytes
	long rw_buffer_size;

	//indicate whether to use library reboot feature.
	//0 = default, ua implements its own reboot/resume support.
	//1 = ua uses libary's reboot/resume support.
	int reboot_support;

	// specifies which source package file used for backup,
	// when  delta reconstruction is triggered.
	// 0 = Use the actual full path used for installation, this is default.
	// 1 = Use the full path resulted from delta reconstruction.
	// Both may or may not be the same full path.
	int backup_source;

	//Indicate whether to disable sha verification of downloaded package.
	//0 = default, verify downloaded package against sha256
	//1 = disable, do not verify downloaded package against sha256
	int package_verification_disabled;

	//Enable fake rollback version in query-package response.
	//0 = default, do NOT use fake rollback version.
	//1 = enable, use fake rollback version.
	int enable_fake_rb_ver;

	//Password for x.509 certificate private key encryption. 
	//It's only used when private_key_password is not NULL.
	char* private_key_password;

#ifdef SUPPORT_UA_DOWNLOAD
	// specifies whether UA is to handle package download.
	int ua_download_required;
	// specifies directory used for UA download.
	char* ua_dl_dir;
	// specifies sigca bundle  directory
	char* sigca_dir;
#endif
} ua_cfg_t;


typedef struct ua_handler {
	char* type_handler;

	ua_routine_t* (*get_routine)(void);

} ua_handler_t;

#if XL4_SYMBOL_VISIBILITY_SUPPORTED
	#ifndef XL4_PUB
	#define XL4_PUB __attribute__((visibility ("default")))
	#endif
#else
	#define XL4_PUB
#endif

XL4_PUB
/**
 * Initializes updateagent.
 * @param updatagent configuration
 * @return ::E_UA_OK if initialization was successful, or an error code otherwise.
 */
int ua_init(ua_cfg_t* ua_config);

XL4_PUB int ua_register(ua_handler_t* uah, int len);

XL4_PUB int ua_unregister(ua_handler_t* uah, int len);

XL4_PUB int ua_stop(void);

XL4_PUB const char* ua_get_updateagent_version(void);

XL4_PUB const char* ua_get_xl4bus_version(void);

XL4_PUB int ua_send_install_progress(const char* pkgName, const char* version, int indeterminate, int percent);

XL4_PUB int ua_send_transfer_progress(const char* pkgName, const char* version, int indeterminate, int percent);

XL4_PUB int ua_backup_package(char* type, char* pkgName, char* version);

XL4_PUB int ua_send_message_string(char* message);

XL4_PUB int ua_send_message_string_with_address(char* message,  xl4bus_address_t* xl4_address);

#ifdef _json_h_

XL4_PUB int ua_send_message(json_object* message);

XL4_PUB int ua_send_message_with_address(json_object* jsonObj, xl4bus_address_t* xl4_address);


typedef enum log_type {
	LOG_EVENT,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
	LOG_SEVERE
} log_type_t;

typedef struct log_data {
	json_object* message;
	int compound;
	char* binary;
	char* timestamp;
} log_data_t;

XL4_PUB int ua_send_log_report(char* pkgType, log_type_t logtype, log_data_t* logdata);

#endif /* _json_h_ */

XL4_PUB
/**
 * Check if rollback has been disabled for pkgNmae.
 * @param pkgName package name of update component.
 * @return !0 = disabled, 0 = enabled
 */
int ua_rollback_disabled(const char* pkgName);

XL4_PUB
/**
 * Disable/enable rollback for pkgNmae.
 * @param pkgName package name of update component.
 * @param disable, 1 = disable rollback, 0 = enable rollback
 * @return none
 */
void ua_rollback_control(const char* pkgName, int disable);

XL4_PUB
/**
 * Set rollback type for pkgNmae.
 * When rollback-versions available, default is set to URB_DMC_INITIATED_WITH_UA_INTENT.
 * The caller shall set the desired rollback type in on_install callback.
 * Note if rb_type is set to URB_NONE, this function has no effect.
 * @param pkgName package name of update component.
 * @param rb_type rollback type.
 * @return E_UA_OK, or E_UA_ERR
 */
int ua_set_rollback_type(const char* pkgName, update_rollback_t rb_type);

XL4_PUB
/**
 * Set a custom message string to be included in query-package response,
 * or/and installation status (update-status). Each subsequent call of this
 * function will replace the message set previously.
 * The message set by the last call will be included util user explicitly calls
 * ua_clear_custom_message.
 * @param pkgName package name of update component.
 * @param message, message to be included.
 * @return E_UA_OK, or E_UA_ERR
 */
int ua_set_custom_message(const char* pkgName, char* message);

XL4_PUB
/**
 * Clear the message set previously by ua_set_custom_message, no more custom
 * message string will be included after this call.
 * @param pkgName package name of update component.
 * @return none
 */
void ua_clear_custom_message(const char* pkgName);

#ifdef HAVE_INSTALL_LOG_HANDLER
typedef enum ua_log_type {
	ua_debug_log,
	ua_info_log,
	ua_warning_log,
	ua_error_log,
	ua_fatal_log
}ua_log_type_t;

typedef void (*ua_log_handler_f)(ua_log_type_t type, const char* log);
XL4_PUB void ua_install_log_handler(ua_log_handler_f handler);
#endif /* HAVE_INSTALL_LOG_HANDLER */


#endif /* XL4UA_H_ */
