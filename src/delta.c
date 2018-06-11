
#include "delta.h"
#include "xml.h"

static int patch_delta(diff_info_t * diffInfo, const char * old, const char * diff, const char * new);
static int verify_file(const char * file, const char * sha256);
static int uncompress(diff_compression_t cmp, const char * old, const char * new);
extern ua_cfg_t ua_cfg;


char * get_delta_capability() {

    int memory = 100;
    char format[6] = "";
    char compression[6] = "";

    if (!is_cmd_runnable("bspatch"))
        strcat(format, "1,");
    if (!is_cmd_runnable("xdelta3"))
        strcat(format, "2,");
    if (!is_cmd_runnable("espatch"))
        strcat(format, "3,");

    if (!is_cmd_runnable("gzip"))
        strcat(compression, "1,");
    if (!is_cmd_runnable("bzip2"))
        strcat(compression, "2,");
    if (!is_cmd_runnable("xz"))
        strcat(compression, "3,");

    format[strlen(format) - 1] = 0;
    compression[strlen(compression) - 1] = 0;

    return f_asprintf("A:%s;B:%s;C:%d", format, compression, memory);
}


int is_delta_package(char *pkg) {

    return !zip_find_file(pkg, MANIFEST_DIFF);
}


int delta_reconstruct(char * oldPkg, char * diffPkg, char * newPkg) {

    int err = E_UA_OK;
    diff_info_t *di, *aux, *diList = 0;
    char *oldPath = 0, *diffPath = 0, *newPath = 0;
    char *manifest_diff = 0, *manifest_old = 0, *manifest_new = 0;

    do {

        BOLT_SYS(mkdirp(oldPath = JOIN(ua_cfg.cache_dir, "delta", "old"), 0755) && (errno != EEXIST), "failed to make directory %s", oldPath);
        BOLT_SYS(mkdirp(diffPath = JOIN(ua_cfg.cache_dir, "delta", "diff"), 0755) && (errno != EEXIST), "failed to make directory %s", diffPath);
        BOLT_SYS(mkdirp(newPath = JOIN(ua_cfg.cache_dir, "delta", "new"), 0755) && (errno != EEXIST), "failed to make directory %s", newPath);

        manifest_diff = JOIN(diffPath, MANIFEST_DIFF);
        manifest_old = JOIN(oldPath, MANIFEST);
        manifest_new = JOIN(newPath, MANIFEST);

        BOLT_IF(unzip(oldPkg, oldPath), E_UA_ERR, "unzip failed: %s", oldPkg);
        BOLT_IF(unzip(diffPkg, diffPath), E_UA_ERR, "unzip failed: %s", diffPkg);

        BOLT_IF(parse_diff_manifest(manifest_diff, &diList), E_UA_ERR, "failed to parse: %s", manifest_diff);

        DL_FOREACH_SAFE(diList, di, aux) {

            char * oldFile = JOIN(oldPath, di->name);
            char * diffFile = JOIN(diffPath, di->name);
            char * newFile = JOIN(newPath, di->name);

            if(di->type == DT_ADDED) {

                if (copy_file(diffFile, newFile)) err = E_UA_ERR;

            } else if (di->type == DT_UNCHANGED) {

                if (verify_file(oldFile, di->sha256.old) ||
                        copy_file(oldFile, newFile)) err = E_UA_ERR;

            } else if (di->type == DT_CHANGED) {

                if (verify_file(oldFile, di->sha256.old) ||
                        patch_delta(di, oldFile, diffFile, newFile) ||
                        verify_file(newFile, di->sha256.new)) err = E_UA_ERR;

            }

            DL_DELETE(diList, di);
            free(di->name);
            free(di);
            free(oldFile);
            free(diffFile);
            free(newFile);

            if (err) break;
        }

        if (!err) {
            BOLT_IF(copy_file(manifest_old, manifest_new), E_UA_ERR, "failed to copy manifest");
            BOLT_IF(zip(newPkg, newPath), E_UA_ERR, "zip failed: %s", newPkg);
        }

    } while (0);

    if (manifest_diff) free(manifest_diff);
    if (manifest_old)  free(manifest_old);
    if (manifest_new)  free(manifest_new);

#define FREEPATH(x) { if(x) { if(rmdirp(x)) DBG("failed to remove directory %s", x); free(x);}}

    FREEPATH(oldPath);
    FREEPATH(diffPath);
    FREEPATH(newPath);

#undef FREEPATH

    return err;
}


