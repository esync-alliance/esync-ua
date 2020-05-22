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

void comp_release_state_info(comp_state_info_t* cs_head)
{

}

ua_state_t comp_get_update_stage(comp_state_info_t* cs_head, char* pkg_name)
{
	ua_state_t st = UA_STATE_UNKNOWN;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(cs_head, pkg_name, cs);
	if (cs) {
		st = cs->stage;
	}

	return st;
}

int comp_set_update_stage(comp_state_info_t* cs_head, char* pkg_name, ua_state_t stage)
{
	int rc = E_UA_OK;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(cs_head, pkg_name, cs);
	if (cs) {
		cs->stage = stage;
	} else {
		cs = (comp_state_info_t*)malloc(sizeof(comp_state_info_t));
		memset(cs, 0, sizeof(comp_state_info_t));
		cs->pkg_name = f_strdup(pkg_name);
		cs->stage = stage;
		HASH_ADD_KEYPTR( hh, cs_head, cs->pkg_name , strlen(cs->pkg_name ), cs );
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

	return ret;
}

int comp_set_rb_type(comp_state_info_t* cs_head, char* pkg_name, update_rollback_t rb_type)
{
	int rc = E_UA_OK;
	comp_state_info_t* cs = NULL;

	HASH_FIND_STR(cs_head, pkg_name, cs);
	if (cs) {
		cs->rb_type = rb_type;
	} else {
		cs = (comp_state_info_t*)malloc(sizeof(comp_state_info_t));
		memset(cs, 0, sizeof(comp_state_info_t));
		cs->pkg_name = f_strdup(pkg_name);
		cs->rb_type = rb_type;
		HASH_ADD_KEYPTR( hh, cs_head, cs->pkg_name , strlen(cs->pkg_name ), cs );
	}

	return rc;
}
