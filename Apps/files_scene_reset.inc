/* Only after a previous invocation has proved complete cleanup, or initial
 * zero-initialized module entry. Never called to make live custody look clean. */
static void files_reset_domain(void){
#define FILES_CLEAR(value) memset(&(value),0,sizeof(value))
 FILES_CLEAR(fb_grant);FILES_CLEAR(fb_root_grants);FILES_CLEAR(fb_root_volumes);
 FILES_CLEAR(fb_handler_grant);FILES_CLEAR(fb_owned);fb_handler_api=NULL;fb_volume=NULL;
 fb_retained_file=0;fb_storage_index=fb_source_index=fb_list_choice=fb_action_page=0;
 fb_handler_offset=fb_handler_count=fb_notice_return=0;
 fb_manage=fb_terminal=fb_publication_unknown=fb_moving=false;
 FILES_CLEAR(fb_source_path);FILES_CLEAR(fb_source_base);FILES_CLEAR(fb_source_query);
 FILES_CLEAR(fb_handler_path);FILES_CLEAR(fb_handlers);
 FILES_CLEAR(fb_text);FILES_CLEAR(fb_text_state);FILES_CLEAR(fb_text_checkpoint);
 fb_text_mode=fb_text_return_mode=fb_text_return_choice=fb_text_storage=0;
 fb_text_active=fb_text_closing=fb_text_home=fb_text_invalidated=false;
 fb_text_policy_continuity=fb_text_policy_boundary=false;
 fpeer_mode=fbm_new_directory=fbm_mounted=fbm_applying=fbm_loading=false;
 FILES_CLEAR(fbm_local_path);FILES_CLEAR(fb_rows);FILES_CLEAR(fb_selected);
 fb_page_rows=FB_MAX_PAGE_ROWS;
 fb_count=fb_choice=fb_page=fb_mode=fb_preview_count=fb_detail_offset=fb_matched=0;
 fb_feedback=fb_hidden=fb_more=fb_hex=fb_loaded=false;
 fb_offset=fb_size=0;fb_io_pending=false;fb_dirty=true;
 FILES_CLEAR(fb_path);FILES_CLEAR(fb_file_path);FILES_CLEAR(fb_query);FILES_CLEAR(fb_editor);
 FILES_CLEAR(fb_status);FILES_CLEAR(fb_label);FILES_CLEAR(fb_preview);strcpy(fb_path,"/");
 fsource_reset();FILES_CLEAR(fsource_buffer);
 fsh_direct=true;fsh_private_started=false;fsh_direct_stage=0;FILES_CLEAR(fsh_offer);
 FILES_CLEAR(fsh_entropy_grant);FILES_CLEAR(fsh_ble_power_grant);
 fsh_ble_restore_off=fsh_ble_power_ready=false;fpeer=NULL;fsh_phase=FSH_OFF;fsh_radio_flags=0;
 fsh_network_initialized=fsh_back_pending=fsh_service_ending=false;
 FILES_CLEAR(fsh_store_grant);FILES_CLEAR(fsh_wifi_grant);FILES_CLEAR(fsh_network);
 fsh_client=NULL;FILES_CLEAR(fsh_status);FILES_CLEAR(fsh_link);fsh_now=0;
 fsh_last_clock=fsh_last_second=0;FILES_CLEAR(fsh_url);FILES_CLEAR(fsh_message);
 FILES_CLEAR(fsh_setup);FILES_CLEAR(fsh_setup_status);
 fsh_setup_initialized=fsh_setup_screen=fsh_setup_leaving=fsh_setup_stopping=fsh_setup_servicing=false;
 fsh_setup_polled=0;fsh_setup_display_generation=fsh_setup_display_number=0;
 fsh_setup_pending_generation=fsh_setup_pending_number=0;
 FILES_CLEAR(fpeer_status);fpeer_choice=0;fpeer_launch=false;fpeer_pair_generation=fpeer_pair_number=0;
 FILES_CLEAR(fpeer_local_folder);FILES_CLEAR(fpeer_upload_path);FILES_CLEAR(fpeer_upload_name);FILES_CLEAR(fpeer_launch_path);
 fbm_operation=FBM_IDLE;fbm_wait_generation=0;FILES_CLEAR(fbm_requested_path);FILES_CLEAR(fbm_replace_path);
 fbm_preview_offset=0;fbm_preview_hex=fbm_choose_handler=fbm_dispatch_pending=false;
 fbm_local_volume=NULL;FILES_CLEAR(fbm_local_grant);fbm_local_index=fbm_directory_cursor=0;
 FILES_CLEAR(files_scene_peer_rows);files_scene_peer_count=0;files_revision=files_settled_at=0;files_cosmetic_pending=files_settled_seen=false;
 FILES_CLEAR(files_candidate);files_scene_exit_requested=false;
 files_handler_pending=false;files_handler_index=0;FILES_CLEAR(files_handler_identity);
 FILES_CLEAR(files_handler_value);FILES_CLEAR(files_handler_path);
 files_presenter_reset();
#undef FILES_CLEAR
}
