/* Copyright(c) 2019 Excelfore Corporation, - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 * Its use or disclosure, in whole or in part, without
 * written permission of Excelfore Corp. is prohibited.
 *
 * updater.c
 *
 */

#include "updater.h"
#include "utils.h"
#include "xml.h"
#include "utlist.h"
#include "pthread.h"
#include "debug.h"
#include "component.h"

extern ua_internal_t ua_intl;
json_object* update_get_pkg_info_jo(pkg_info_t* pkg)
{
	json_object* jo_pkg = json_object_new_object();

	json_object_object_add(jo_pkg, "type", json_object_new_string(NULL_STR(pkg->type)));
	json_object_object_add(jo_pkg, "name", json_object_new_string(NULL_STR(pkg->name)));
	json_object_object_add(jo_pkg, "version", json_object_new_string(NULL_STR(pkg->version)));
	if (pkg->rollback_version != NULL)
		json_object_object_add(jo_pkg, "rollback-version", json_object_new_string(NULL_STR(pkg->rollback_version)));
	if (pkg->rollback_versions != NULL)
		json_object_object_add(jo_pkg, "rollback-versions", json_object_get(pkg->rollback_versions));

	return jo_pkg;
}

int update_set_pkg_info(json_object* jo_pkg, pkg_info_t* pkg)
{
	int err = E_UA_ERR;

	if (jo_pkg && pkg) {
		err = json_get_property(jo_pkg, json_type_string, &pkg->type, "package", "type", NULL);

		if (err == E_UA_OK)
			err = json_get_property(jo_pkg, json_type_string, &pkg->name, "package", "name", NULL);

		if (err == E_UA_OK)
			err = json_get_property(jo_pkg, json_type_string, &pkg->version, "package", "version", NULL);

		if (err == E_UA_OK)
			json_get_property(jo_pkg, json_type_string, &pkg->rollback_version, "package", "rollback-version", NULL);

		if (err == E_UA_OK)
			json_get_property(jo_pkg, json_type_object, &pkg->rollback_versions, "package", "rollback-versions", NULL);

	}

	return err;

}

json_object* update_get_update_file_info_jo(pkg_file_t* update_inf)
{
	json_object* jo_update_inf = json_object_new_object();

	json_object_object_add(jo_update_inf, "file", json_object_new_string(NULL_STR(update_inf->file)));
	json_object_object_add(jo_update_inf, "version", json_object_new_string(NULL_STR(update_inf->version)));
	json_object_object_add(jo_update_inf, "sha256", json_object_new_string(NULL_STR(update_inf->sha256b64)));
	json_object_object_add(jo_update_inf, "downloaded", json_object_new_int(update_inf->downloaded));
	json_object_object_add(jo_update_inf, "rollback-order", json_object_new_int(update_inf->rollback_order));

	return jo_update_inf;
}

int update_set_update_file_info(json_object* jo_update_inf, pkg_file_t* update_inf)
{
	int err                = E_UA_OK;
	char* update_file_prop = "update-file-info";

	if (jo_update_inf && update_inf) {
		err = json_get_property(jo_update_inf, json_type_string, &update_inf->file, update_file_prop, "file", NULL);
		if (err == E_UA_OK)
			err = json_get_property(jo_update_inf, json_type_string, &update_inf->version, update_file_prop, "version", NULL);
		if (err == E_UA_OK)
			err = json_get_property(jo_update_inf, json_type_string, &update_inf->sha256b64, update_file_prop, "sha256", NULL);
		if (err == E_UA_OK)
			err = json_get_property(jo_update_inf, json_type_int, &update_inf->downloaded, update_file_prop, "downloaded", NULL);
		if (err == E_UA_OK)
			err = json_get_property(jo_update_inf, json_type_int, &update_inf->rollback_order, update_file_prop, "rollback-order", NULL);

	}

	return err;
}

