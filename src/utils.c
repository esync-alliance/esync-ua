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


int get_file_from_json(json_object * jsonObj, char * version, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "body", "package", "version-list", version, "file", NULL);

}


int get_pkg_status_from_json(json_object * jsonObj, char ** value) {

    return json_get_property(jsonObj, json_type_string, value, "body", "package", "status", NULL);

}


int get_downloaded_bytes_from_json(json_object * jsonObj, int64_t * value) {

    return json_get_property(jsonObj, json_type_int, value, "body", "downloaded-bytes", NULL);

}


int get_total_bytes_from_json(json_object * jsonObj, int64_t * value) {

    return json_get_property(jsonObj, json_type_int, value, "body", "total-bytes", NULL);

}



static int json_get_property(json_object * json, enum json_type typ, void * value, const char * node, ... ) {

    int err = E_UA_OK;
    json_object * aux;
    va_list ap;
    va_start( ap, node);

    json_object * obj = json;

    while (node) {
        BOLT_IF(!json_object_object_get_ex(obj, node, &aux), E_UA_ERR, "No %s property in %s", node, json_object_to_json_string(json));

        node = va_arg( ap, const char* );
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

    if (err != E_UA_OK) {
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

