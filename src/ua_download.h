/* Copyright Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * ua_downlaod.h
 *
 */

#ifndef _UA_DOWNLOAD_H
#define _UA_DOWNLOAD_H

#include "eua_json.h"
#include "handler.h"
#include <dmclient/auth_ext.h>
#include <dmclient/security.h>
#include <dmclient/download.h>

enum ua_dl_step {
	UA_DL_STEP_NONE       = 0,
	UA_DL_STEP_DOWNLOADED = 1,
	UA_DL_STEP_ENCRYPTED  = 2,
	UA_DL_STEP_VERIFIED   = 3,
	UA_DL_STEP_DONE       = 4
};

#define UA_DL_REPORT_BLK_SIZE 512*1024
#define E_TAG_MAX_SIZE        128
#define MAX_SIGCA_LIST        3
typedef struct ua_dl_record {
	int bytes_written;
	int step;
	unsigned int crc32;
	int last_bytes_written;
	unsigned int last_crc32;
	int e_tag_valid;
	char e_tag[E_TAG_MAX_SIZE];
} ua_dl_record_t;

typedef struct ua_dl_context {
	char* name;
	char* version;
	char* dl_pkg_filename;
	char* dl_rec_filename;
	char* dl_encrytion_filename;
	char* dl_zip_folder;
	ua_dl_record_t dl_rec;
	pkg_info_t* pkg_info;
	ua_dl_info_t dl_info;
	int bytes_to_report;
	char* ca_file;
	dmclient_cert_ck_f cert_auth;
	ua_dl_trust_t dl_trust;
}ua_dl_context_t;

int ua_dl_start_download(pkg_info_t* pkgInfo );
int ua_dl_set_trust_info(ua_dl_trust_t* trust);


#endif //_UA_DOWNLOAD_H
