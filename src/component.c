/* Copyright Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * component.c
 *
 */

#include "component.h"
#include "debug.h"
static char* st_string[] = {
	"UA_STATE_UNKNOWN",
	"UA_STATE_IDLE_INIT",
	"UA_STATE_READY_DOWNLOAD_STARTED",
	"UA_STATE_READY_DOWNLOAD_DONE",
	"UA_STATE_PREPARE_UPDATE_STARTED",
	"UA_STATE_PREPARE_UPDATE_DONE",
	"UA_STATE_READY_UPDATE_STARTED",
	"UA_STATE_READY_UPDATE_DONE",
	"UA_STATE_CONFIRM_UPDATE_STARTED",
	"UA_STATE_CONFIRM_UPDATE_DONE",
};

void comp_release_state_info(comp_state_info_t* cs_head)
{
	comp_state_info_t* cur, * tmp = NULL;

	HASH_ITER(hh, cs_head, cur, tmp) {
		HASH_DEL(cs_head,cur);
		f_free(cur->fake_rb_ver);
		f_free(cur->pkg_name);
		f_free(cur);
	}
}

ua_state_t comp_get_update_stage(comp_state_info_t* cs_head, char* pkg_name)
{
	ua_state_t st         = UA_STATE_UNKNOWN;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(cs_head, pkg_name, cs);
	if (cs) {
		st = cs->stage;
	}
	DBG("update stage of %s is %s", pkg_name, st_string[st]);
	return st;
}

int comp_set_update_stage(comp_state_info_t** cs_head, char* pkg_name, ua_state_t stage)
{
	int rc                = E_UA_OK;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(*cs_head, pkg_name, cs);
	if (cs) {
		DBG("change update stage of %s to %s", pkg_name, st_string[stage]);
		cs->stage = stage;
	} else {
		DBG("init update stage of %s  to %s", pkg_name, st_string[stage]);
		cs = (comp_state_info_t*)malloc(sizeof(comp_state_info_t));
		memset(cs, 0, sizeof(comp_state_info_t));
		cs->pkg_name = f_strdup(pkg_name);
		cs->stage    = stage;
		HASH_ADD_KEYPTR( hh, *cs_head, cs->pkg_name, strlen(cs->pkg_name ), cs );
	}

	return rc;
}

update_rollback_t comp_get_rb_type(comp_state_info_t* cs_head, char* pkg_name)
{
	update_rollback_t ret = URB_NONE;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(cs_head, pkg_name, cs);
	if (cs) {
		ret = cs->rb_type;
	}
	//DBG("%s rb_type is %d", pkg_name, ret);
	return ret;
}

int comp_set_rb_type(comp_state_info_t** cs_head, char* pkg_name, update_rollback_t rb_type)
{
	int rc                = E_UA_OK;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(*cs_head, pkg_name, cs);
	if (cs) {
		DBG("change rb_type of %s to %d", pkg_name, rb_type);
		cs->rb_type = rb_type;
	} else {
		DBG("init rb_type of %s  to %d", pkg_name, rb_type);
		cs = (comp_state_info_t*)malloc(sizeof(comp_state_info_t));
		memset(cs, 0, sizeof(comp_state_info_t));
		cs->pkg_name = f_strdup(pkg_name);
		cs->rb_type  = rb_type;
		HASH_ADD_KEYPTR( hh, *cs_head, cs->pkg_name, strlen(cs->pkg_name ), cs );
	}

	return rc;
}


char* comp_get_fake_rb_version(comp_state_info_t* cs_head, char* pkg_name)
{
	char* fake_ver          = NULL;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(cs_head, pkg_name, cs);
	if (cs) {
		fake_ver = cs->fake_rb_ver;
	}
	DBG("comp_get_fake_rb_version is %s", fake_ver?fake_ver:"NIL_V");
	return fake_ver;
}

int comp_set_fake_rb_version(comp_state_info_t** cs_head, char* pkg_name, char* fake_ver)
{
	int rc                = E_UA_OK;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(*cs_head, pkg_name, cs);
	if (cs) {
		DBG("change fake rb version of %s to %s", pkg_name, fake_ver);
		f_free(cs->fake_rb_ver);
		cs->fake_rb_ver = f_strdup(fake_ver);
	} else {
		DBG("init fake rb version of %s to %s", pkg_name, fake_ver);
		cs = (comp_state_info_t*)malloc(sizeof(comp_state_info_t));
		memset(cs, 0, sizeof(comp_state_info_t));
		cs->pkg_name           = f_strdup(pkg_name);
		cs->fake_rb_ver = f_strdup(fake_ver);
		HASH_ADD_KEYPTR( hh, *cs_head, cs->pkg_name, strlen(cs->pkg_name ), cs );
	}

	return rc;
}