json_object* update_get_comp_context_jo(ua_component_context_t* uacc)
{
	json_object* update_cc = json_object_new_object();

	if (uacc) {
		json_object_object_add(update_cc, "package", update_get_pkg_info_jo(&uacc->update_pkg));
		json_object_object_add(update_cc, "update-file-info", update_get_update_file_info_jo(&uacc->update_file_info));

		update_rollback_t rb_type = comp_get_rb_type(uacc->st_info, uacc->update_pkg.name);
		json_object_object_add(update_cc, "rb-type", json_object_new_int(rb_type));

		ua_stage_t state= comp_get_update_stage(uacc->st_info, uacc->update_pkg.name);
		json_object_object_add(update_cc, "update-state", json_object_new_int(state));
		json_object_object_add(update_cc, "update-error", json_object_new_int(uacc->update_error));

	} else {
		json_object_put(update_cc);

	}

	return update_cc;
}

int update_set_comp_context(ua_component_context_t* uacc, json_object* update_cc)
{
	int err = E_UA_OK;

	err = update_set_update_file_info(update_cc, &uacc->update_file_info);
	err = update_set_pkg_info(update_cc, &uacc->update_pkg);

	update_rollback_t rb_type = URB_NONE;
	err = json_get_property(update_cc, json_type_int, &rb_type, "rb-type", NULL);
	comp_set_rb_type(&uacc->st_info, uacc->update_pkg.name, rb_type);

	ua_stage_t state = UA_STATE_UNKNOWN;
	err = json_get_property(update_cc, json_type_int, &state, "state", NULL);
	comp_set_update_stage(&uacc->st_info, uacc->update_pkg.name, state);

	err = json_get_property(update_cc, json_type_int, &uacc->update_error, "update-error", NULL);

	return err;

}

int update_record_save(ua_component_context_t* uacc)
{
	int err  = E_UA_OK;
	FILE* fd = NULL;

	if (uacc && uacc->record_file && (fd = fopen(uacc->record_file, "w"))) {
		json_object* jo_rec = update_get_comp_context_jo(uacc);
		char* str_rec       = (char*)json_object_to_json_string(jo_rec);
		if (str_rec != NULL) {
			fwrite(str_rec, 1, strlen(str_rec), fd);

		}
		fclose(fd);
		json_object_put(jo_rec);

	}else{
		A_ERROR_MSG("Error save update record %s", NULL_STR(uacc->record_file));
		err = E_UA_ERR;
	}

	return err;
}

char* update_record_load(char* record_file)
{
	int len       = 0;
	FILE* fd      = NULL;
	char* jstring = NULL;

	if (record_file && (fd = fopen(record_file, "r"))) {
		fseek(fd, 0L, SEEK_END);
		len = ftell(fd);
		rewind(fd);

		jstring = (char*)malloc(len+1);
		if (jstring) {
			if (fread(jstring, 1, len, fd) != len) {
				free(jstring);
				jstring = NULL;
			}
		}

		fclose(fd);

	}else{
		A_INFO_MSG("Could not open update record file %s", NULL_STR(record_file));

	}

	return jstring;
}

int update_installed_version_same(ua_component_context_t* uacc, char* target_version)
{
	char* install_version = NULL;

	if (E_UA_OK != (uacc->uar->on_get_version)(uacc->update_pkg.type,
	                                           uacc->update_pkg.name,
	                                           &install_version))
		A_ERROR_MSG("Error get version for %s.", uacc->update_pkg.name);

	return (S(install_version) && !strcmp(target_version, install_version));

}

