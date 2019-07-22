/*File: libuac.i*/

%module libuamodule
%include "pua.h"

%{
#define SWIG_FILE_WITH_INIT
#include "pua.h"
%}

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

} ua_cfg_t;

