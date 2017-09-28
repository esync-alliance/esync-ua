/*
 * utils.c
 */

#include "utils.h"
#include "debug.h"

static int get_json_string(json_object * json, char ** value, const char * node, ... );
static int get_json_int64(json_object * json, int64_t * value, const char * node, ... );



int get_type_from_json(json_object * jsonObj, char ** value) {

    return get_json_string(jsonObj, value, "type", NULL);
}


int get_replyid_from_json(json_object * jsonObj, char ** value) {

    return get_json_string(jsonObj, value, "reply-id", NULL);
}


int get_pkg_version_from_json(json_object * jsonObj, char ** value) {

    return get_json_string(jsonObj, value, "body", "package", "version", NULL);
}


int get_pkg_type_from_json(json_object * jsonObj, char ** value) {

    return get_json_string(jsonObj, value, "body", "package", "type", NULL);
}


int get_pkg_name_from_json(json_object * jsonObj, char ** value) {

    return get_json_string(jsonObj, value, "body", "package", "name", NULL);
}


int get_file_from_json(json_object * jsonObj, char * version, char ** value) {

    return get_json_string(jsonObj, value, "body", "package", "version-list", version, "file", NULL);
}


int get_pkg_status_from_json(json_object * jsonObj, char ** value) {

    return get_json_string(jsonObj, value, "body", "package", "status", NULL);
}

int get_bytes_downloaded_from_json(json_object * jsonObj, int64_t * value) {

    return get_json_int64(jsonObj, value, "body", "downloaded-bytes", NULL);
}

int get_bytes_total_from_json(json_object * jsonObj, int64_t * value) {

    return get_json_int64(jsonObj, value, "body", "total-bytes", NULL);
}



static int get_json_string(json_object * json, char ** value, const char * node, ... ) {

    int err = E_UA_OK;
    json_object * aux;
    va_list ap;
    va_start( ap, node);

    json_object * jObject = json;

    while (node) {
        BOLT_IF(!json_object_object_get_ex(jObject, node, &aux) ||
                !(json_object_is_type(aux, json_type_object) || json_object_is_type(aux, json_type_string)),
                E_UA_ERR, "No %s property in %s", node, json_object_to_json_string_ext(json, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));

        node = va_arg( ap, const char* );
        jObject = aux;

        if (json_object_is_type(jObject, json_type_string))
            *value = (char *) json_object_get_string(jObject);
    }

    if (err != E_UA_OK) {
        *value = 0;
    }

    return err;
}

static int get_json_int64(json_object * json, int64_t * value, const char * node, ... ) {

    int err = E_UA_OK;
    json_object * aux;
    va_list ap;
    va_start( ap, node);

    json_object * jObject = json;

    while (node) {
        BOLT_IF(!json_object_object_get_ex(jObject, node, &aux) ||
                !(json_object_is_type(aux, json_type_object) || json_object_is_type(aux, json_type_int)),
                E_UA_ERR, "No %s property in %s", node, json_object_to_json_string_ext(json, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));

        node = va_arg( ap, const char* );
        jObject = aux;

        if (json_object_is_type(jObject, json_type_int))
            *value = json_object_get_int64(jObject);
    }

    if (err != E_UA_OK) {
        *value = 0;
    }

    return err;
}