static int patch_delta(diff_info_t * diffInfo, const char * old, const char * diff, const char * new) {

    int err = E_UA_OK;
    char * cmd = 0;

    DBG("patching file old:%s diff:%s new:%s", old, diff, new);

#define COMMAND(NAME, O, D, N) f_asprintf("%s %s %s %s", NAME, O, D, N)

    if (diffInfo->format == DF_BSDIFF) {

        cmd = COMMAND("bspatch", old, new, diff);

    } else if (diffInfo->format == DF_ESDIFF) {

        cmd = COMMAND("espatch", old, new, diff);

    } else if (diffInfo->format == DF_RFC3284) {

        char * diffp = f_asprintf("%s%s", diff, ".patch");
         if (!(err = uncompress(diffInfo->compression, diff, diffp)))
             cmd = COMMAND("xdelta3 -d -s", old, diffp, new);
         free(diffp);

    } else if (diffInfo->format == DF_NONE) {

        err = uncompress(diffInfo->compression, diff, new);

    }
#undef COMMAND

    if (cmd) {
        chkdirp(new);
        DBG("Executing: %s", cmd);
        err = system(cmd);
        free(cmd);
    }

    return err;
}


static int uncompress(diff_compression_t cmp, const char * old, const char * new) {

    int err = E_UA_OK;
    char * cmd = 0;

    DBG("uncompressing file %s to %s", old, new);

#define COMMAND(NAME, O, N) f_asprintf("%s %s > %s", NAME, O, N)

    if (cmp == DC_XZ) {
        cmd = COMMAND("xz -cd", old, new);
    } else if (cmp == DC_BZIP2) {
        cmd = COMMAND("bzip2 -cd", old, new);
    } else if (cmp == DC_GZIP) {
        cmd = COMMAND("gzip -cd", old, new);
    } else if (cmp == DC_NONE) {
        cmd = COMMAND("cat", old, new);
    }
#undef COMMAND

    if (cmd) {
        chkdirp(new);
        DBG("Executing: %s", cmd);
        err = system(cmd);
        free(cmd);
    }

    return err;
}


static int verify_file(const char * file, const char * sha256) {

    int err = E_UA_OK;
    char hash[SHA256_HEX_LENGTH];

    if (!(err = calc_sha256_hex(file, hash))) {

        if (strncmp(hash, sha256, SHA256_HEX_LENGTH - 1)) {
            err = E_UA_ERR;
            DBG("SHA256 Hash mismatch %s : Expected: %s  Calculated: %s", file, sha256, hash);
        }

    } else {
        DBG("SHA256 Hash calculation failed : %s", file);
    }

    return err;
}


diff_format_t diff_format_enum(const char * f) {

    if (!f) { return 0; }

    if (!strcasecmp("bsdiff", f)) {
        return DF_BSDIFF;
    }
    if (!strcasecmp("esdiff", f)) {
        return DF_ESDIFF;
    }
    if (!strcasecmp("rfc3284", f)) {
        return DF_RFC3284;
    }
    if (!strcasecmp("none", f)) {
        return DF_NONE;
    }

    return 0;
}


diff_compression_t diff_compression_enum(const char * c) {

    if (!c) { return 0; }

    if (!strcasecmp("xz", c)) {
        return DC_XZ;
    }
    if (!strcasecmp("bzip2", c)) {
        return DC_BZIP2;
    }
    if (!strcasecmp("gzip", c)) {
        return DC_GZIP;
    }
    if (!strcasecmp("none", c)) {
        return DC_NONE;
    }

    return 0;
}