install_state_t update_start_install_operations(ua_component_context_t* uacc, int reboot_support)
{
	install_state_t update_sts;

	A_INFO_MSG("Start installation of %s for version %s.", uacc->update_pkg.name, uacc->update_file_info.version);
	if (!uacc->update_pkg.rollback_version && update_installed_version_same(uacc, uacc->update_pkg.version)) {
		A_INFO_MSG("Found installed version is same as requested target version.");
		send_install_status(uacc, INSTALL_COMPLETED, 0, UE_NONE);
		update_sts = INSTALL_COMPLETED;
	}else {
		if (reboot_support)
			update_record_save(uacc);

		update_sts = pre_update_action(uacc);

		if (update_sts == INSTALL_IN_PROGRESS)
			update_sts = update_action(uacc);

		if (update_sts == INSTALL_COMPLETED)
			post_update_action(uacc);

		if (reboot_support)
			remove(uacc->record_file);
	}
	if (update_sts == INSTALL_COMPLETED)
		A_INFO_MSG("Installation of %s for version %s has completed successfully.", uacc->update_pkg.name, uacc->update_file_info.version);
	else
		A_INFO_MSG("Installation of %s for version %s did not succeed.", uacc->update_pkg.name, uacc->update_file_info.version);
	return update_sts;
}

void update_set_rollback_info(ua_component_context_t* uacc)
{
	if (uacc) {
		update_rollback_t rb_type = URB_NONE;

		if (uacc->update_pkg.rollback_version) {
			rb_type = URB_DMC_INITIATED;

			char* fake_rb_ver = comp_get_fake_rb_version(uacc->st_info, uacc->update_pkg.name);
			if (fake_rb_ver && !strcmp(fake_rb_ver, uacc->update_pkg.rollback_version)) {
				rb_type = URB_UA_INITIATED;
			}

		}

		json_object* ua_ctrl_rb_versions = NULL;
		pkg_file_t* pf                   = NULL, * aux = NULL, * pkg_file = NULL;
		if (uacc->backup_manifest && !parse_pkg_manifest(uacc->backup_manifest, &pkg_file)) {
			ua_ctrl_rb_versions = json_object_new_array();
			DL_FOREACH_SAFE(pkg_file, pf, aux) {
				json_object_array_add(ua_ctrl_rb_versions, json_object_new_string(pf->version));

				if (uacc->update_pkg.rollback_version) {
					if (!strcmp(uacc->update_pkg.rollback_version, pf->version))
						rb_type = URB_UA_INITIATED;
				}

				free_pkg_file(pf);
			}

			if (!uacc->update_pkg.rollback_version &&
			    !uacc->update_pkg.rollback_versions &&
			    json_object_array_length(ua_ctrl_rb_versions) > 0) {
				A_INFO_MSG("Found local backup available for rollback");
				rb_type                            = URB_UA_LOCAL_BACKUP;
				uacc->update_pkg.rollback_versions = ua_ctrl_rb_versions;
			}else {
				A_INFO_MSG("No local backup available for rollback");
				json_object_put(ua_ctrl_rb_versions);
			}
		}

		comp_set_rb_type(&uacc->st_info, uacc->update_pkg.name, rb_type);
	}
}

char* update_get_next_rollback_version(ua_component_context_t* uacc, char* cur_version)
{
	char* rb_version = NULL;

	if (uacc->update_pkg.rollback_versions != NULL)
		get_pkg_next_rollback_version(uacc->update_pkg.rollback_versions,
		                              cur_version, &rb_version);

	return rb_version;
}

static int update_package_available(pkg_file_t* update_file, char* version)
{
	int avail = 0;

	if (update_file && version) {
		avail = (!strcmp(update_file->version, version) &&
		         !access(update_file->file, F_OK));
	}
	return avail;
}

