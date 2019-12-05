/* Copyright(c) 2019 Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * updater.h
 *
 */

#ifndef UPDATER_H_
#define UPDATER_H_

#include "handler.h"

typedef struct thread_resume {
	ua_component_context_t* uacc;
	json_object* jo_update_rec;

}thread_resume_t;

void update_release_comp_context(ua_component_context_t* uacc);

int update_parse_json_ready_update(ua_component_context_t* uacc, json_object* jsonObj, char* cache_dir, char* backup_dir);

void update_set_rollback_info(ua_component_context_t* uacc);

install_state_t update_start_rollback_operations(ua_component_context_t* uacc, char* rb_version, int reboot_support);

install_state_t update_start_install_operations(ua_component_context_t* uacc, int reboot_support);

char* update_get_next_rollback_version(ua_component_context_t* uacc, char* cur_version);

void update_handle_resume_from_reboot(char* rec_file, runner_info_hash_tree_t* ri_tree);



#endif //UPDATER_H_