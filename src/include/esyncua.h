/**
 * @file esyncua.h
 * @brief API header file for eSync Update Agent Library (libua)
 *
 * Copyright(c) 2020 Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 */

#ifndef ESYNCUA_H
#define ESYNCUA_H

#include <libxl4bus/types.h>

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

typedef struct ua_callback_ctl {
	/**
	 * Component handler type, set by the library.
	 * Its value shall not be modified in the callback function.
	 */
	const char* type;

	/**
	 * Component package name, set by the library.
	 * Its value shall not be modified in the callback function.
	 */
	const char* pkg_name;

	/**
	 * Component version, set by the library except in callback on_get_version.
	 * Its value can only be modified in the on_get_version function,
	 * in all other callback functions, its value shall not be modified.
	 */
	char* version;

	/**
	 * full path of installation package archive,
	 * set by the the caller in the libarary.
	 */
	char* pkg_path;

	/**
	 * full path of new installation package archive,
	 * shall be set in the callback functin if needed,
	 * and the reference pointer should be still vald after callback returns.
	 * library does not modify the value referenced by the pointer.
	 *
	 */
	char* new_file_path;

	/**
	 * indicate whether the current installation is for rollback,
	 * this is ony valid for callback on_install.
	 * 0 = normal installation, 1 = rollback installation.
	 *
	 */
	int is_rollback;

	/**
	 * self reference pointer set by UA when calling ua_regitster,
	 * library does not alter the contents referenced by the pointer.
	 *
	 */
	void* ref;

}ua_callback_ctl_t;

/**
 * Callback invoked when handling xl4.query-package to get ECU version
 * @param clt UA control data struct
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
typedef int (*ua_on_get_version)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked after successful update when handling xl4.ready-update to
 * allow ECU to set installed version manually if neeeded.
 * @param clt UA control data struct
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
typedef int (*ua_on_set_version)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked when handling xl4.ready-download
 * @param clt UA control data struct
 * @return DOWNLOAD_CONSENT to signal eSync Client to start downloading,
 *                 DOWNLOAD_DENIED to singal eSync Client not to download.
 */
typedef download_state_t (*ua_on_prepare_download)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked when handling xl4.prepare-update,
 * to allow UA transfer file from a remote system if needed.
 * @param clt UA control data struct
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
typedef int (*ua_on_transfer_file)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked when handling xl4.prepare-update,
 * after ua_on_transfer_file to allow UA transfer file from a remote system,
 * after delta reconstruction if delta pakcage is used for update.
 * @param clt UA control data struct
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
typedef install_state_t (*ua_on_prepare_install)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked when handling xl4.ready-update,
 * This is first callback to allow ECU to setup for update installaion.
 * @param clt UA control data struct
 * @return INSTALL_IN_PROGRESS on success, INSTALL_FAILED on failure
 */
typedef install_state_t (*ua_on_pre_install)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked when handling xl4.ready-update,
 * if ua_on_pre_install returns INSTALL_IN_PROGRESS,
 * ECU should perform the actural update installation.
 * @param clt UA control data struct
 * @return INSTALL_COMPLETED on success, INSTALL_FAILED on failure
 */
typedef install_state_t (*ua_on_install)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked when handling xl4.ready-update,
 * if ua_on_install returns INSTALL_IN_PROGRESS,
 * UA can perform additional processing after successfull update.
 * @param clt UA control data struct
 */
typedef void (*ua_on_post_install)(ua_callback_ctl_t* ctl);

/**
 * Callback invoked when eSync Client is connected/disconnected to/from eSync bus
 * @param dp, pointer to dmc_presence_t structure, reserved for future use.
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
typedef int (*ua_on_dmc_presence)(dmc_presence_t* dp);

/**
 * Callback invoked when eSync bus status has changed.
 * @param status, status value defined by eSync Bus.
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
typedef void (*ua_on_esyncbus_status)(int status);

/**
 * Callback invoked for every eSync bus message received by libua
 * Returning !0 in this function signals this message has been processed,
 * libua will not invoke the default handler function for this messsage type.
 * If this function returns 0, the default handler function will still be called.
 * @param type, component handler type
 * @param message, charater string containing raw eSync bus message
 * @return !0 = message has been processed, 0 = not processed
 */
typedef int (*ua_on_message)(const char* type, const char* message);