static int update_get_rollback_package(ua_component_context_t* uacc, pkg_file_t* rb_file_info, char* rb_version)
{
	int rc = E_UA_ERR;

	if (rb_file_info && rb_version) {
		char* filepath = 0;
		if (!get_pkg_downloaded_from_json(uacc->cur_msg, rb_version, &rb_file_info->downloaded)
		    && rb_file_info->downloaded
		    && !get_pkg_sha256_from_json(uacc->cur_msg, rb_version, rb_file_info->sha256b64)
		    && !get_pkg_file_from_json(uacc->cur_msg, rb_version, &filepath)) {
			A_INFO_MSG("RB info available from ready-update package.");

			rb_file_info->file    = f_strdup(filepath);
			rb_file_info->version = f_strdup(rb_version);
			if (update_package_available(rb_file_info, rb_version))
				rc = E_UA_OK;
			else {
				free(rb_file_info->file);
				free(rb_file_info->version);
			}
		} else {
			A_INFO_MSG("Getting RB info from backup manifest.");
			if (E_UA_OK == get_pkg_file_manifest(uacc->backup_manifest, rb_version, rb_file_info)) {
				if (update_package_available(rb_file_info, rb_version))
					rc = E_UA_OK;
				else {
					free(rb_file_info->file);
					free(rb_file_info->version);
				}
			}
		}
	}

	return rc;
}

int update_send_rollback_status(ua_component_context_t* uacc, char* next_rb_version)
{
	pkg_file_t tmp_rb_file_info = {0};

	uacc->update_pkg.rollback_version = next_rb_version;

	if (update_get_rollback_package(uacc, &tmp_rb_file_info, next_rb_version) != E_UA_OK) {
		tmp_rb_file_info.downloaded = 0;
		tmp_rb_file_info.version    = next_rb_version;
	}

	send_install_status(uacc, INSTALL_ROLLBACK, &tmp_rb_file_info, UE_NONE);

	if (tmp_rb_file_info.file) {
		Z_FREE(tmp_rb_file_info.file);
		Z_FREE(tmp_rb_file_info.version);
	}

	return 0;

}

install_state_t update_start_rollback_operations(ua_component_context_t* uacc, char* rb_version, int reboot_support)
{
	install_state_t update_sts  = INSTALL_FAILED;
	char* next_rb_version       = rb_version;
	char* tmp_cur_version       = NULL;
	pkg_file_t tmp_rb_file_info = {0};

	if (uacc && rb_version ) {
		A_INFO_MSG("Starting rollback type(%d) to version(%s)", comp_get_rb_type(uacc->st_info, uacc->update_pkg.name), next_rb_version);
		Z_FREE(uacc->update_file_info.version);
		Z_FREE(uacc->update_file_info.file);
		uacc->update_pkg.rollback_version = next_rb_version;

		if (update_installed_version_same(uacc, next_rb_version)) {
			uacc->update_file_info.version = f_strdup(next_rb_version);
			uacc->update_file_info.file    = NULL;
			send_install_status(uacc, INSTALL_ROLLBACK, &uacc->update_file_info, UE_NONE);
			A_INFO_MSG("Found installed version is same as requested rollback version.");
			update_sts = INSTALL_COMPLETED;
			send_install_status(uacc, INSTALL_COMPLETED, 0, UE_NONE);

		}else {
			if (update_get_rollback_package(uacc, &tmp_rb_file_info, next_rb_version) == E_UA_OK) {
				A_INFO_MSG("Rollback package found, rollback installation starts now.");

				update_sts = prepare_install_action(uacc, &tmp_rb_file_info, 0,
				                                    &uacc->update_file_info, &uacc->update_error);

				if (is_prepared_delta_package(uacc->update_file_info.file)) {
					//E115-417: No rollback is available when standalone delta is used.
					A_INFO_MSG("package %s is for preprared delta, rollback is not avaialbe, returning INSTALL_FAILED", uacc->update_file_info.file);
					update_sts = INSTALL_FAILED;

				} else {
					if (update_sts == INSTALL_READY) {
						uacc->update_pkg.rollback_version = uacc->update_file_info.version;
						update_sts                        = update_start_install_operations(uacc, reboot_support);
					}

					if (update_sts != INSTALL_COMPLETED) {
						A_INFO_MSG("Rollback to version(%s) was not successful", next_rb_version);
						tmp_cur_version = next_rb_version;
						next_rb_version = update_get_next_rollback_version(uacc, tmp_cur_version);
						if (next_rb_version) {
							A_INFO_MSG("Rollback will try the next version(%s).", next_rb_version);
							update_sts = INSTALL_ROLLBACK;
							update_send_rollback_status(uacc, next_rb_version);

						}
						else
							A_INFO_MSG("No more rollback version.");
					}
				}

				Z_FREE(tmp_rb_file_info.version);
				Z_FREE(tmp_rb_file_info.file);

			} else {
				if ( comp_get_rb_type(uacc->st_info, uacc->update_pkg.name) == URB_DMC_INITIATED ) {
					A_INFO_MSG("Rollback package file is not available locally, asking eSync client to download it.");
					update_send_rollback_status(uacc, next_rb_version);
					tmp_cur_version = next_rb_version;
					next_rb_version = NULL;
					update_sts      = INSTALL_ROLLBACK;

				} else {
					A_INFO_MSG("Rollback package file cannot be located for %s, version: %s", uacc->update_pkg.name, next_rb_version);
					tmp_cur_version = next_rb_version;
					next_rb_version = update_get_next_rollback_version(uacc, tmp_cur_version);
					if (next_rb_version) {
						A_INFO_MSG("Rollback will try the next version(%s).", next_rb_version);
						update_sts = INSTALL_ROLLBACK;
						update_send_rollback_status(uacc, next_rb_version);

					}else {
						A_INFO_MSG("No more rollback version for %s, signal INSTALL_FAILED", uacc->update_pkg.name);
						update_sts = INSTALL_FAILED;

					}
				}
			}
		}

		if (update_sts != INSTALL_COMPLETED && !access(uacc->update_manifest, F_OK)) {
			A_INFO_MSG("Removing temp manifest, RB version was %s.", tmp_cur_version);
			remove(uacc->update_manifest);
		}

	};

	if (update_sts == INSTALL_COMPLETED) {
		A_INFO_MSG("Rollback to version %s has succeeded.", uacc->update_file_info.version);

	} else if (update_sts == INSTALL_FAILED) {
		A_INFO_MSG("Rollback has exhausted all available versions, informing terminal-failure.");
		uacc->update_error = UE_TERMINAL_FAILURE;
		send_install_status(uacc, INSTALL_FAILED, &uacc->update_file_info, uacc->update_error);

	}

	return update_sts;
}

