/* Shared Wi-Fi storage: existing codecs, native copied operations and explicit
 * availability generations. No provider choice or legacy-write fallback. */
static bool store_guard(void *unused){(void)unused;return live(&wm);}
static int32_t checked_state(void *c,risc_key_value_atomic_state_v1 *v){
 wm_store*s=c;if(!live(&wm))return RISC_ATOMIC_RETAINED;
 int rc=s->raw->state(s->raw->context,v);
 return live(&wm)?rc:RISC_ATOMIC_RETAINED;
}
static int32_t checked_read(void*c,uint64_t r,const char*k,void*v,uint32_t n,uint32_t*out){
 wm_store*s=c;if(out)*out=0;if(!live(&wm))return RISC_ATOMIC_RETAINED;
 int rc=s->raw->get(s->raw->context,r,k,v,n,out);
 return live(&wm)?rc:RISC_ATOMIC_RETAINED;
}
static int32_t checked_submit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*m,uint32_t n,const void*i,uint32_t z,uint64_t*t){
 wm_store*s=c;*t=0;if(!live(&wm))return RISC_ATOMIC_RETAINED;
 int rc=s->raw->submit(s->raw->context,r,m,n,i,z,t);
 if(!live(&wm))return RISC_ATOMIC_RETAINED;
 if((rc==RISC_ATOMIC_OK)!=(*t!=0)){retain(&wm);return RISC_ATOMIC_RETAINED;}
 return rc;
}
static int32_t checked_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){
 wm_store*s=c;if(!live(&wm))return RISC_ATOMIC_RETAINED;
 int rc=s->raw->collect(s->raw->context,t,r);
 return live(&wm)?rc:RISC_ATOMIC_RETAINED;
}
static int32_t checked_cancel(void*c,uint64_t t){
 wm_store*s=c;if(!live(&wm))return RISC_ATOMIC_RETAINED;
 int rc=s->raw->cancel(s->raw->context,t);
 return live(&wm)?rc:RISC_ATOMIC_RETAINED;
}
static int32_t store_get(void*c,const char*k,void*v,uint32_t n,uint32_t*out){
 wm_store*s=c;if(out)*out=0;if(!s->raw||!s->read_revision)return RISC_KEY_VALUE_IO;
 int rc=checked_read(s,s->read_revision,k,v,n,out);
 return rc==RISC_ATOMIC_OK?RISC_KEY_VALUE_OK:rc==RISC_ATOMIC_NOT_FOUND?RISC_KEY_VALUE_NOT_FOUND:
 rc==RISC_ATOMIC_BUSY||rc==RISC_ATOMIC_STALE?RISC_KEY_VALUE_BUSY:
 rc==RISC_ATOMIC_BUFFER_SMALL?RISC_KEY_VALUE_BUFFER_SMALL:RISC_KEY_VALUE_IO;
}
static int32_t store_deny_put(void*c,const char*k,const void*v,uint32_t n){
 (void)c;(void)k;(void)v;(void)n;return RISC_KEY_VALUE_CONTEXT;
}
static bool store_observe(wifi_manager*a,wm_store*s){
 if(s->namespace_id==1)return s->raw&&s->loaded;
 if(!s->availability.api)return false;
 bool ok=portable_settings_availability_refresh(&s->availability);
 if(!live(a))return false;
 if(!ok){retain(a);return false;}return true;
}
static bool store_cached_ready(const wm_store*s){
 return s->raw&&s->loaded&&(s->namespace_id==1||(
 portable_settings_availability_available(&s->availability)&&
 s->loaded_generation==s->availability.state.generation));
}
static bool store_ready(wifi_manager*a,wm_store*s){return store_observe(a,s)&&store_cached_ready(s);}
static const char *store_notice(const wm_store*s){
 if(!s->availability.api)return "Storage unavailable; RAM draft retained";
 if(s->availability.state.availability==RISC_KEY_VALUE_CONFLICT)return "Storage conflict; sources preserved";
 if(s->availability.state.availability==RISC_KEY_VALUE_RECONCILIATION_PENDING)return "Recovery pending; clean reload needed";
 if(!portable_settings_availability_available(&s->availability))return "Storage unavailable; RAM draft retained";
 if(!s->raw)return "Storage grant unavailable; reload to retry";
 return "Storage changed; keep or discard draft";
}
static const char *atomic_notice(int rc){
 return rc==RISC_ATOMIC_CONFLICT?"Saved-network conflict; nothing changed":
 rc==RISC_ATOMIC_STALE?"Saved networks changed; retry explicitly":
 rc==RISC_ATOMIC_CAPACITY?"Storage full; nothing changed":
 rc==RISC_ATOMIC_UNCERTAIN?"Save uncertain; reload before retry":
 rc==RISC_ATOMIC_CANCELLED?"Storage operation cancelled":
 "Storage failed; reload before retry";
}
static bool store_open(wifi_manager*a,wm_store*s,unsigned ns){
 memset(s,0,sizeof(*s));s->namespace_id=ns;
 s->view=(risc_key_value_v1){1,sizeof(s->view),s,store_get,store_deny_put};
 bool ok=true;
 if(ns!=1)ok=portable_settings_availability_open(&s->availability,a->rt,ns);
 if(!live(a))return false;
 if(!ok&&(s->availability.grant.api||s->availability.grant.slot||s->availability.grant.generation)){retain(a);return false;}
 s->grant=(risc_runtime_capability_v1){.struct_size=sizeof(s->grant)};
 ok=a->rt->acquire(ns==1?PORTABLE_PREFERENCE_CAPABILITY:RISC_KEY_VALUE_ATOMIC_CAPABILITY,1,ns,&s->grant);
 if(!live(a))return false;
 if(!ok){if(s->grant.api||s->grant.slot||s->grant.generation||s->grant.struct_size!=sizeof(s->grant))return retain(a);return true;}
 if(s->grant.struct_size!=sizeof(s->grant)||!s->grant.slot||!s->grant.generation||!portable_wifi_atomic_api_valid(s->grant.api))return retain(a);
 s->raw=s->grant.api;
 s->checked=(risc_key_value_atomic_v1){1,sizeof(s->checked),s,checked_state,checked_read,checked_submit,checked_collect,checked_cancel};
 s->loaded=ns==1||portable_settings_availability_available(&s->availability);
 s->loaded_generation=s->availability.state.generation;
 return true;
}
static bool store_close(wifi_manager*a,wm_store*s){
 if(!live(a))return false;
 if(s->grant.api){bool ok=a->rt->release(&s->grant);if(!live(a))return false;
  if(!ok||s->grant.api||s->grant.slot||s->grant.generation)return retain(a);}
 if(!portable_settings_availability_close(&s->availability,a->rt)||!live(a))return retain(a);
 memset(s,0,sizeof(*s));return true;
}
static int store_read_begin(wifi_manager*a,wm_store*s){
 s->read_revision=0;if(!store_ready(a,s))return RISC_ATOMIC_IO;
 risc_key_value_atomic_state_v1 st={.struct_size=sizeof(st)};
 int rc=checked_state(s,&st);if(!live(a))return RISC_ATOMIC_RETAINED;
 if(rc)return rc;
 if(st.struct_size<sizeof(st)||st.namespace_id!=s->namespace_id||!st.revision||st.reserved)return RISC_ATOMIC_INVALID;
 s->read_revision=st.revision;return RISC_ATOMIC_OK;
}
static bool storage_pending(wifi_manager*a){
 return portable_wifi_atomic_pending(&a->atomic_edit)||a->policy_edit.token;
}
static bool storage_idle(wifi_manager*a){
 return !storage_pending(a)&&!a->db_job&&!a->atomic_ready&&!a->draft_dirty&&
 !a->policy_dirty&&!a->keyboard_pending&&!a->editor_session;
}
static void storage_discard(wifi_manager*a){
 if(storage_pending(a))return;
 portable_wifi_atomic_clear(&a->atomic_edit);a->atomic_ready=false;
 wipe(&a->draft,sizeof(a->draft));a->draft_dirty=false;a->db_job=DB_NONE;
 a->save_on_connect=false;a->selected_saved=false;a->selected_slot=UINT32_MAX;
}
static void storage_reload(wifi_manager*a){
 if(!storage_idle(a)||a->operation||a->queued_kind||a->stopping){notice(a,"Disconnect and finish editing before reload");return;}
 if(!store_close(a,&a->network_storage)||!store_close(a,&a->radio_storage))return;
 a->store=a->policy=NULL;a->rt->yield_ms(1);if(!live(a))return;
 if(!store_open(a,&a->network_storage,6)||!store_open(a,&a->radio_storage,1))return;
 a->store=&a->network_storage.view;a->policy=&a->radio_storage.view;
 a->policy_loading=true;list_start(a);a->dirty=true;
}
static void storage_observe(wifi_manager*a){
 uint64_t before=a->network_storage.availability.state.generation;
 uint32_t old=a->network_storage.availability.state.availability;
 if(a->network_storage.availability.api&&!store_observe(a,&a->network_storage))return;
 if(before!=a->network_storage.availability.state.generation||old!=a->network_storage.availability.state.availability){
  toast(a,"Storage changed; reload when clean");a->dirty=true;
 }
 if(a->radio_storage.availability.api)(void)store_observe(a,&a->radio_storage);
}
static bool storage_start(wifi_manager*a,unsigned job,unsigned kind){
 if(a->db_job||storage_pending(a)||a->atomic_ready){toast(a,"Finish the current storage action");return false;}
 if(!store_ready(a,&a->network_storage)){notice(a,store_notice(&a->network_storage));return false;}
 if(!portable_wifi_credentials_validate(&a->draft)){toast(a,"Name required; password 8 to 63 characters");return false;}
 int rc=portable_wifi_atomic_begin(&a->atomic_edit,&a->network_storage.checked,kind,&a->draft);
 if(rc!=PORTABLE_WIFI_ATOMIC_AGAIN){notice(a,atomic_notice(rc));return false;}
 a->db_job=job;a->storage_cancelling=false;a->dirty=true;return true;
}
static void storage_connect(wifi_manager*a){(void)storage_start(a,DB_PREPARE_CONNECT,PORTABLE_WIFI_ATOMIC_SAVE);}
static void storage_save(wifi_manager*a){(void)storage_start(a,DB_SAVE,PORTABLE_WIFI_ATOMIC_SAVE);}
static void storage_forget(wifi_manager*a){(void)storage_start(a,DB_PREPARE_FORGET,PORTABLE_WIFI_ATOMIC_FORGET);}
static void storage_finish(wifi_manager*a,unsigned job,int rc){
 a->db_job=DB_NONE;a->atomic_ready=false;a->storage_cancelling=false;a->dirty=true;
 if(rc==RISC_ATOMIC_RETAINED){retain(a);return;}
 if(rc!=RISC_ATOMIC_OK){a->save_on_connect=false;toast(a,atomic_notice(rc));return;}
 a->draft_dirty=false;
 if(job==DB_FORGET){wipe(&a->draft,sizeof(a->draft));a->selected_saved=false;route(a,SAVED);list_start(a);toast(a,"Forgotten; old bytes may remain");}
 else {wipe(a->draft.password,sizeof(a->draft.password));a->selected_saved=true;toast(a,"Saved locally; backup not confirmed");}
}
static void db_step(wifi_manager*a){
 if(!a->db_job)return;
 unsigned job=a->db_job;
 if(job==DB_SAVE||job==DB_FORGET||job==DB_PREPARE_CONNECT||job==DB_PREPARE_FORGET){
  bool pending=portable_wifi_atomic_pending(&a->atomic_edit);
  if(!pending&&!store_ready(a,&a->network_storage)){
   portable_wifi_atomic_clear(&a->atomic_edit);a->db_job=DB_NONE;a->atomic_ready=false;
   a->save_on_connect=false;toast(a,"Storage changed; explicit retry required");return;
  }
  if(!storage_begin(a))return;
  int rc=a->storage_cancelling?portable_wifi_atomic_cancel(&a->atomic_edit):portable_wifi_atomic_step(&a->atomic_edit);
  if(!live(a)||!storage_end(a))return;
  if(rc==PORTABLE_WIFI_ATOMIC_AGAIN)return;
  if(rc==PORTABLE_WIFI_ATOMIC_READY){
   if(job==DB_PREPARE_CONNECT){a->atomic_ready=true;a->db_job=DB_NONE;schedule(a,RISC_RADIO_REQUEST_JOIN);return;}
   if(job==DB_PREPARE_FORGET){a->atomic_ready=true;a->db_job=DB_NONE;a->parent=a->screen;route(a,FORGET);return;}
   if(!storage_begin(a))return;
   rc=portable_wifi_atomic_submit(&a->atomic_edit);
   if(!live(a)||!storage_end(a))return;
   if(rc==PORTABLE_WIFI_ATOMIC_AGAIN||rc==PORTABLE_WIFI_ATOMIC_READY)return;
  }
  storage_finish(a,job,rc);return;
 }
 if(!storage_begin(a))return;
 int rc=store_read_begin(a,&a->network_storage);
 if(rc==RISC_ATOMIC_BUSY){(void)storage_end(a);return;}
 if(rc){(void)storage_end(a);a->db_job=DB_NONE;a->db_fault=true;toast(a,"Saved-network storage unavailable");return;}
 bool completed=false;portable_wifi_credentials value;memset(&value,0,sizeof(value));
 if(job==DB_LIST){
  if(!a->db_count)rc=portable_wifi_profile_count(a->store,&a->db_count);
  else if(a->db_next<a->db_count){uint32_t slot=a->db_next;rc=portable_wifi_profile_load(a->store,slot,&value);
   if(rc!=PORTABLE_WIFI_PROFILE_AGAIN){a->db_next++;
    if(!rc&&slot>=a->saved_base&&a->saved_rows<8){unsigned i=a->saved_rows++;a->saved_slots[i]=slot;copy_text(a->saved_names[i],33,value.ssid);}
    if(rc==PORTABLE_WIFI_CREDENTIALS_EMPTY)rc=0;
   }
  }
  if(a->db_count&&a->db_next>=a->db_count)completed=true;
 }else if(job==DB_FIND){
  rc=portable_wifi_profile_search_step(a->store,&a->search);
  if(rc!=PORTABLE_WIFI_PROFILE_AGAIN){
   if(!rc||rc==PORTABLE_WIFI_CREDENTIALS_EMPTY){
    a->selected_slot=a->search.index;
    if(!rc){rc=portable_wifi_profile_load(a->store,a->selected_slot,&value);
     if(!rc){if(a->after_find==AFTER_DETAIL){a->draft=value;a->draft_dirty=false;}a->selected_saved=true;if(a->selected.rssi==-127&&!value.password[0])a->selected.auth=0;}}
    else rc=0;
   }
   if(rc!=PORTABLE_WIFI_PROFILE_AGAIN)completed=true;
  }
 }
 wipe(&value,sizeof(value));if(!live(a)||!storage_end(a))return;
 if(rc==PORTABLE_WIFI_PROFILE_AGAIN)return;
 if(rc){a->db_job=DB_NONE;a->db_fault=true;toast(a,"Storage unreadable; sources preserved");return;}
 if(!completed)return;a->db_job=DB_NONE;a->dirty=true;
 if(job==DB_FIND&&a->after_find==AFTER_CONNECT)connect_selected(a);
}
static unsigned policy_load(wifi_manager*a){
 if(!a->policy)return PORTABLE_RADIO_UNAVAILABLE;
 if(!storage_begin(a))return PORTABLE_RADIO_BUSY;
 int rc=store_read_begin(a,&a->radio_storage);
 unsigned result=rc==RISC_ATOMIC_BUSY?PORTABLE_RADIO_BUSY:rc?PORTABLE_RADIO_UNAVAILABLE:portable_radio_load_result(a->policy,&a->radio_flags);
 if(!live(a)||!storage_end(a))return PORTABLE_RADIO_UNAVAILABLE;return result;
}
static void policy_toggle(wifi_manager*a){
 if(a->policy_dirty||a->policy_edit.token){toast(a,"Radio setting is still saving");return;}
 if(!store_ready(a,&a->radio_storage)){notice(a,store_notice(&a->radio_storage));return;}
 if(!storage_begin(a))return;
 int rc=portable_preference_atomic_begin(&a->policy_edit,&a->radio_storage.checked,PORTABLE_RADIO_KEY,PORTABLE_PREFERENCE_USER,store_guard,a);
 if(!live(a)||!storage_end(a))return;
 if(rc!=PORTABLE_PREFERENCE_READY){notice(a,atomic_notice(rc));return;}
 uint8_t old=PORTABLE_RADIO_WIFI;
 if(a->policy_edit.present&&!portable_radio_decode(a->policy_edit.expected,a->policy_edit.expected_size,&old)){notice(a,"Radio setting unreadable; sources preserved");return;}
 a->policy_proposed=old^PORTABLE_RADIO_WIFI;
 if(a->policy_proposed&PORTABLE_RADIO_WIFI)a->policy_proposed&=(uint8_t)~PORTABLE_RADIO_AIRPLANE;
 uint8_t bytes[4]={0x51,1,a->policy_proposed,(uint8_t)(a->policy_proposed^0xa5)};
 rc=portable_preference_atomic_prepare(&a->policy_edit,bytes,sizeof(bytes));
 if(rc!=PORTABLE_PREFERENCE_READY){notice(a,atomic_notice(rc));return;}
 a->policy_dirty=true;a->dirty=true;
}
static bool policy_step(wifi_manager*a){
 if(a->policy_loading&&!a->policy_dirty){
  unsigned result=policy_load(a);if(!live(a))return false;
  if(result==PORTABLE_RADIO_BUSY)return true;
  a->policy_loading=false;a->enabled=result<=PORTABLE_RADIO_DEFAULT&&(a->radio_flags&PORTABLE_RADIO_WIFI);a->dirty=true;
 }
 if(!a->policy_dirty)return true;
 if(!a->policy_edit.token&&!store_ready(a,&a->radio_storage)){a->policy_dirty=false;notice(a,store_notice(&a->radio_storage));return live(a);}
 if(!storage_begin(a))return live(a);
 int rc=a->policy_edit.token?portable_preference_atomic_collect(&a->policy_edit):portable_preference_atomic_submit(&a->policy_edit);
 if(!live(a)||!storage_end(a))return false;
 if(rc==PORTABLE_PREFERENCE_READY||rc==RISC_ATOMIC_PENDING)return true;
 if(rc==RISC_ATOMIC_RETAINED)return retain(a);
 a->policy_dirty=false;
 if(rc){notice(a,atomic_notice(rc));return true;}
 a->radio_flags=a->policy_proposed;a->enabled=(a->radio_flags&PORTABLE_RADIO_WIFI)!=0;
 if(!a->enabled){a->queued_kind=0;(void)stop_radio(a);}else schedule(a,RISC_RADIO_REQUEST_SCAN);
 a->dirty=true;return true;
}
static bool storage_cancel(wifi_manager*a){
 if(a->atomic_edit.token){
  a->storage_cancelling=true;if(!a->db_job)a->db_job=DB_SAVE;db_step(a);
  if(!live(a)||a->atomic_edit.token)return false;
 }else if(a->atomic_ready||a->db_job==DB_PREPARE_CONNECT||a->db_job==DB_PREPARE_FORGET||a->db_job==DB_SAVE||a->db_job==DB_FORGET){
  portable_wifi_atomic_clear(&a->atomic_edit);a->atomic_ready=false;a->db_job=DB_NONE;a->save_on_connect=false;
 }
 if(a->policy_edit.token){
  if(!storage_begin(a))return false;
  int rc=portable_preference_atomic_cancel(&a->policy_edit);if(!live(a)||!storage_end(a))return false;
  if(rc==RISC_ATOMIC_PENDING)return false;if(rc==RISC_ATOMIC_RETAINED)return retain(a);
  a->policy_dirty=false;
  if(rc==RISC_ATOMIC_OK){a->radio_flags=a->policy_proposed;a->enabled=(a->radio_flags&PORTABLE_RADIO_WIFI)!=0;}
 }else a->policy_dirty=false;
 return !storage_pending(a);
}
static bool storage_exit(wifi_manager*a){
 if(!storage_cancel(a)||!live(a))return false;
 if(a->draft_dirty&&!a->storage_leave){
  a->exit_requested=false;a->shared_reason=0;a->parent=a->screen;route(a,STORAGE);return false;
 }
 a->db_job=DB_NONE;return true;
}
static bool storage_action(wifi_manager*a,unsigned intent){
 if(storage_pending(a)||a->db_job==DB_SAVE||a->db_job==DB_FORGET||a->db_job==DB_PREPARE_CONNECT||a->db_job==DB_PREPARE_FORGET){
  if(intent==BACK||intent==CANCEL){(void)storage_cancel(a);a->queued_kind=0;a->save_on_connect=false;(void)stop_radio(a);}
  else toast(a,"Storage action in progress; Back cancels");
  return false;
 }
 if(intent==BACK||intent==CANCEL){
  if(a->atomic_ready)(void)storage_cancel(a);
  if(a->draft_dirty&&a->screen!=STORAGE){a->parent=a->screen;route(a,STORAGE);return false;}
 }
 if(a->draft_dirty&&(intent==ADD_NETWORK||intent==TAB||
 (intent>=SELECT_SCAN&&intent<SELECT_SCAN+GARDEN_RADIO_SCAN_MAX)||
 (intent>=SELECT_SAVED&&intent<SELECT_SAVED+8))){
  a->parent=a->screen;route(a,STORAGE);return false;
 }
 return true;
}
