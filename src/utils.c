/*
 * utils.c
 */

#include "utils.h"
#include <string.h>
#include <stdarg.h>
#include "xl4ua.h"
#include "debug.h"

int get_type_from_json(json_object* jsonObj, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "type", NULL);

}

int get_seq_num_from_json(json_object* jsonObj, int* value)
{
	return json_get_property(jsonObj, json_type_int, value, "body", "sequence", NULL);

}

int get_seq_info_per_update_from_json(json_object* jsonObj, json_object** value)
{
	return json_get_property(jsonObj, json_type_object, value, "body", "change-notification-sequences", "per-update", NULL);

}

int get_replyid_from_json(json_object* jsonObj, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "reply-id", NULL);

}


int get_replyto_from_json(json_object* jsonObj, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "reply-to", NULL);

}


int get_pkg_version_from_json(json_object* jsonObj, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "body", "package", "version", NULL);

}


int get_pkg_type_from_json(json_object* jsonObj, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "body", "package", "type", NULL);

}


int get_pkg_name_from_json(json_object* jsonObj, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "body", "package", "name", NULL);

}


int get_pkg_file_from_json(json_object* jsonObj, char* version, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "body", "package", "version-list", version, "file", NULL);

}


int get_pkg_downloaded_from_json(json_object* jsonObj, char* version, int* value)
{
	return json_get_property(jsonObj, json_type_boolean, value, "body", "package", "version-list", version, "downloaded", NULL);

}


int get_pkg_rollback_version_from_json(json_object* jsonObj, char** value)
{
	return json_get_property(jsonObj, json_type_string, value, "body", "package", "rollback-version", NULL);

}

int get_pkg_rollback_versions_from_json(json_object* jsonObj, json_object** value)
{
	return json_get_property(jsonObj, json_type_array, value, "body", "package", "rollback-versions", NULL);

}


int get_downloaded_bytes_from_json(json_object* jsonObj, int64_t* value)
{
	return json_get_property(jsonObj, json_type_int, value, "body", "downloaded-bytes", NULL);

}


int get_total_bytes_from_json(json_object* jsonObj, int64_t* value)
{
	return json_get_property(jsonObj, json_type_int, value, "body", "total-bytes", NULL);

}

int get_update_status_response_from_json(json_object* jsonObj, int* value)
{
	return json_get_property(jsonObj, json_type_boolean, value, "body", "successful", NULL);

}

int get_pkg_sha256_from_json(json_object* jsonObj, char* version, char value[SHA256_B64_LENGTH])
{
	int err      = E_UA_OK;
	char* sha256 = 0;

	if (!json_get_property(jsonObj, json_type_string, &sha256, "body", "package", "version-list", version, "sha-256", NULL) &&
	    (strlen(sha256) == (SHA256_B64_LENGTH - 1))) {
		strcpy(value, sha256);
	} else {
		err = E_UA_ERR;
	}
	return err;

}

int get_pkg_delta_sha256_from_json(json_object* jsonObj, char* version, char value[SHA256_B64_LENGTH])
{
	int err      = E_UA_OK;
	char* sha256 = 0;

	if (!json_get_property(jsonObj, json_type_string, &sha256, "body", "package", "version-list", version, "delta-sha-256", NULL) &&
	    (strlen(sha256) == (SHA256_B64_LENGTH - 1))) {
		strcpy(value, sha256);
	} else {
		err = E_UA_ERR;
	}
	return err;

}

#ifdef SUPPORT_UA_DOWNLOAD
int get_pkg_version_item_from_json(json_object* jsonObj, char* version, version_item_t* vi)
{
	if (jsonObj && version && vi) {
		if (json_get_property(jsonObj, json_type_string, &vi->sha256, "body", "package", "version-list", version, "sha-256", NULL))
			return E_UA_ERR;

		if (json_get_property(jsonObj, json_type_boolean, &vi->downloaded, "body", "package", "version-list", version, "downloaded", NULL))
			return E_UA_ERR;

		json_object* jdl = NULL, * jdl_arr = NULL;
		json_get_property(jsonObj, json_type_array, &jdl_arr, "body", "package", "version-list", version, "downloadables", NULL);
		int jdl_arr_num = json_object_array_length(jdl_arr);
		if (!jdl_arr && !json_object_is_type(jdl_arr, json_type_array) && jdl_arr_num < 1)
			return E_UA_ERR;

		for (int i = 0; i < jdl_arr_num; i++) {
			jdl = json_object_array_get_idx(jdl_arr, i);

			if (json_get_property(jdl, json_type_string, &vi->downloadable.sha256, "sha-256", NULL))
				return E_UA_ERR;

			if (json_get_property(jdl, json_type_int, &vi->downloadable.length, "length", NULL))
				return E_UA_ERR;

			if (json_get_property(jdl, json_type_string, &vi->downloadable.url, "url", NULL))
				return E_UA_ERR;

			if (json_get_property(jsonObj, json_type_string, &vi->encryption.method, "body", "package", "version-list", version, "encryption", "method", NULL))
				return E_UA_ERR;

			if (json_get_property(jsonObj, json_type_string, &vi->encryption.key, "body", "package", "version-list", version, "encryption", "key", NULL))
				return E_UA_ERR;

			if (!json_get_property(jdl, json_type_string, &vi->downloadable.against_sha256, "against-sha-256", NULL)) // This url is delta URL
				break;
		}
#if 1
		A_INFO_MSG("vi->sha256: %s", vi->sha256);
		A_INFO_MSG("vi->downloaded: %d", vi->downloaded);
		A_INFO_MSG("vi->downloadable.sha256: %s", vi->downloadable.sha256);
		A_INFO_MSG("vi->downloadable.length: %d", vi->downloadable.length);
		A_INFO_MSG("vi->downloadable.url: %s", vi->downloadable.url);
		A_INFO_MSG("vi->encryption.method: %s", vi->encryption.method);
		A_INFO_MSG("vi->encryption.key: %s", vi->encryption.key);
#endif
		return E_UA_OK;
	}

	return E_UA_ERR;

}