void* update_resume_from_reboot(void* arg)
{
	thread_resume_t* t_arg       = (thread_resume_t*)arg;
	ua_component_context_t* uacc = t_arg->uacc;

	if (uacc) {
		uacc->worker.worker_running = 1;
		if (update_installed_version_same(uacc, uacc->update_file_info.version)) {
			A_INFO_MSG("Resume: update installation was successful.");
			post_update_action(uacc);
			send_install_status(uacc, INSTALL_COMPLETED, 0, UE_NONE);

		}else {
			if (comp_get_rb_type(uacc->st_info, uacc->update_pkg.name) != URB_NONE) {
				A_INFO_MSG("Resume: update installation failed, continue next rollback action.");
				char* rb_version = update_get_next_rollback_version(uacc, uacc->update_file_info.version);
				if (rb_version != NULL)
					update_start_rollback_operations(uacc, rb_version, 1);
				else
					send_install_status(uacc, INSTALL_FAILED,
					                    &uacc->update_file_info, UE_TERMINAL_FAILURE);

			}else {
				A_ERROR_MSG("Resume: update installation has failed, informing eSync client.");
				send_install_status(uacc, INSTALL_FAILED, 0, UE_NONE);
			}
		}

		remove(uacc->record_file);
		json_object_put(t_arg->jo_update_rec);
		Z_FREE(t_arg);
		A_INFO_MSG("Resume operations after reboot have completed");
		handler_set_internal_state(UAI_STATE_RESUME_DONE);
		uacc->worker.worker_running = 0;

	}

	return 0;
}

