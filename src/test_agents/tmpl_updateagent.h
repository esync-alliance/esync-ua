/*
 * ua.h
 */

#ifndef _TMPL_UPDATEAGENT_H_
#define _TMPL_UPDATEAGENT_H_

#include <xl4ua.h>
#include <string.h>
#include "uthash.h"

typedef struct pkg_version {
	char* version;
	char* pkg;
	UT_hash_handle hh;
} pkg_version_t;

#define TMPL_VER_GET(p,v) do { \
		pkg_version_t* pv = 0; \
		HASH_FIND_STR(package_version, p, pv); \
		v = pv ? pv->version : NULL; \
} while (0)

#define TMPL_VER_SET(p,v) do { \
		pkg_version_t* pv = 0; \
		HASH_FIND_STR(package_version, p, pv); \
		if (!pv) { \
			pv          = malloc(sizeof(pkg_version_t)); \
			pv->pkg     = strdup(p); \
			pv->version = 0; \
			HASH_ADD_STR(package_version, pkg, pv); \
		} \
		free(pv->version); \
		pv->version = strdup(v); \
} while (0)


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

#endif /* _TMPL_UPDATEAGENT_H_ */
