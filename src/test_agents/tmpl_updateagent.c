/*
 * tmpl_updateagent.c
 */

#include "tmpl_updateagent.h"
#include <fcntl.h> /* Definition of AT_* constants */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_SIZE 256
const char backupDir[]         = "/data/sota/esync/backup/";
char rbConfFile[256]           = "/data/sota/rbConf";
update_mode_t test_update_mode = 0;
int test_reboot                = 0;
char* instVerFile;
char instVer[60] = "\0";
char* bkpDir;
FILE* fp1;

int get_tmpl_version(const char* type, const char* pkgName, char** version)
{
	instVerFile = (char*) malloc(MAX_SIZE * sizeof(char));
	getFileName(pkgName);
	if (getVerFromFile(pkgName) != 0)
	{
		printf("Version file not available\n");
		*version = NULL;
		free(instVerFile);
		return E_UA_OK;
	}
	*version = instVer;
	free(instVerFile);
	return E_UA_OK;
}

static int set_tmpl_version(const char* type, const char* pkgName, const char* version)
{
	instVerFile = (char*) malloc(MAX_SIZE * sizeof(char));
	getFileName(pkgName);
	if (setVerToFile(pkgName, version) != 0)
	{
		printf("Set version to file Failed\n");
		free(instVerFile);
		return E_UA_ERR;
	}
	free(instVerFile);
	return E_UA_OK;
}

static install_state_t do_tmpl_pre_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	return INSTALL_IN_PROGRESS;

}

static install_state_t do_tmpl_install(const char* type, const char* pkgName, const char* version, const char* pkgFile)
{
	install_state_t rc     = INSTALL_COMPLETED;
	static int cnt         = 0;
	char usr_rbVersion[25] = "\0";
	char usr_pkgName[25]   = "\0";

	printf("Emulate installation of %s for %s(%s) using %s\n", type, pkgName, version, pkgFile);
	get_usr_rbVersion(usr_rbVersion, usr_pkgName);

	if (test_reboot) {
		printf("Emulate system reboot for update installation.\n");
		abort();
	} else {
		switch (test_update_mode) {
			case UPDATE_MODE_FAILURE:
				printf("In UPDATE_MODE_FAILURE\n");
				rc = INSTALL_FAILED;
				break;

			case UPDATE_MODE_ALTERNATE_FAILURE_SUCCESSFUL:
				if (cnt++ % 2 == 0) {
					rc = INSTALL_FAILED;
				}
				break;

			case UPDATE_MODE_ROLLBACK:
				printf("In UPDATE_MODE_ROLLBACK\n");
				if (!strcmp(pkgName, usr_pkgName)) {
					//printf("package name match\n");
					if (strcmp(version, usr_rbVersion)) {
						//printf("target version not same as rbVersion\n");
						rc = INSTALL_FAILED;
					}
				}
				else {
					printf("unknown package name.. Rollback Not initiated\n");
				}
				break;
			case UPDATE_MODE_SUCCESSFUL:
				printf("In UPDATE_MODE_SUCCESSFUL\n");
				rc = INSTALL_COMPLETED;
				break;
			default:
				break;
		}
	}

	if (rc == INSTALL_COMPLETED)
		printf("Emulate installation succeeded\n");
	else
		printf("Emulate installation failed(%d)\n", rc);

	return rc;

}

static void do_tmpl_post_install(const char* type, const char* pkgName)
{
	return;

}

static install_state_t do_prepare_install(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	return INSTALL_READY;

}

static download_state_t do_prepare_download(const char* type, const char* pkgName, const char* version)
{
	return DOWNLOAD_CONSENT;

}

static int tmpl_on_dmc_presence(dmc_presence_t* dp)
{
	return 0;
}
ua_routine_t tmpl_rtns = {
	.on_get_version      = get_tmpl_version,
	.on_set_version      = set_tmpl_version,
	.on_pre_install      = do_tmpl_pre_install,
	.on_install          = do_tmpl_install,
	.on_post_install     = do_tmpl_post_install,
	.on_prepare_install  = do_prepare_install,
	.on_prepare_download = do_prepare_download,
	.on_dmc_presence     = tmpl_on_dmc_presence
};

