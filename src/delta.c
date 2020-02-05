
#include "delta.h"
#include "xml.h"
#include "utlist.h"
#include "debug.h"

static int add_delta_tool(delta_tool_hh_t** hash, const delta_tool_t* tool, int count, int isPatchTool);
static void clear_delta_tool(delta_tool_hh_t* hash);
static char* get_deflt_delta_cap(delta_tool_hh_t* patchTool, delta_tool_hh_t* decompTool);
static char* get_config_delta_cap(char* delta_cap);
static char* expand_tool_args(const char* args, const char* old, const char* new, const char* diff);
static int delta_patch(diff_info_t* diffInfo, const char* old, const char* new, const char* diff);
static int verify_file(const char* file, const char* sha256);

delta_stg_t delta_stg = {0};

const delta_tool_t deflt_patch_tools[] = {
	{"bsdiff", "bspatch", OFA " "NFA " "PFA, 1},
	{"rfc3284", "xdelta3", "-D -d -s "OFA " "PFA " "NFA, 0},
	{"esdiff", "espatch", OFA " "NFA " "PFA, 1}
};

const delta_tool_t deflt_decomp_tools[] = {
	{"gzip", "gzip", "-cd "OFA " > "NFA, 0},
	{"bzip2", "bzip2", "-cd "OFA " > "NFA, 0},
	{"xz", "xz", "-cd "OFA " > "NFA, 0}
};


int delta_init(char* cacheDir, delta_cfg_t* deltaConfig)
{
	int err = E_UA_OK;

	do {
		memset(&delta_stg, 0, sizeof(delta_stg_t));

		BOLT_IF(!S(cacheDir) || chkdirp(delta_stg.cache_dir = cacheDir), E_UA_ARG, "cache directory invalid");

		BOLT_IF(add_delta_tool(&delta_stg.patch_tool, deflt_patch_tools, sizeof(deflt_patch_tools)/sizeof(deflt_patch_tools[0]), 1), E_UA_ERR, "default patch tools adding failed");
		BOLT_IF(add_delta_tool(&delta_stg.decomp_tool, deflt_decomp_tools, sizeof(deflt_decomp_tools)/sizeof(deflt_decomp_tools[0]), 0), E_UA_ERR, "default decompression tools adding failed");

		BOLT_IF(deltaConfig && deltaConfig->patch_tools && add_delta_tool(&delta_stg.patch_tool, deltaConfig->patch_tools, deltaConfig->patch_tool_cnt, 1), E_UA_ARG, "patch tools adding failed");
		BOLT_IF(deltaConfig && deltaConfig->decomp_tools && add_delta_tool(&delta_stg.decomp_tool, deltaConfig->decomp_tools, deltaConfig->decomp_tool_cnt, 0), E_UA_ARG, "decompression tools adding failed");

		delta_stg.delta_cap = (deltaConfig && S(deltaConfig->delta_cap)) ? get_config_delta_cap(deltaConfig->delta_cap) :
		                      get_deflt_delta_cap(delta_stg.patch_tool, delta_stg.decomp_tool);

	} while (0);

	if (err) {
		delta_stop();
	}

	return err;
}


void delta_stop()
{
	if (delta_stg.patch_tool != NULL) {
		clear_delta_tool(delta_stg.patch_tool);
		delta_stg.patch_tool = NULL;
	}

	if (delta_stg.decomp_tool != NULL) {
		clear_delta_tool(delta_stg.decomp_tool);
		delta_stg.decomp_tool = NULL;
	}

	if (delta_stg.delta_cap != NULL) {
		free(delta_stg.delta_cap);
		delta_stg.delta_cap = NULL;
	}

}


const char* get_delta_capability()
{
	return delta_stg.delta_cap;
}


int is_delta_package(const char* pkgFile)
{
	return !libzip_find_file(pkgFile, MANIFEST_DIFF);
}

int delta_use_external_algo(void)
{
	return delta_stg.use_external_algo;
}

