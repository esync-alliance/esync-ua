/*File: libuac.i*/

%module libuamodule
%include "pua.h"

%{
#define SWIG_FILE_WITH_INIT
#include "pua.h"
%}

typedef struct delta_cfg {
	char* delta_cap;
	delta_tool_t* patch_tools;
	int patch_tool_cnt;
	delta_tool_t* decomp_tools;
	int decomp_tool_cnt;

} delta_cfg_t;

typedef struct ua_cfg {
	// specifies the URL of the broker
	char* url;

	// specifies certificate directory
	char* cert_dir;

	// specifies the cache directory
	char* cache_dir;

	// specifies the backup directory
	char* backup_dir;

	// enables delta support
	int delta;

	// delta configuration
	delta_cfg_t* delta_config;

	// enables debug messages
	int debug;

	// specifies the buffer size for read/write, in kilobytes
	long rw_buffer_size;

	//indicate whether to use library reboot feature.
	//0 = default, ua implements its own reboot/resume support.
	//1 = ua uses libary's reboot/resume support.
	int reboot_support;

	// specifies which source package file used for backup,
	// when  delta reconstruction is triggered.
	// 0 = Use the actual full path used for installation, this is default.
	// 1 = Use the full path resulted from delta reconstruction.
	// Both may or may not be the same full path.
	int backup_source;

	//Indicate whether to disable sha verification of downloaded package.
	//0 = default, verify downloaded package against sha256
	//1 = disable, do not verify downloaded package against sha256
	int package_verification_disabled;

	//Enable fake rollback version in query-package response.
	//0 = default, do NOT use fake rollback version.
	//1 = enable, use fake rollback version.
	int enable_fake_rb_ver;

#ifdef SUPPORT_UA_DOWNLOAD
	// specifies whether UA is to handle package download.
	int ua_download_required;
	// specifies directory used for UA download.
	char* ua_dl_dir;
	// specifies sigca bundle  directory
	char* sigca_dir;
#endif
} ua_cfg_t;

