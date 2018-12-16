/*
 * utils.c
 */

#include "utils.h"


static int json_get_property(json_object * json, enum json_type typ, void * value, const char * node, ... );



int get_type_from_json(json_object * jsonObj, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "type", NULL);

}


int get_replyid_from_json(json_object * jsonObj, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "reply-id", NULL);

}


int get_pkg_version_from_json(json_object * jsonObj, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "body", "package", "version", NULL);

}


int get_pkg_type_from_json(json_object * jsonObj, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "body", "package", "type", NULL);

}


int get_pkg_name_from_json(json_object * jsonObj, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "body", "package", "name", NULL);

}


int get_pkg_file_from_json(json_object * jsonObj, char * version, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "body", "package", "version-list", version, "file", NULL);

}


int get_pkg_downloaded_from_json(json_object * jsonObj, char * version, int * value) {

    return json_get_property(jsonObj, json_type_boolean, value, "body", "package", "version-list", version, "downloaded", NULL);

}


int get_pkg_rollback_version_from_json(json_object * jsonObj, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "body", "package", "rollback-version", NULL);

}

int get_pkg_rollback_versions_from_json(json_object * jsonObj, json_object ** value) {

    return json_get_property(jsonObj, json_type_array, value, "body", "package", "rollback-versions", NULL);

}


int get_downloaded_bytes_from_json(json_object * jsonObj, int64_t * value) {

    return json_get_property(jsonObj, json_type_int, value, "body", "downloaded-bytes", NULL);

}


int get_total_bytes_from_json(json_object * jsonObj, int64_t * value) {

    return json_get_property(jsonObj, json_type_int, value, "body", "total-bytes", NULL);

}


int get_pkg_sha256_from_json(json_object * jsonObj, char * version, char value[SHA256_B64_LENGTH]) {

    int err = E_UA_OK;
    char * sha256 = 0;
    if (!json_get_property(jsonObj, json_type_string, &sha256, "body", "package", "version-list", version, "sha-256", NULL) &&
            (strlen(sha256) == (SHA256_B64_LENGTH - 1))) {
        strcpy(value, sha256);
    } else {
        err = E_UA_ERR;
    }
    return err;

}


int get_pkg_next_rollback_version(json_object * jsonArr, char * currentVer, char ** nextVer) {

    int i, len, idx, err = E_UA_OK;
    char * ver = 0;
    *nextVer = 0;
    idx = 0;
    
    if (currentVer && json_object_is_type(jsonArr, json_type_array) && ((len = json_object_array_length(jsonArr)) > 0)) {
 
        idx = len - 1;

        for (i = len-1; i >= 0; i--) {
            ver = (char*) json_object_get_string(json_object_array_get_idx(jsonArr, i));
            if (!strcmp(currentVer, ver)) {
                idx = i - 1;
                break;
            }
        }

        if (idx >= 0) {
            *nextVer = (char*) json_object_get_string(json_object_array_get_idx(jsonArr, idx));
        } else {
            err = E_UA_ERR;
        }
        
    } else {
        err = E_UA_ERR;
    }

    return err;
}



static int json_get_property(json_object * json, enum json_type typ, void * value, const char * node, ... ) {

    int err = E_UA_OK;
    json_object * aux;
    va_list ap;
    va_start(ap, node);

    json_object * obj = json;

    while (node) {
        BOLT_IF(!json_object_object_get_ex(obj, node, &aux), E_UA_ERR, "No %s property in json object", node);

        node = va_arg(ap, const char* );
        obj = aux;

        if (json_object_is_type(obj, typ))
            switch (typ) {
                case json_type_boolean:
                    *(int *)value = json_object_get_boolean(obj);
                    break;
                case json_type_double:
                    *(double*)value = json_object_get_double(obj);
                    break;
                case json_type_int:
                    *(int64_t *)value = json_object_get_int64(obj);
                    break;
                case json_type_string:
                    *(char const **)value = json_object_get_string(obj);
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

