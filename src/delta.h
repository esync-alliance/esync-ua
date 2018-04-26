/*
 * delta.h
 */
#ifndef _UA_DELTA_H_
#define _UA_DELTA_H_

#include "common.h"

#define SHA256_STRING_LENGTH    SHA256_DIGEST_LENGTH * 2 + 1
#define MANIFEST_DIFF           "manifest_diff.xml"
#define MANIFEST_PKG            "manifest_pkg.xml"
#define MANIFEST                "manifest.xml"

typedef enum diff_type {
    DT_ADDED = 1,
    DT_REMOVED,
    DT_CHANGED,
    DT_UNCHANGED
} diff_type_t;

typedef enum diff_format {
    DF_BSDIFF = 1,
    DF_ESDIFF,
    DF_RFC3284,
    DF_NONE
} diff_format_t;

typedef enum diff_compression {
    DC_XZ = 1,
    DC_BZIP2,
    DC_GZIP,
    DC_NONE
} diff_compression_t;

typedef struct diff_info {

    diff_type_t type;
    char * name;
    struct {
        unsigned char old[SHA256_STRING_LENGTH];
        unsigned char new[SHA256_STRING_LENGTH];
    } sha256;
    diff_format_t format;
    diff_compression_t compression;

    struct diff_info * next;
    struct diff_info * prev;
} diff_info_t;


char * get_delta_capability();
int is_delta_package(char *pkg);
int delta_reconstruct(char * oldPkg, char * diffPkg, char * newPkg);
diff_format_t diff_format_enum(const char * f);
diff_compression_t diff_compression_enum(const char * c);

#endif /* _UA_DELTA_H_ */