char* update_record_get_type(json_object* jo_cc)
{
	char* type            = NULL;
	pkg_info_t update_pkg = {0};

	if (E_UA_OK == update_set_pkg_info(jo_cc, &update_pkg)) {
		A_INFO_MSG("Resume record for type %s.", update_pkg.type);
		type = f_strdup(update_pkg.type);
	}

	return type;
}

void update_handle_resume_from_reboot(char* rec_file, runner_info_hash_tree_t* ri_tree)
{
	ua_component_context_t* uacc = NULL;
	char* rec_string             = update_record_load(rec_file);
	char* type                   = NULL;
	UT_array ri_list;
	int err                    = E_UA_OK;
	json_object* jo_update_rec = NULL;

	handler_set_internal_state(UAI_STATE_RESUME_STARTED);
	if (rec_string != NULL) {
		A_INFO_MSG("Starting resume operations after reboot.");
		enum json_tokener_error jerr;
		jo_update_rec = json_tokener_parse_verbose(rec_string, &jerr);
		type          = update_record_get_type(jo_update_rec);
		if (type) {
			utarray_init(&ri_list, &ut_ptr_icd);
			query_hash_tree(ri_tree, 0, type, 0, &ri_list, 0);
			if (utarray_len(&ri_list) > 0) {
				/* Resume record should have no more than 1 entry */
				runner_info_t* ri = *(runner_info_t**) utarray_eltptr(&ri_list, 0);
				uacc = &ri->component;
				err  = update_set_comp_context(uacc, jo_update_rec);
				if (uacc && err == E_UA_OK) {
					pthread_t thread_resume;
					thread_resume_t* thread_arg = (thread_resume_t*)malloc(sizeof(thread_resume_t));
					thread_arg->jo_update_rec = jo_update_rec;
					thread_arg->uacc          = uacc;
					if (pthread_create(&thread_resume, 0, update_resume_from_reboot, thread_arg)) {
						A_ERROR_MSG("Failed to spawn a resume thread.");
						json_object_put(jo_update_rec);
						Z_FREE(thread_arg);
						err = E_UA_ERR;
					}
				} else
				{
					A_ERROR_MSG("Error setting component context for resume.");
				}


			}else {
				A_ERROR_MSG("Error: Could not find registered type of %s", type);
				err = E_UA_ERR;

			}
			utarray_done(&ri_list);
			free(type);

		}else {
			A_ERROR_MSG("Error getting type from resume record.");

		}
		free(rec_string);
		if (err == E_UA_ERR)
			A_ERROR_MSG("Resume operations after reboot have concluded with error, discarding resume record.");

	}else {
		err = E_UA_ERR;
	}

	if (err == E_UA_ERR) {
		if (rec_file)
			remove(rec_file);
		handler_set_internal_state(UAI_STATE_RESUME_DONE);
	}
}

