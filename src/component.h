/* Copyright Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * component.h
 *
 */

#ifndef COMPONENT_H
#define COMPONENT_H

#include "handler.h"

ua_state_t comp_get_update_stage(comp_state_info_t* cs_head, char* pkg_name);
int comp_set_update_stage(comp_state_info_t** cs_head, char* pkg_name, ua_state_t stage);
void comp_release_state_info(comp_state_info_t* cs_head);
update_rollback_t comp_get_rb_type(comp_state_info_t* cs_head, char* pkg_name);
int comp_set_rb_type(comp_state_info_t** cs_head, char* pkg_name, update_rollback_t rb_type);

#endif //COMPONENT_H
