/*
 * updateagent.h
 */

#ifndef _UPDATEAGENT_H_
#define _UPDATEAGENT_H_

typedef enum install_state {
    INSTALL_PENDING,
    INSTALL_INPROGRESS,
    INSTALL_COMPLETED,
    INSTALL_FAILED,
    INSTALL_POSTPONED,
    INSTALL_ABORTED,
    INSTALL_ROLLBACK,
} install_state_t;


#define E_UA_OK         ( 0)
#define E_UA_ERR        ( -1)


typedef int (*ua_do_get_version)(char * pkgName, char ** version);

typedef int (*ua_do_set_version)(char * pkgName, char * version);

typedef install_state_t (*ua_do_pre_install)(char * pkgName, char * version);

typedef install_state_t (*ua_do_install)(char * pkgName, char * version, char * pkgFile);

typedef void (*ua_do_post_install)();


typedef struct update_agent {

    ua_do_get_version       do_get_version;

    ua_do_set_version       do_set_version;

    ua_do_pre_install       do_pre_install;

    ua_do_install           do_install;

    ua_do_post_install      do_post_install;

    char *                  ua_type;

    char *                  url;

} update_agent_t;


#endif /* _UPDATEAGENT_H_ */
