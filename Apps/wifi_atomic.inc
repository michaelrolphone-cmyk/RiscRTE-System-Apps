#ifdef PORTABLE_SETTINGS_AVAILABILITY
/* Status grants do not fence recovery. Namespace-6 data ownership is retained
 * through RAM editing and is released only at a clean reload or explicit exit. */
static bool wifi_storage_ready(void) {
 return wifi_availability_valid&&wifi_atomic_api&&portable_settings_availability_available(&wifi_availability)&&
        wifi_loaded_available&&wifi_loaded_generation==wifi_availability.state.generation;
}
static const char *wifi_storage_notice(void) {
 if(!wifi_availability_valid)return "Storage unavailable; temporary RAM draft";
 if(wifi_availability.state.availability==RISC_KEY_VALUE_RECONCILIATION_PENDING)
  return draft_dirty?"Recovery pending; keep or discard RAM draft":"Pending; clean Runtime restart may be needed";
 if(wifi_availability.state.availability==RISC_KEY_VALUE_CONFLICT)
  return "Storage conflict; sources preserved, Save refused";
 if(!portable_settings_availability_available(&wifi_availability))return "Storage unavailable; temporary RAM draft";
 if(!wifi_atomic_api)return "Storage unavailable; reload to retry grant";
 if(!wifi_storage_ready())return draft_dirty?"Storage changed; keep or discard RAM draft":"Storage changed; reload saved networks";
 return "Available locally; SD backup not confirmed";
}
static void wifi_storage_observe(void) {
 uint64_t generation=wifi_availability.state.generation;
 uint32_t availability=wifi_availability.state.availability;
 wifi_availability_valid=portable_settings_availability_refresh(&wifi_availability);
 if(!wifi_availability_valid&&wifi_availability.grant.api){PORTABLE_APP_RETAIN();dirty=true;return;}
 if(generation!=wifi_availability.state.generation||availability!=wifi_availability.state.availability){
  wifi_message=wifi_storage_notice();dirty=true;
 }
}
static bool wifi_storage_acquire(void) {
 if(wk.api)return wifi_atomic_api!=NULL;
 wk=(risc_runtime_capability_v1){.struct_size=sizeof(wk)};
 if(!wr->acquire(RISC_KEY_VALUE_ATOMIC_CAPABILITY,1,PORTABLE_WIFI_STORAGE_INSTANCE,&wk)){
  if(wk.struct_size!=sizeof(wk)||wk.api||wk.slot||wk.generation)PORTABLE_APP_RETAIN();
  return false;
 }
 const risc_key_value_atomic_v1 *api=wk.api;
 if(wk.struct_size!=sizeof(wk)||!wk.slot||!wk.generation||!portable_wifi_atomic_api_valid(api)){
  PORTABLE_APP_RETAIN();return false;
 }
 wifi_atomic_api=api;
 wifi_atomic_read_view=(risc_key_value_v1){1,sizeof(wifi_atomic_read_view),NULL,wifi_atomic_current_get,wifi_atomic_deny_put};
 wifi_store=&wifi_atomic_read_view;
 wifi_storage_observe();if(PORTABLE_APP_RETAINED())return false;
 wifi_loaded_available=portable_settings_availability_available(&wifi_availability);
 wifi_loaded_generation=wifi_availability.state.generation;
 return true;
}
static bool wifi_storage_clean(void) {
 return !draft_dirty&&!profile_work&&!remember_join&&!remember_pending&&!forget_pending&&
        !saved_open_pending&&!saved_select_pending&&!saved_loading_active&&
        !portable_wifi_atomic_pending(&wifi_atomic_edit)&&!wifi_atomic_connect_ready&&
        !cleanup_pending&&!scanning&&!scan_owned&&!joining&&link_state==WIFI_LINK_DOWN&&
        page==WP_ROOT
#ifdef PORTABLE_TEXT_INPUT_CLIENT
        &&!wifi_text_active&&!wifi_text_closing&&!wifi_home_return
#endif
#ifdef WIFI_ASYNC_V1_SIZE
        &&!wifi_operation&&!wifi_next_request.kind&&!wifi_select_pending
#endif
        ;
}
static void wifi_storage_discard(void) {
 /* Invoked only by a labeled user choice, never a generation notification. */
 wifi_atomic_discard();portable_wifi_credentials_clear(&credentials);
 portable_wifi_credentials_clear(&profile_draft);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));
 draft_dirty=false;open_network=false;saved_state=PORTABLE_WIFI_CREDENTIALS_EMPTY;
}
static void wifi_storage_reload(void) {
 if(PORTABLE_APP_RETAINED())return;
 if(!wifi_storage_clean()){wifi_message="Disconnect and finish editing before reload";dirty=true;return;}
 if(wk.api&&!wr->release(&wk)){PORTABLE_APP_RETAIN();wifi_message="Storage release failed; draft retained";dirty=true;return;}
 memset(&wk,0,sizeof(wk));wifi_atomic_api=NULL;wifi_store=NULL;
 wifi_atomic_discard();wifi_loaded_available=false;
 /* The owner checkpoint, not app-side source selection, may publish here. */
 wr->yield_ms(1);if(PORTABLE_APP_RETAINED())return;
 if(!wifi_availability.grant.api&&!wifi_availability.grant.slot&&!wifi_availability.grant.generation){
  wifi_availability_valid=portable_settings_availability_open(&wifi_availability,wr,PORTABLE_WIFI_STORAGE_INSTANCE);
  if(!wifi_availability_valid&&(wifi_availability.grant.struct_size!=sizeof(wifi_availability.grant)||wifi_availability.grant.api||wifi_availability.grant.slot||wifi_availability.grant.generation)){PORTABLE_APP_RETAIN();return;}
 }
 wifi_storage_observe();if(PORTABLE_APP_RETAINED())return;
 wifi_boundary_generation=wifi_availability.state.generation;
 (void)wifi_storage_acquire();if(PORTABLE_APP_RETAINED())return;
 wifi_initial_profile_pending=false;
 if(wifi_storage_ready()){
  portable_wifi_credentials loaded={0};int state=portable_wifi_credentials_load(wifi_store,&loaded);
  if(PORTABLE_APP_RETAINED()){portable_wifi_credentials_clear(&loaded);return;}
  saved_state=state;wifi_initial_profile_pending=state==PORTABLE_WIFI_CREDENTIALS_AGAIN;
  /* This boundary had no draft. Do not keep an old clean SSID that would
   * suppress the deferred loader when the new authoritative view is Busy. */
  credentials=loaded;open_network=state==PORTABLE_WIFI_CREDENTIALS_LOADED&&!credentials.password[0];
  portable_wifi_credentials_clear(&loaded);
 }
 wifi_initial_retry_at=wa->millis()+250u;
 wifi_message=!wifi_storage_ready()?wifi_storage_notice():
  saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED?"Saved draft reloaded; Connect when ready":
  saved_state==PORTABLE_WIFI_CREDENTIALS_EMPTY?"No saved network; enter a temporary draft":
  saved_state==PORTABLE_WIFI_CREDENTIALS_AGAIN?"Saved networks still loading":"Saved data unreadable; sources preserved";
 dirty=true;
}
static void wifi_storage_step(void) {
 if(!opened||PORTABLE_APP_RETAINED()||cleanup_pending||(int32_t)(wa->millis()-wifi_availability_retry_at)<0)return;
 wifi_availability_retry_at=wa->millis()+250u;wifi_storage_observe();
 if(PORTABLE_APP_RETAINED()||!wifi_availability_valid)return;
 if(wifi_storage_clean()&&wifi_boundary_generation!=wifi_availability.state.generation&&
    (wifi_availability.state.availability==RISC_KEY_VALUE_RECONCILIATION_PENDING||
     (portable_settings_availability_available(&wifi_availability)&&!wifi_storage_ready())))wifi_storage_reload();
}
#endif
/* Explicit X4 experimental binding only. Watch and ordinary builds never
 * acquire this capability. The old KV1 writer is never a fallback. */