int delta_reconstruct(const char* oldPkgFile, const char* diffPkgFile, const char* newPkgFile)
{
	int err = E_UA_OK;
	diff_info_t* di, * aux, * diList = 0;
	char* oldFile, * diffFile, * newFile;
	char* oldPath       = 0, * diffPath = 0, * newPath = 0;
	char* diff_manifest = 0, * manifest_old = 0, * manifest_diff = 0, * manifest_new = 0;
	char* top_delta_dir = 0;

	do {
		top_delta_dir = JOIN(delta_stg.cache_dir, "delta");
		if (top_delta_dir) {
			if (!access(top_delta_dir, W_OK))
				rmdirp(top_delta_dir);
			free(top_delta_dir);
		}

#define DTR_MK(type) \
	BOLT_SYS(newdirp(type ## Path = JOIN(delta_stg.cache_dir, "delta", # type), 0755) && (errno != EEXIST), "failed to make directory %s", type ## Path); \
	manifest_ ## type = JOIN(type ## Path, MANIFEST); do { \
	} while (0)

		DTR_MK(old);
		DTR_MK(diff);
		DTR_MK(new);

#undef DTR_MK

		diff_manifest = JOIN(diffPath, MANIFEST_DIFF);

		BOLT_IF(unzip(oldPkgFile, oldPath), E_UA_ERR, "unzip failed: %s", oldPkgFile);
		BOLT_IF(unzip(diffPkgFile, diffPath), E_UA_ERR, "unzip failed: %s", diffPkgFile);

		BOLT_IF(parse_diff_manifest(diff_manifest, &diList), E_UA_ERR, "failed to parse: %s", diff_manifest);

		DL_FOREACH_SAFE(diList, di, aux) {
			if (!err) {
				oldFile  = JOIN(oldPath, di->name);
				diffFile = JOIN(diffPath, di->name);
				newFile  = JOIN(newPath, di->name);

				if (di->type == DT_ADDED) {
					if (verify_file(diffFile, di->sha256.new) || copy_file(diffFile, newFile)) err = E_UA_ERR;

				} else if (di->type == DT_UNCHANGED) {
					if (verify_file(oldFile, di->sha256.old) || copy_file(oldFile, newFile)) err = E_UA_ERR;

				} else if (di->type == DT_CHANGED) {
					if (verify_file(oldFile, di->sha256.old) || delta_patch(di, oldFile, newFile, diffFile) || verify_file(newFile, di->sha256.new)) err = E_UA_ERR;

				}

				if (!access(oldFile, R_OK))
					remove(oldFile);
				if (!access(diffFile, R_OK))
					remove(diffFile);

				free(oldFile);
				free(diffFile);
				free(newFile);

			}

			DL_DELETE(diList, di);
			free_diff_info(di);

		}

		if (!err) {
			rmdirp(oldPath);
			BOLT_IF(copy_file(manifest_diff, manifest_new), E_UA_ERR, "failed to copy manifest");
			rmdirp(diffPath);
			BOLT_IF(zip(newPkgFile, newPath), E_UA_ERR, "zip failed: %s", newPkgFile);
		}

	} while (0);

#define DTR_RM(type) \
	if ((type ## Path) ) { if (!access(type ## Path, F_OK) && rmdirp(type ## Path)) DBG("error removing directory %s", type ## Path); free(type ## Path); } \
	Z_FREE(manifest_ ## type); do { } while (0)

	DTR_RM(old);
	DTR_RM(diff);
	DTR_RM(new);

#undef DTR_RM

	Z_FREE(diff_manifest);

	return err;
}


static int add_delta_tool(delta_tool_hh_t** hash, const delta_tool_t* tool, int count, int isPatchTool)
{
	int i, err = E_UA_OK;
	delta_tool_hh_t* dth, * aux;

	for (i = 0; i < count; i++) {
		if (S((tool + i)->algo) && S((tool + i)->path) && S((tool + i)->args) &&
		    !is_cmd_runnable((tool + i)->path) &&
		    (SUBSTRCNT((tool + i)->args, OFA) == 1) && (SUBSTRCNT((tool + i)->args, NFA) == 1) && (SUBSTRCNT((tool + i)->args, PFA) == (isPatchTool ? 1 : 0))) {
			dth            = f_malloc(sizeof(delta_tool_hh_t));
			dth->tool.algo = STRLWR(f_strdup((tool + i)->algo));
			dth->tool.path = f_strdup((tool + i)->path);
			dth->tool.args = f_strdup((tool + i)->args);
			dth->tool.intl = (tool + i)->intl;

			HASH_FIND_STR(*hash, dth->tool.algo, aux);
			if (!aux) {
				HASH_ADD_STR(*hash, tool.algo, dth);
			} else {
				HASH_REPLACE_STR(*hash, tool.algo, dth, aux);
				free_delta_tool_hh(aux);
			}
		} else if (tool != deflt_patch_tools && tool != deflt_decomp_tools) {
			DBG("failed to add %s tool: %s %s", (isPatchTool ? "patch" : "diff"), SAFE_STR((tool + i)->algo), SAFE_STR((tool + i)->path));
			err = E_UA_ERR;
			break;
		}
	}

	return err;
}


static void clear_delta_tool(delta_tool_hh_t* hash)
{
	delta_tool_hh_t* dth, * aux;

	if (!hash) { return; }

	HASH_ITER(hh, hash, dth, aux) {
		HASH_DEL(hash, dth);
		free_delta_tool_hh(dth);

	}

}


static int get_espatch_version(char* ver, int len)
{
	int rc = E_UA_OK;

	if (ver) {
		char output[64];
		FILE* pf;

		pf = popen("espatch -v", "r");
		if (!pf) {
			DBG("Could not run espatch to determine its version, using default V2.5.");
			rc = E_UA_ERR;

		}else {
			if (fgets(output, sizeof(output), pf) != NULL) {
				if (output[strlen(output) - 1] == '\n')
					output[strlen(output) - 1] = 0;
				char* tmp = strstr(output, "version: ");
				if (tmp && (strlen(tmp)- strlen("version: ") < len-1)) {
					memset(ver, 0, len);
					strncpy(ver, tmp+strlen("version: "), len-1);
					DBG("espatch outputs version: %s", ver);
				} else {
					DBG("error: could not parse version from espatch output: %s.", output);
					rc = E_UA_ERR;
				}

			} else {
				DBG("error: espatch has no output.");
				rc = E_UA_ERR;
			}

			pclose(pf);

		}

	}

	return rc;
}

static char* get_config_delta_cap(char* delta_cap)
{
	char espatch_ver[7] = "";
	char* ret_cap       = NULL;

	if (delta_cap) {
		char* tmp_cap = f_strdup(delta_cap);
		char* tmp     = strtok(tmp_cap, ";");
		if (tmp && *tmp == 'A') {
			char* format = strstr(tmp, ":");
			while (format) {
				if (!strstr(delta_cap, "E:") && *(format+1) == '3') {
					if (E_UA_OK == get_espatch_version(espatch_ver, sizeof(espatch_ver)))
						ret_cap = f_asprintf("%s;E:%s", delta_cap, espatch_ver);
				} else if (*(format+1) == '4') {
					delta_stg.use_external_algo = 1;
				}

				tmp    = format + 1;
				format = strstr(tmp, ",");
			}

		}

		if (ret_cap == NULL)
			ret_cap = f_strdup(delta_cap);
		free(tmp_cap);
	}

	return ret_cap;
}

static char* get_deflt_delta_cap(delta_tool_hh_t* patchTool, delta_tool_hh_t* decompTool)
{
	int memory            = 100;
	char format[7]        = "";
	char compression[7]   = "";
	char espatch_ver[7]   = "";
	int espatch_ver_valid = E_UA_ERR;
	delta_tool_hh_t* dth  = 0;

	HASH_FIND_STR(patchTool, "bsdiff", dth);
	if (dth) strcat(format, "1,");
	HASH_FIND_STR(patchTool, "rfc3284", dth);
	if (dth) strcat(format, "2,");
	HASH_FIND_STR(patchTool, "esdiff", dth);
	if (dth) {
		strcat(format, "3,");
		espatch_ver_valid = get_espatch_version(espatch_ver, sizeof(espatch_ver));
	}

	HASH_FIND_STR(decompTool, "gzip", dth);
	if (dth) strcat(compression, "1,");
	HASH_FIND_STR(decompTool, "bzip2", dth);
	if (dth) strcat(compression, "2,");
	HASH_FIND_STR(decompTool, "xz", dth);
	if (dth) strcat(compression, "3,");

	format[strlen(format) - 1]           = 0;
	compression[strlen(compression) - 1] = 0;

	if (espatch_ver_valid == E_UA_OK)
		return f_asprintf("A:%s;B:%s;C:%d;E:%s", format, compression, memory, espatch_ver);
	else
		return f_asprintf("A:%s;B:%s;C:%d", format, compression, memory);
}


static char* expand_tool_args(const char* args, const char* old, const char* new, const char* diff)
{
	int i = 0;
	const char* rp, * ex, * s = args;

	char* res = f_malloc(strlen(s) + 1 +
	                     ((old) ? SUBSTRCNT(s, OFA) * (strlen(old) - strlen(OFA)) : 0) +
	                     ((new) ? SUBSTRCNT(s, NFA) * (strlen(new) - strlen(NFA)) : 0) +
	                     ((diff) ? SUBSTRCNT(s, PFA) * (strlen(diff) - strlen(PFA)) : 0));

	while (*s) {
		if (((rp = old) && (strcasestr(s, ex = OFA) == s)) || ((rp = new) && (strcasestr(s, ex = NFA) == s)) || ((rp = diff) && (strcasestr(s, ex = PFA) == s))) {
			strcpy(&res[i], rp);
			i += strlen(rp);
			s += strlen(ex);
		} else {
			res[i++] = *s++;
		}
	}
	res[i] = 0;

	return res;
}



static int delta_patch(diff_info_t* diffInfo, const char* old, const char* new, const char* diff)
{
	int err     = E_UA_OK;
	char* cmd   = 0;
	char* diffp = 0;
	char* targs;
	delta_tool_hh_t* decompdth, * patchdth = 0;

	do {
#define TOOL_EXEC(_t, _o, _n, _p) { \
		targs = expand_tool_args(_t->tool.args, _o, _n, _p); \
		cmd   = f_asprintf("%s %s", _t->tool.path, targs); \
		DBG("Executing: %s", cmd); \
		if (system(cmd)) err = E_UA_SYS; \
		free(targs); \
		free(cmd); \
} do {} while (0)

		if (strcmp(diffInfo->format, "none")) {
			HASH_FIND_STR(delta_stg.patch_tool, diffInfo->format, patchdth);
			BOLT_IF(!patchdth, E_UA_ERR, "Diff format %s not found", diffInfo->format);
		}

		if (strcmp(diffInfo->compression, "none") && !(patchdth && patchdth->tool.intl)) {
			HASH_FIND_STR(delta_stg.decomp_tool, diffInfo->compression, decompdth);
			BOLT_IF(!decompdth, E_UA_ERR, "Decompression %s not found", diffInfo->compression);

			diffp = f_asprintf("%s%s", diff, ".z");
			TOOL_EXEC(decompdth, diff, diffp, 0);
			if (err) { DBG_SYS("Decompression failed"); break; }
		}

		if (patchdth) {
			chkdirp(new);
			TOOL_EXEC(patchdth, old, new, diffp ? diffp : diff);
			if (err) { DBG_SYS("Patching failed"); break; }
		} else {
			BOLT_SUB(copy_file(diffp ? diffp : diff, new));
		}

#undef TOOL_EXEC

	} while (0);

	if (diffp) free(diffp);

	return err;
}


static int verify_file(const char* file, const char* sha256)
{
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

void free_delta_tool_hh(delta_tool_hh_t* dth)
{
	Z_FREE(dth->tool.algo);
	Z_FREE(dth->tool.path);
	Z_FREE(dth->tool.args);
	Z_FREE(dth);

}

void free_diff_info(diff_info_t* di)
{
	Z_FREE(di->name);
	Z_FREE(di->format);
	Z_FREE(di->compression);
	Z_FREE(di);

}

