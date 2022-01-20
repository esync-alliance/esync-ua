/*
 * utils.h
 */

#ifndef UA_UTILS_H_
#define UA_UTILS_H_
#include "misc.h"
#include <json.h>

#ifdef SUPPORT_UA_DOWNLOAD
#include "eua_json.h"
#endif

int get_type_from_json(json_object* jsonObj, char** value);

int get_seq_num_from_json(json_object* jsonObj, int* value);

int get_seq_info_per_update_from_json(json_object* jsonObj, json_object** value);

int get_replyid_from_json(json_object* jsonObj, char** value);

int get_replyto_from_json(json_object* jsonObj, char** value);

int get_pkg_version_from_json(json_object* jsonObj, char** value);

int get_pkg_type_from_json(json_object* jsonObj, char** value);

int get_pkg_name_from_json(json_object* jsonObj, char** value);

int get_pkg_status_from_json(json_object* jsonObj, char** value);

int get_pkg_id_from_json(json_object *jsonObj, char **value);

int get_pkg_stage_from_json(json_object *jsonObj, char **value);

int get_pkg_file_from_json(json_object* jsonObj, char* version, char** value);

int get_pkg_downloaded_from_json(json_object* jsonObj, char* version, int* value);

int get_pkg_rollback_version_from_json(json_object* jsonObj, char** value);

int get_pkg_rollback_versions_from_json(json_object* jsonObj, json_object** value);

int get_downloaded_bytes_from_json(json_object* jsonObj, int64_t* value);

int get_total_bytes_from_json(json_object* jsonObj, int64_t* value);

int get_update_status_response_from_json(json_object* jsonObj, int* value);

int get_body_rollback_from_json(json_object* jsonObj, int* value);

int get_pkg_next_rollback_version(json_object* jsonArr, char* currentVer, char** nextVer);

int get_pkg_sha256_from_json(json_object * jsonObj, char* version, char value[SHA256_B64_LENGTH]);

int get_pkg_delta_sha256_from_json(json_object* jsonObj, char* version, char value[SHA256_B64_LENGTH]);

#ifdef SUPPORT_UA_DOWNLOAD
int get_pkg_version_item_from_json(json_object* jsonObj, char* version, version_item_t* vi);

int get_trust_info_from_json(json_object* jsonObj, ua_dl_trust_t* dlt);
#endif

int json_get_property(json_object* json, enum json_type typ, void* value, const char* node, ... );

#endif /* UA_UTILS_H_ */
