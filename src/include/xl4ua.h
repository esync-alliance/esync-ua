/*
 * xl4ua.h
 */

#ifndef _XL4UA_H_
#define _XL4UA_H_

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


#define E_UA_OK         ( 0)
#define E_UA_ERR        (-1)
#define E_UA_MEMORY     (-2)
#define E_UA_ARG        (-3)
#define E_UA_SYS        (-4)


typedef int (*ua_on_get_version)(const char * pkgName, char ** version);

typedef int (*ua_on_set_version)(const char * pkgName, const char * version);

typedef install_state_t (*ua_on_pre_install)(const char * pkgName, const char * version, const char * pkgFile);

typedef install_state_t (*ua_on_install)(const char * pkgName, const char * version, const char * pkgFile);

typedef void (*ua_on_post_install)(const char * pkgName);

typedef install_state_t (*ua_on_prepare_install)(const char * pkgName, const char * version, const char * pkgFile, char ** newFile);

typedef download_state_t (*ua_on_prepare_download)(const char * pkgName, const char * version);

#ifdef _json_h_

typedef int (*ua_on_message)(const char * msgType, json_object * message);

#endif

typedef struct ua_routine {

    // gets the version of package installed
    ua_on_get_version       on_get_version;

    // sets the new version of the package
    ua_on_set_version       on_set_version;

    // (optional) to perform pre install actions
    ua_on_pre_install       on_pre_install;

    // to perform install operation
    ua_on_install           on_install;

    // (optional) to perform additional actions after install
    ua_on_post_install      on_post_install;

    // (optional) to prepare for install
    ua_on_prepare_install   on_prepare_install;

    // (optional) to prepare for download
    ua_on_prepare_download  on_prepare_download;

#ifdef _json_h_

    // (optional) called on xl4bus messages
    ua_on_message           on_message;

#endif

} ua_routine_t;


typedef struct delta_tool {

    char *                  algo;
    char *                  path;
    char *                  args;
    int                     intl;

} delta_tool_t;

typedef struct delta_cfg {

    char *                  delta_cap;
    delta_tool_t *          patch_tools;
    int                     patch_tool_cnt;
    delta_tool_t *          decomp_tools;
    int                     decomp_tool_cnt;

} delta_cfg_t;


typedef struct ua_cfg {

    // specifies the URL of the broker
    char *                  url;

    // specifies certificate directory
    char *                  cert_dir;

    // specifies the cache directory
    char *                  cache_dir;

    // specifies the backup directory
    char *                  backup_dir;

    // enables delta support
    int                     delta;

    // delta configuration
    delta_cfg_t *           delta_config;

    // enables debug messages
    int                     debug;

} ua_cfg_t;


typedef struct ua_handler {

    char * type_handler;

    ua_routine_t* (*get_routine)(void);

} ua_handler_t;


#ifndef XL4_PUB
/**
 * Used to indicate that the library symbol is properly exported.
 */
#define XL4_PUB __attribute__((visibility ("default")))
#endif

XL4_PUB
/**
 * Initializes updateagent.
 * @param updatagent configuration
 * @return ::E_UA_OK if initialization was successful, or an error code otherwise.
 */
int ua_init(ua_cfg_t * ua_config);

XL4_PUB int ua_register(ua_handler_t * uah, int len);

XL4_PUB int ua_unregister(ua_handler_t * uah, int len);

XL4_PUB int ua_stop();

XL4_PUB const char * ua_get_updateagent_version(void);

XL4_PUB const char * ua_get_xl4bus_version(void);

XL4_PUB int ua_install_progress(const char * pkgName, const char * version, int indeterminate, int percent);

XL4_PUB int ua_transfer_progress(const char * pkgName, const char * version, int indeterminate, int percent);


#ifdef _json_h_

XL4_PUB int ua_send_message(json_object * message);


typedef struct log_data {
    json_object * message;
    int    compound;
    char * binary;
    char * timestamp;
} log_data_t;

typedef enum log_type {
    LOG_EVENT,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_SEVERE
} log_type_t;

XL4_PUB int ua_report_log(char * pkgType, log_data_t * logdata, log_type_t logtype);


#endif

#endif /* _XL4UA_H_ */