ua_routine_t* get_tmpl_routine(void)
{
	return &tmpl_rtns;

}

void set_test_installation_mode(update_mode_t mode, int reboot)
{
	test_update_mode = mode;
	test_reboot      = reboot;
}



void get_usr_rbVersion(char* usr_rbVersion, char* usr_pkgName)
{
	FILE* fp;
	char* rbRead[2];
	const size_t buff_size = 50;
	char* buff             = malloc(buff_size);
	int i                  =0;


	if (access(rbConfFile, F_OK) != -1 ) {
		fp = fopen(rbConfFile, "r");
		if (fp == NULL) {
			printf("Rollback config file open failed\n");
			strcpy(usr_rbVersion, "\0");
			strcpy(usr_pkgName, "\0");
			free(buff);
			return;
		}

		//printf("File open success\n ");
		for (i=0; i < 2; i++) {
			rbRead[i] = malloc(buff_size);
		}
		i = 0;
		while (fgets(buff, buff_size, fp) != NULL) {
			sscanf(buff,"%s",rbRead[i]);
			strcpy(buff, "");
			i++;
		}
		strcpy(usr_pkgName, rbRead[0]);
		strcpy(usr_rbVersion, rbRead[1]);

	}
	else {
		printf("Rollback config file not accessable\n");
		strcpy(usr_rbVersion, "\0");
		strcpy(usr_pkgName, "\0");
		free(buff);
		return;
	}
	fclose(fp);
	for (i=0; i < 2; i++) {
		free(rbRead[i]);
	}
	free(buff);
	return;
}

void getFileName(const char* pkgName)
{
	char* verFile = (char*)malloc(MAX_SIZE * sizeof(char));

	sprintf(verFile, "/%s.txt", pkgName);
	strcpy(instVerFile, backupDir);
	strcat(instVerFile, pkgName);
	strcat(instVerFile, verFile);
	//printf("getFileName: resulting file: %s\n", instVerFile);
	free(verFile);
}

void getBackupDir(const char* pkgName)
{
	int ret_dir;

	strcpy(bkpDir, backupDir);
	strcat(bkpDir, pkgName);
	//printf("getBackupDir: resulting BackupDir: %s\n", bkpDir);
	ret_dir = mkdir(bkpDir, 0755);
	if (ret_dir != 0) {
		printf("Back up directory creation failed or already exists\n");
	}
	else {
		printf("Back up directory created\n");
	}
}

int getVerFromFile(const char* pkgName)
{
	if (access(instVerFile, F_OK) != -1 ) {
		if ((fp1 = fopen(instVerFile, "r")) != NULL) {
			//printf("File open success\n ");
			if (fgets(instVer, sizeof(instVer), fp1) != NULL) {
				//printf("getVerFromFile: Installed version: %s \n", instVer);
				fclose(fp1);
				return E_UA_OK;
			}
			else {
				printf("file read failed, may be empty file\n");
				fclose(fp1);
				return E_UA_ERR;
			}
		}
		else {
			printf("File open failed\n");
			return E_UA_ERR;
		}
	}
	else {
		printf("File not accessable\n");
		return E_UA_ERR;
	}
	fclose(fp1);
	return E_UA_OK;
}

int setVerToFile(const char* pkgName, const char* version)
{
	bkpDir = (char*)malloc(MAX_SIZE * sizeof(char));
	getBackupDir(pkgName);
	if (access(bkpDir, F_OK) != -1 ) {
		if ((fp1 = fopen(instVerFile, "w")) == NULL)
		{
			printf("Version file open failed\n");
			perror("fopen");
			return E_UA_ERR;
		}
		//printf("Version file open success\n ");
		fputs(version, fp1);
		fclose(fp1);
		free(bkpDir);
		return E_UA_OK;
	}
	else {
		printf("BackupDir not accessable\n");
		return E_UA_ERR;
	}
}