int get_trust_info_from_json(json_object* jsonObj, ua_dl_trust_t* dlt) //char ** sync_trust, char ** pkg_trust) {
{
	if (jsonObj && dlt) {
		json_object* jarr = NULL;
		int len           = 0;

		json_get_property(jsonObj, json_type_array, &jarr, "body", "sync", "trust", NULL);
		len = json_object_array_length(jarr);
		if (len > 0)
			dlt->sync_trust = (char*)json_object_get_string(json_object_array_get_idx(jarr, 0));

		json_get_property(jsonObj, json_type_array, &jarr, "body", "sync", "crl", NULL);
		len = json_object_array_length(jarr);
		if (len > 0)
			dlt->sync_crl = (char*)json_object_get_string(json_object_array_get_idx(jarr, 0));

		json_get_property(jsonObj, json_type_array, &jarr, "body", "package", "trust", NULL);
		len = json_object_array_length(jarr);
		if (len > 0)
			dlt->pkg_trust = (char*)json_object_get_string(json_object_array_get_idx(jarr, 0));

		json_get_property(jsonObj, json_type_array, &jarr, "body", "package", "crl", NULL);
		len = json_object_array_length(jarr);
		if (len > 0)
			dlt->pkg_crl = (char*)json_object_get_string(json_object_array_get_idx(jarr, 0));

		return E_UA_OK;

	}

	return E_UA_ERR;
}
#endif

int get_body_rollback_from_json(json_object* jsonObj, int* value)
{
	return json_get_property(jsonObj, json_type_boolean, value, "body", "rollback", NULL);

}

int get_pkg_next_rollback_version(json_object* jsonArr, char* currentVer, char** nextVer)
{
	int i, len, idx, err = E_UA_ERR;
	char* ver = 0;

	if (currentVer && json_object_is_type(jsonArr, json_type_array) && ((len = json_object_array_length(jsonArr)) > 0)) {
		idx = 0;

		for (i = 0; i < len; i++) {
			ver = (char*) json_object_get_string(json_object_array_get_idx(jsonArr, i));
			if (!strcmp(currentVer, ver)) {
				idx = i + 1;
				break;
			}
		}

		if (idx >= 0 && idx < len) {
			*nextVer = (char*) json_object_get_string(json_object_array_get_idx(jsonArr, idx));
			err      = E_UA_OK;
		}

	}

	return err;
}

int json_get_property(json_object* json, enum json_type typ, void* value, const char* node, ... )
{
	int err = E_UA_OK;
	json_object* aux;
	va_list ap;

	va_start(ap, node);

	json_object* obj = json;

	while (node) {
		if (!json_object_object_get_ex(obj, node, &aux)) {
			//A_ERROR_MSG("No %s property in json object", node);
			err = E_UA_ERR;
			break;
		}

		node = va_arg(ap, const char* );
		obj  = aux;

		if (json_object_is_type(obj, typ))
			switch (typ) {
				case json_type_boolean:
					*(int*)value = json_object_get_boolean(obj);
					break;
				case json_type_double:
					*(double*)value = json_object_get_double(obj);
					break;
				case json_type_int:
					*(int64_t*)value = json_object_get_int64(obj);
					break;
				case json_type_string:
					*(char const**)value = json_object_get_string(obj);
					break;
				case json_type_object:
				case json_type_array:
					if (value) {
						*(json_object**)value = obj;
					}
					break;
				default:
					break;
			}
	}

	va_end(ap);

	if (err) {
		switch (typ) {
			case json_type_object:
			case json_type_array:
			case json_type_string:
				*(void**)value = 0;
				break;
			default:
				break;
		}
	}

	return err;

}

