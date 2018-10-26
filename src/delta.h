/*
 * delta.h
 */
#ifndef _UA_DELTA_H_
#define _UA_DELTA_H_

#include "common.h"


#define MANIFEST_DIFF           "manifest_diff.xml"
#define MANIFEST_PKG            "manifest_pkg.xml"
#define MANIFEST                "manifest.xml"
#define XL4_X_PREFIX            "xl4-x-"
#define XL4_SIGNATURE_PREFIX    "xl4-signature"


typedef enum diff_type {
    DT_ADDED = 1,
    DT_REMOVED,
    DT_CHANGED,
    DT_UNCHANGED
} diff_type_t;

typedef struct diff_info {

    diff_type_t type;
    char * name;
    struct {
        char old[SHA256_HEX_LENGTH];
        char new[SHA256_HEX_LENGTH];
    } sha256;
    char * format;
    char * compression;

    struct diff_info * next;
    struct diff_info * prev;
} diff_info_t;


typedef struct delta_tool_hh {
    delta_tool_t tool;
    UT_hash_handle hh;
} delta_tool_hh_t;

typedef struct delta_stg {

    char * delta_cap;
    char * cache_dir;
    delta_tool_hh_t * patch_tool;
    delta_tool_hh_t * decomp_tool;

} delta_stg_t;


#define OFA "{o}"
#define NFA "{n}"
#define PFA "{p}"


int delta_init(char * cacheDir, delta_cfg_t * deltaConfig);
void delta_stop();
const char * get_delta_capability();
int is_delta_package(const char *pkgFile);
int delta_reconstruct(const char * oldPkgFile, const char * diffPkgFile, const char * newPkgFile);

void free_diff_info(diff_info_t * di);
void free_delta_tool_hh (delta_tool_hh_t * dth);

#endif /* _UA_DELTA_H_ */