static int32_t wifi_atomic_current_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n) {
 (void)c;if(n)*n=0;
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!wifi_storage_ready())return RISC_KEY_VALUE_NOT_FOUND;
#endif
 if(!wifi_atomic_api)return RISC_KEY_VALUE_IO;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 int rc=wifi_atomic_api->state(wifi_atomic_api->context,&state);
 if(!rc&&(state.struct_size<sizeof(state)||state.namespace_id!=PORTABLE_WIFI_STORAGE_INSTANCE))return RISC_KEY_VALUE_IO;
 if(!rc)rc=wifi_atomic_api->get(wifi_atomic_api->context,state.revision,k,out,cap,n);
 return rc==RISC_ATOMIC_OK?RISC_KEY_VALUE_OK:rc==RISC_ATOMIC_NOT_FOUND?RISC_KEY_VALUE_NOT_FOUND:
        rc==RISC_ATOMIC_BUSY||rc==RISC_ATOMIC_STALE?RISC_KEY_VALUE_BUSY:rc==RISC_ATOMIC_BUFFER_SMALL?RISC_KEY_VALUE_BUFFER_SMALL:RISC_KEY_VALUE_IO;
}
static int32_t wifi_atomic_deny_put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;return RISC_KEY_VALUE_CONTEXT;}
static const char*wifi_atomic_message(int rc) {
 return rc==RISC_ATOMIC_CONFLICT?"Saved-network conflict; nothing changed":rc==RISC_ATOMIC_STALE?"Saved networks changed; retry explicitly":
        rc==RISC_ATOMIC_CANCELLED?"Network save cancelled":rc==RISC_ATOMIC_CAPACITY?"Native storage full; nothing changed":
        rc==RISC_ATOMIC_UNCERTAIN?"Save uncertain; reload before retry":"Atomic storage unavailable; retry";
}
static void wifi_atomic_discard(void) {
 if(portable_wifi_atomic_pending(&wifi_atomic_edit))return;
 portable_wifi_atomic_clear(&wifi_atomic_edit);wifi_atomic_connect_ready=false;forget_index_valid=false;
}
static void wifi_atomic_profile_start(unsigned work,const portable_wifi_credentials*value) {
 if(profile_work||portable_wifi_atomic_pending(&wifi_atomic_edit))return;
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 wifi_storage_observe();
 if(!wifi_storage_ready()){wifi_message=wifi_storage_notice();dirty=true;return;}
#endif
 if(remember_join||remember_pending){wifi_message="Connected-network save pending";dirty=true;return;}
 wifi_atomic_discard();profile_draft=*value;
 int rc=portable_wifi_atomic_begin(&wifi_atomic_edit,wifi_atomic_api,work==WIFI_PROFILE_FORGET?PORTABLE_WIFI_ATOMIC_FORGET:PORTABLE_WIFI_ATOMIC_SAVE,value);
 if(rc!=PORTABLE_WIFI_ATOMIC_AGAIN){portable_wifi_credentials_clear(&profile_draft);wifi_message=wifi_atomic_message(rc);dirty=true;return;}
 profile_work=work;wifi_message="Checking saved networks - Back cancels";dirty=true;
}
static void wifi_atomic_complete(unsigned work,int rc) {
 profile_work=0;wifi_atomic_connect_ready=false;
 if(rc==RISC_ATOMIC_OK){
  if(work==WIFI_PROFILE_ATOMIC_FORGET){saved_state=PORTABLE_WIFI_CREDENTIALS_EMPTY;draft_dirty=false;portable_wifi_credentials_clear(&credentials);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));open_network=false;forget_index_valid=false;wifi_message="Forgotten; old storage bytes may remain";wifi_page(WP_ROOT);}
  else {saved_state=PORTABLE_WIFI_CREDENTIALS_SAVED;if(!memcmp(&credentials,&profile_draft,sizeof(credentials)))draft_dirty=false;wifi_message="Saved locally; SD backup not confirmed";}
 }else {saved_state=PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED;wifi_message=wifi_atomic_message(rc);}
 portable_wifi_credentials_clear(&profile_draft);dirty=true;
 if(rc==RISC_ATOMIC_RETAINED){PORTABLE_APP_RETAIN();cleanup_pending=true;}
}
static bool wifi_atomic_step_work(void) {
 if(remember_pending&&!profile_work){
  remember_pending=false;portable_wifi_credentials_clear(&joined_profile);
  if(!wifi_atomic_connect_ready||wifi_atomic_edit.status!=PORTABLE_WIFI_ATOMIC_READY){wifi_message="Connected; use Save to store network";dirty=true;return true;}
  profile_work=WIFI_PROFILE_SAVE;wifi_atomic_connect_ready=false;
 }
 unsigned work=profile_work;
 if(work!=WIFI_PROFILE_SAVE&&work!=WIFI_PROFILE_FORGET&&work!=WIFI_PROFILE_ATOMIC_FORGET&&work!=WIFI_PROFILE_ATOMIC_CONNECT)return false;
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 /* Accepted work must still be collected. Unsubmitted work cannot become an
  * implicit save when unavailable storage later becomes authoritative. */
 wifi_storage_observe();
 if(PORTABLE_APP_RETAINED()||!wifi_availability_valid)return true;
 if(!portable_wifi_atomic_pending(&wifi_atomic_edit)&&!wifi_storage_ready()){
  wifi_atomic_discard();profile_work=0;portable_wifi_credentials_clear(&profile_draft);
  wifi_message=wifi_storage_notice();dirty=true;return true;
 }
#endif
 int rc=portable_wifi_atomic_step(&wifi_atomic_edit);
 if(rc==PORTABLE_WIFI_ATOMIC_AGAIN)return true;
 if(rc==PORTABLE_WIFI_ATOMIC_READY){
  if(work==WIFI_PROFILE_FORGET){profile_work=0;forget_index_valid=true;forget_index=wifi_atomic_edit.slot;wifi_page(WP_FORGET);return true;}
  if(work==WIFI_PROFILE_ATOMIC_CONNECT){profile_work=0;wifi_atomic_connect_ready=true;wifi_connect_now();return true;}
  rc=portable_wifi_atomic_submit(&wifi_atomic_edit);
  if(rc==PORTABLE_WIFI_ATOMIC_AGAIN||rc==PORTABLE_WIFI_ATOMIC_READY)return true;
 }
 wifi_atomic_complete(work,rc);return true;
}
/* Return true while Back has a local operation to cancel/drain. Cancellation
 * after submission never destroys the token or a committed success receipt. */
