/* Explicit X4 experimental binding only. Watch and ordinary builds never
 * acquire this capability. The old KV1 writer is never a fallback. */
static int32_t wifi_atomic_current_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n) {
 (void)c;if(n)*n=0;if(!wifi_atomic_api)return RISC_KEY_VALUE_IO;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 int rc=wifi_atomic_api->state(wifi_atomic_api->context,&state);
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