typedef struct ua_routine {
	// gets the version of package installed
	ua_on_get_version on_get_version;

	// (optional)  sets the new version of the package
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

	// (optional) called on esyncbus messages
	ua_on_message on_message;

	// (optional) called when dmc is connnected to esyncbus.
	ua_on_dmc_presence on_dmc_presence;

	// (optional) called when esyncbus status has changed.
	ua_on_esyncbus_status on_esyncbus_status;

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

	// specifies the diagnostic file directory
	#ifdef SUPPORT_LOGGING_INFO
	char *diag_dir;
	#endif

	// enables delta support
	// 0 = default, delta update is disable.
	// 1 = delta update is enabled.
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
	//component handler type
	char* type_handler;

	//get a pointer to a set of callback functions invoked by libua
	ua_routine_t* (*get_routine)(void);

	//Pointer to user referenced data structure
	//This is passed to all callback functions.
	void* ref;

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
 * Initializes updateagent instance.
 * @param ua_config updatagent configuration
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_init(ua_cfg_t* ua_config);

XL4_PUB
/**
 * Registers handler(s) and the corresponding context for UA instance.
 * @param uah points to an array structure of UA handler data type(s).
 * @param len num of handler type(s) in the array.
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_register(ua_handler_t* uah, int len);

XL4_PUB
/**
 * De-registers handler(s) and the corresponding context for UA instance.
 * @param uah points to an array structure of UA handler data type(s).
 * @param len num of handler type(s) in the array.
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_unregister(ua_handler_t* uah, int len);

XL4_PUB
/**
 * Stop update agent, all related resource will be released.
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_stop(void);

XL4_PUB
/**
 * Provide libua version string to the UA.
 * @return version string
 */
const char* ua_get_updateagent_version(void);

XL4_PUB
/**
 * Provide libxl4bus version string to the UA.
 * @return version string
 */
const char* ua_get_xl4bus_version(void);

XL4_PUB
/**
 * Make a backup image of the installation package after successful update.
 * libua performs the same backup automatically after successful installation.
 * For ECU(s) reboots itself to start update installation (e.g. by boot loader),
 * UA should call this fuction to make a backup image manually.
 * @param type component handler type
 * @param pkgName package name
 * @param version installed version string
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_backup_package(char* type, char* pkgName, char* version);

XL4_PUB
/**
 * Send installation progress (US_INSTALL) via xl4.update-report status to eSync Client
 * @param pkgName package name of update component
 * @param version version string
 * @param indeterminate 1 = indeterminate, 0 = not indeterminate
 * @param percent progress percentage (0-100)
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_send_install_progress(const char* pkgName, const char* version, int indeterminate, int percent);

XL4_PUB
/**
 * Send transfer progress (US_TRANSFER) via xl4.update-report status to eSync Client
 * @param pkgName package name of update component
 * @param version version string
 * @param indeterminate 1 = indeterminate, 0 = not indeterminate
 * @param percent progress percentage (0-100)
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_send_transfer_progress(const char* pkgName, const char* version, int indeterminate, int percent);

XL4_PUB
/**
 * Send charater string as-is in "message" to eSync Client.
 * Sender is fully responsible for the "correctness" of the message format.
 * @param message character string
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_send_message_string(char* message);


XL4_PUB /**
         * Send charater string as-is in "message" to a eSync bus addressed node.
         * Sender is fully responsible for the "correctness" of the message format.
         * @param message character string
         * @param address eSync bus address
         * @return E_UA_OK on success, E_UA_ERR on failure
         */
int ua_send_message_string_with_address(char* message,  xl4bus_address_t* address);


XL4_PUB
/**
 * Send update-status with CURRENT_REPORT.
 * @param pkgName package name of update component
 * @param version current installed version
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_send_current_report(const char * pkgName, const char * version);


#ifdef _json_h_

XL4_PUB
/**
 * Send josn string formated by json_object "message" to eSync Client.
 * Sender is fully responsible for the "correctness" of the message format.
 * libua converts json_object "message" to json charater string.
 * @param message character string
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_send_message(json_object* message);

XL4_PUB
/**
 * Send josn string formated by json_object "message" to a eSync bus addressed node.
 * Sender is fully responsible for the "correctness" of the message format.
 * libua converts json_object "message" to json charater string.
 * @param message character string
 * @param address eSync bus address
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
int ua_send_message_with_address(json_object* jsonObj, xl4bus_address_t* xl4_address);

/**
 * log level used in xl4.log-report messages
 */
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

XL4_PUB int
/**
 * Send xl4.log-report to eSync Client
 * @param pkgType component handler type
 * @param logtype enum type log_type_t
 * @param logdata log report data
 * @return E_UA_OK on success, E_UA_ERR on failure
 */
ua_send_log_report(char* pkgType, log_type_t logtype, log_data_t* logdata);

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
 * unzip utility.
 * @param zipfile   pathname of zip archive.
 * @param destpath  pathname of destination folder.
 * @return          E_UA_OK on success, E_UA_ERR on failure
 */
int ua_unzip(const char* archive, const char* destpath);

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

XL4_PUB
/**
 * Callback invoked whenever eSync bus connection status change.
 * @return to give eSync bus connection status.
 */
int ua_get_bus_status(void);

XL4_PUB
/**
 * Return the path name of component package archive.
 * @param pkgName package name of update component.
 * @param pkgVersion package version of update component.
 * @return full path name of component package archive, return NULL if none is found.
 */
char* ua_get_package_path(const char* pkgName, const char* pkgVersion);

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

#endif /* ESYNCUA_H */