static bool wifi_atomic_back(void) {
 if(portable_wifi_atomic_pending(&wifi_atomic_edit)){
  int rc=portable_wifi_atomic_cancel(&wifi_atomic_edit);
  if(rc!=PORTABLE_WIFI_ATOMIC_AGAIN)wifi_atomic_complete(profile_work,rc);
  else {wifi_message="Finishing native save cancellation";dirty=true;}
  return true;
 }
 if(profile_work==WIFI_PROFILE_SAVE||profile_work==WIFI_PROFILE_FORGET||profile_work==WIFI_PROFILE_ATOMIC_CONNECT||profile_work==WIFI_PROFILE_ATOMIC_FORGET){
  wifi_atomic_discard();profile_work=0;portable_wifi_credentials_clear(&profile_draft);wifi_message="Saved-network action cancelled";dirty=true;return true;
 }
 if(remember_pending){remember_pending=false;portable_wifi_credentials_clear(&joined_profile);wifi_atomic_discard();portable_wifi_credentials_clear(&profile_draft);wifi_message="Network save cancelled; use Save to retry";dirty=true;return true;}
 if(wifi_atomic_edit.status==PORTABLE_WIFI_ATOMIC_READY&&!remember_join){wifi_atomic_discard();portable_wifi_credentials_clear(&profile_draft);}
 return false;
}
