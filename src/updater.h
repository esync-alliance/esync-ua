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

typedef enum comp_update_state {
	CUS_NONE,
	CUS_DOWNLOAD_STARTED,
	CUS_DOWNLOAD_COMPLETD,
	CUS_PREPARE_UPDATE_STARTED,
	CUS_PREPARE_UPDATE_COMPLETED,
	CUS_READY_UPDATE_STARTED,
	CUS_READ_UPDATE_COMPLETED,
	CUS_BACKUP_STARTED,
	CUS_BACUP_COMPLETED,
	CUS_CONFIRM_UPDATE_STARTED,
	CUS_CONFIRM_UPDATE_COMPLETED,

}comp_update_state_t;

typedef struct thread_resume {
	ua_component_context_t* uacc;
	json_object* jo_update_rec;
	
}thread_resume_t;

void update_release_comp_context(ua_component_context_t* uacc);

int update_record_save(ua_component_context_t* uacc);

char* update_record_load(char* rec_file);

#endif //UPDATER_H_