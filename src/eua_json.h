/* Copyright Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * eua_json.h- eSync UA JSON helper.
 *
 */

#ifndef _EUA_JSON_H
#define _EUA_JSON_H

typedef enum ua_dl_error {
	DLE_PREPARE,
	DLE_CONNECT,
	DLE_AUTH,
	DLE_DOWNLOAD,
	DLE_VERIFY,
	DLE_STORE,
	DLE_PROMPT,
	DLE_DENIED,
	DLE_POSTPONED,

}ua_dl_error_t;

typedef struct ua_dl_info {
	uint64_t total_bytes;
	uint64_t downloaded_bytes;
	uint64_t expected_bytes;
	uint64_t no_download;
	uint64_t completed_download;
	char* error;

} ua_dl_info_t;

typedef struct vi_encryption {
	char* method;
	char* key;
}vi_encryption_t;

typedef struct vi_downloadable {
	char* sha256;
	uint64_t length;
	char* url;
	char* against_sha256;

}vi_downloadable_t;

typedef struct version_item {
	int downloaded; /*boolean*/
	char* sha256;
	vi_downloadable_t downloadable;
	vi_encryption_t encryption;

}version_item_t;

typedef struct ua_dl_trust {
	char* sync_trust;
	char* sync_crl;
	char* pkg_trust;
	char* pkg_crl;

}ua_dl_trust_t;

#endif //_EUA_JSON_H
