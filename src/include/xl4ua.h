/*
 * xl4ua.h
 */

#ifndef _XL4UA_H_
#define _XL4UA_H_

typedef enum install_state {
    INSTALL_PENDING,
    INSTALL_INPROGRESS,
    INSTALL_COMPLETED,
    INSTALL_FAILED,
    INSTALL_POSTPONED,
    INSTALL_ABORTED,
    INSTALL_ROLLBACK
} install_state_t;

typedef enum log_type {
    LOG_EVENT,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_SEVERE
} log_type_t;

typedef struct log_data {
    void * message;
    int    compound;
    char * binary;
    char * timestamp;
} log_data_t;


#define E_UA_OK         ( 0)
#define E_UA_ERR        (-1)
#define E_UA_MEMORY     (-2)
#define E_UA_ARG        (-3)
#define E_UA_SYS        (-4)


typedef int (*ua_on_get_version)(char * pkgName, char ** version);

typedef int (*ua_on_set_version)(char * pkgName, char * version);

typedef install_state_t (*ua_on_pre_install)(char * pkgName, char * version);

typedef install_state_t (*ua_on_install)(char * pkgName, char * version, char * pkgFile);

typedef void (*ua_on_post_install)(void);


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

} ua_routine_t;

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

XL4_PUB int ua_init(ua_cfg_t * in_cfg);

XL4_PUB int ua_register(ua_handler_t * uah, int len);

XL4_PUB int ua_unregister(ua_handler_t * uah, int len);

XL4_PUB int ua_stop();

XL4_PUB int ua_report_log(char * pkgType, log_data_t * logdata, log_type_t logtype);


#endif /* _XL4UA_H_ */
