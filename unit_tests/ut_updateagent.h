/*
 * ut_updateagent.h
 */

#ifndef TMPL_UPDATEAGENT_H_
#define TMPL_UPDATEAGENT_H_

#include "xl4ua.h"

typedef enum update_mode {
	UPDATE_MODE_SUCCESSFUL,
	UPDATE_MODE_FAILURE,
	UPDATE_MODE_ALTERNATE_FAILURE_SUCCESSFUL,
	UPDATE_MODE_ROLLBACK,
}update_mode_t;

int get_tmpl_version(const char* type, const char* pkgName, char** version);
ua_routine_t* get_tmpl_routine(void);
void set_test_installation_mode(update_mode_t mode, int reboot);
void get_usr_rbVersion(char* usr_rbVersion, char* usr_pkgName);
void getFileName(const char* pkgName);
void getBackupDir(const char* pkgName);
int getVerFromFile(const char* pkgName);
int setVerToFile(const char* pkgName, const char* version);
void set_rbConf_path(char * rbPath);
void set_backup_dir(const char * bkpDir);

#endif /* TMPL_UPDATEAGENT_H_ */