int update_parse_json_ready_update(ua_component_context_t* uacc, json_object* jsonObj, char* cache_dir, char* backup_dir)
{
	int err = E_UA_OK;

	if (jsonObj && uacc) {
		if (get_pkg_type_from_json(jsonObj, &uacc->update_pkg.type))
			err = E_UA_ERR;

		if (err == E_UA_OK)
			err = get_pkg_name_from_json(jsonObj, &uacc->update_pkg.name);

		if (err == E_UA_OK) {
			uacc->update_manifest = JOIN(cache_dir, uacc->update_pkg.name, MANIFEST_PKG);
			uacc->backup_manifest = JOIN(backup_dir, "backup", uacc->update_pkg.name, MANIFEST_PKG);
			err                   = get_pkg_version_from_json(jsonObj, &uacc->update_pkg.version);
		}

		if (err == E_UA_OK) {
			get_pkg_rollback_version_from_json(jsonObj, &uacc->update_pkg.rollback_version);
			get_pkg_rollback_versions_from_json(jsonObj, &uacc->update_pkg.rollback_versions);

			char* version_to_update = S(uacc->update_pkg.rollback_version) ?
			                          uacc->update_pkg.rollback_version : uacc->update_pkg.version;

			if ((E_UA_OK != get_pkg_file_manifest(uacc->update_manifest, version_to_update, &uacc->update_file_info)))
			{
				A_ERROR_MSG("Could not load temp update manifest, getting info from json package object instead.");
				char* update_file_name = NULL;
				if (!get_pkg_downloaded_from_json(jsonObj, version_to_update, &uacc->update_file_info.downloaded)) {
					if (uacc->update_file_info.downloaded) {
						get_pkg_sha256_from_json(jsonObj, version_to_update, uacc->update_file_info.sha256b64);

						if (!get_pkg_file_from_json(jsonObj, version_to_update, &update_file_name)) {
							#ifdef SUPPORT_UA_DOWNLOAD
							if (ua_intl.ua_download_required) {
								char tmp_filename[PATH_MAX] = {0};
								snprintf(tmp_filename, PATH_MAX, "%s/%s/%s/%s.e",
								         ua_intl.ua_dl_dir,
								         uacc->update_pkg.name, uacc->update_file_info.version,
								         uacc->update_file_info.version);
								f_free(uacc->update_file_info.file);
								uacc->update_file_info.file = f_strdup(tmp_filename);
							} else {
								uacc->update_file_info.file = update_file_name ? f_strdup(update_file_name) : NULL;
							}
							#else
							uacc->update_file_info.file = update_file_name ? f_strdup(update_file_name) : NULL;
							#endif

							uacc->update_file_info.version = f_strdup(version_to_update);
							// If the firmware file indicated by json is not available
							if (access(uacc->update_file_info.file, F_OK) == -1) {
								char* new_file    = NULL;
								ua_routine_t* uar = uacc->uar;
								if (uar->on_transfer_file) {
									int ret = (*uar->on_transfer_file)(uacc->update_pkg.type,
									                                   uacc->update_pkg.name,
									                                   uacc->update_pkg.version,
									                                   uacc->update_file_info.file,
									                                   &new_file);
									if (ret) err = E_UA_ERR;
								}
								if (new_file) {
									f_free(uacc->update_file_info.file);
									uacc->update_file_info.file = new_file;
								}
							}

							if (!calc_sha256_x(uacc->update_file_info.file, uacc->update_file_info.sha_of_sha) && err == E_UA_OK) {
								add_pkg_file_manifest(uacc->update_manifest, &uacc->update_file_info);
								err = E_UA_OK;
							} else {
								A_ERROR_MSG("Error in calculating sha_of_sha.");
								err = E_UA_ERR;
							}
						} else {
							A_INFO_MSG("Getting filepath from backup manifest.");
							err = get_pkg_file_manifest(uacc->backup_manifest, version_to_update, &uacc->update_file_info);
						}
					}
				}
			}
		}

		if (err == E_UA_ERR) {
			Z_FREE(uacc->backup_manifest);
			Z_FREE(uacc->update_manifest);
		}
	}

	if (err == E_UA_ERR && S(uacc->update_pkg.rollback_version))
		uacc->update_error = UE_TERMINAL_FAILURE;

	return err;
}

void update_release_comp_context(ua_component_context_t* uacc)
{
	if (comp_get_rb_type(uacc->st_info, uacc->update_pkg.name) == URB_UA_LOCAL_BACKUP) {
		if (uacc->update_pkg.rollback_versions)
			json_object_put(uacc->update_pkg.rollback_versions);
	}

	Z_FREE(uacc->update_file_info.version);
	Z_FREE(uacc->update_file_info.file);

	uacc->update_pkg.name                 = NULL;
	uacc->update_pkg.version              = NULL;
	uacc->update_pkg.type                 = NULL;
	uacc->update_pkg.rollback_version     = NULL;
	uacc->update_pkg.rollback_versions    = NULL;
	uacc->update_file_info.rollback_order = 0;
	uacc->update_error                    = UE_NONE;
}
