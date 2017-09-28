/*
 * utils.h
 */

#ifndef _UA_UTILS_H_
#define _UA_UTILS_H_

#include "updateagent.h"
#include "config.h"

int get_type_from_json(json_object * jsonObj, char ** value);

int get_replyid_from_json(json_object * jsonObj, char ** value);

int get_pkg_version_from_json(json_object * jsonObj, char ** value);

int get_pkg_type_from_json(json_object * jsonObj, char ** value);

int get_pkg_name_from_json(json_object * jsonObj, char ** value);

int get_pkg_status_from_json(json_object * jsonObj, char ** value);

int get_file_from_json(json_object * jsonObj, char * version, char ** value);

int get_bytes_downloaded_from_json(json_object * jsonObj, int64_t * value);

int get_bytes_total_from_json(json_object * jsonObj, int64_t * value);



#endif /* _UA_UTILS_H_ */
