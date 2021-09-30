/*
 * util.c
 */
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <libgen.h>
#include "util.h"

scp_info_t ua_scp_info;

int xl4_run_cmd(char* argv[])
{
	int rc     = E_UA_OK;
	int status = 0;
	char* cmd  = 0;

	if (argv) {
		cmd = argv[0];
		pid_t pid=fork();

		if ( pid == -1) {
			A_INFO_MSG("fork failed: %s", strerror(errno));
			rc = E_UA_SYS;
		}else if ( pid == 0) {
			execvp(cmd, argv);
			A_INFO_MSG("execvp %s failed: %s", cmd, strerror(errno));
			rc = E_UA_SYS;
		}else {
			if (waitpid(pid, &status, 0) == -1) {
				A_INFO_MSG("waitpid failed: %s", strerror(errno));
				rc = E_UA_SYS;
			}else {
				rc = WEXITSTATUS(status);
				if (rc)
					A_INFO_MSG("command(%s) exited with status: %d", cmd, rc);
			}
		}
	}else{
		rc = E_UA_SYS;
	}
	return rc;
}

scp_info_t* scp_init(scp_info_t* scp)
{
	memset(&ua_scp_info, 0, sizeof(scp_info_t));
	if (scp) {
		ua_scp_info.url         = scp->url;
		ua_scp_info.user        = scp->user;
		ua_scp_info.password    = scp->password;
		ua_scp_info.local_dir   = scp->local_dir;
		ua_scp_info.scp_bin     = scp->scp_bin;
		ua_scp_info.sshpass_bin = scp->sshpass_bin;

		char* argv[] = {"mkdir", "-p", scp->local_dir, NULL};
		if (xl4_run_cmd(argv) != E_UA_OK)
			A_INFO_MSG("Failed to create scp dir %s", scp->local_dir);
		return &ua_scp_info;
	}
	return NULL;
}

char* scp_get_file(scp_info_t* scp, char* remote_path)
{
	char* argv[9]                = {0};
	char full_scp_path[PATH_MAX] = {0};
	int err                      = E_UA_OK;
	char* argv_op[]              = {"-p", "-o", "StrictHostKeyChecking no", NULL};

	if (scp && scp->url && scp->user && scp->local_dir && remote_path) {
		if (snprintf(full_scp_path, sizeof(full_scp_path) - 1, "%s@%s:%s", scp->user, scp->url, remote_path) <= 0) {
			A_INFO_MSG("Error creating full_scp_path");
			err = E_UA_ERR;
		}

		if (err == E_UA_OK) {
			char* basec = strdup(remote_path);
			if (snprintf(scp->dest_path, sizeof(scp->dest_path)- 1, "%s/%s", scp->local_dir, basename(basec)) <= 0) {
				A_INFO_MSG("Error creating dest_path");
				err = E_UA_ERR;
			}
			if (basec)
				free(basec);
		}

		if (err == E_UA_OK) {
			if (scp->password) {
				argv[0] = scp->sshpass_bin;
				argv[1] = argv_op[0];
				argv[2] = scp->password;
				argv[3] = scp->scp_bin;
				argv[4] = argv_op[1];
				argv[5] = argv_op[2];
				argv[6] = full_scp_path;
				argv[7] = scp->dest_path;
			} else {
				argv[0] = scp->scp_bin;
				argv[1] = argv_op[1];
				argv[2] = argv_op[2];
				argv[3] = full_scp_path;
				argv[4] = scp->dest_path;
			}

			if (xl4_run_cmd(argv) == E_UA_OK)
				return scp->dest_path;
		}

	} else {
		A_INFO_MSG("scp info is not set.");
	}

	return NULL;
}

scp_info_t* scp_get_info(void)
{
	return &ua_scp_info;
}

#ifdef LIBUA_VER_2_0

int do_transfer_file(ua_callback_ctl_t* ctl)
{
	int rc          = E_UA_OK;
	scp_info_t* scp = scp_get_info();

	if (scp && scp->url) {
		if (scp_get_file(scp, (char*)ctl->pkg_path) == NULL)
			rc = E_UA_ERR;
	} else {
		A_INFO_MSG("SCP url is not set, not transferring file remotely.");
	}

	return rc;
}

#else

int do_transfer_file(const char* type, const char* pkgName, const char* version, const char* pkgFile, char** newFile)
{
	int rc          = E_UA_OK;
	scp_info_t* scp = scp_get_info();

	if (scp && scp->url) {
		if ((*newFile = scp_get_file(scp, (char*)pkgFile)) == NULL)
			rc = E_UA_ERR;
	} else {
		A_INFO_MSG("SCP url is not set, not transferring file remotely.");
	}

	return rc;
}

#endif