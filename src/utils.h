/*
 * utils.h
 */

#ifndef _UA_UTILS_H_
#define _UA_UTILS_H_

#include "common.h"


int get_type_from_json(json_object * jsonObj, char ** value);

int get_replyid_from_json(json_object * jsonObj, char ** value);

int get_replyto_from_json(json_object * jsonObj, char ** value);

int get_pkg_version_from_json(json_object * jsonObj, char ** value);

int get_pkg_type_from_json(json_object * jsonObj, char ** value);

int get_pkg_name_from_json(json_object * jsonObj, char ** value);

int get_pkg_status_from_json(json_object * jsonObj, char ** value);

int get_pkg_file_from_json(json_object * jsonObj, char * version, char ** value);

int get_pkg_downloaded_from_json(json_object * jsonObj, char * version, int * value);

int get_pkg_rollback_version_from_json(json_object * jsonObj, char ** value);

int get_pkg_rollback_versions_from_json(json_object * jsonObj, json_object ** value);

int get_downloaded_bytes_from_json(json_object * jsonObj, int64_t * value);

int get_total_bytes_from_json(json_object * jsonObj, int64_t * value);

int get_update_status_response_from_json(json_object * jsonObj, int * value);

int get_body_rollback_from_json(json_object * jsonObj, int * value);

int get_pkg_next_rollback_version(json_object * jsonArr, char * currentVer, char ** nextVer);

int get_pkg_sha256_from_json(json_object * jsonObj, char * version, char value[SHA256_B64_LENGTH]);

#endif /* _UA_UTILS_H_ */
