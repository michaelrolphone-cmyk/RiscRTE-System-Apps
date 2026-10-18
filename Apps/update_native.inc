/* Opt-in App Store presentation identity. Service/bank authority remains in
 * the existing updater; this stores only the release the user actually saw. */
enum {UN_CATALOG,UN_CONFIRM,UN_PROGRESS,UN_CLEANUP,UN_RESTART};
static unsigned un_mode,un_selected,un_model_mode;
static bool un_model_valid,un_release_valid,un_candidate_valid;
static software_update_row_v1 un_release,un_candidate;
static unsigned un_current_mode(void) {
 return restart_pending?UN_RESTART:cleanup_pending?UN_CLEANUP:
  connecting||ust.state==SOFTWARE_UPDATE_CATALOG||(ust.state>=SOFTWARE_UPDATE_PREPARING&&ust.state<=SOFTWARE_UPDATE_READY)?UN_PROGRESS:
  confirming?UN_CONFIRM:UN_CATALOG;
}
static bool un_string(const char *s,unsigned n){return memchr(s,0,n)!=NULL;}
static bool un_get(software_update_row_v1 *r) {
 *r=(software_update_row_v1){.struct_size=sizeof(*r)};
 return !PORTABLE_APP_RETAINED()&&selected&&selected<=ust.count&&selected<=SOFTWARE_UPDATE_ROWS_MAX&&us&&
  us->get(us->context,selected-1,r)&&!PORTABLE_APP_RETAINED()&&
  un_string(r->id,sizeof(r->id))&&un_string(r->version,sizeof(r->version))&&
  un_string(r->installed,sizeof(r->installed))&&un_string(r->reason,sizeof(r->reason));
}
static bool un_same(const software_update_row_v1 *a,const software_update_row_v1 *b) {
 return a->availability==b->availability&&a->size==b->size&&!strcmp(a->id,b->id)&&
  !strcmp(a->version,b->version)&&!strcmp(a->installed,b->installed)&&!strcmp(a->reason,b->reason);
}
static void un_changed(void) {
 portable_wifi_native_invalidate();un_model_valid=false;confirming=false;install_after_connect=false;
 selected=0;message="Release changed; choose and confirm again";dirty=true;
}
static bool un_selected_current(void) {
 software_update_row_v1 current;
 if(un_get(&current)&&un_same(&current,&selected_row))return true;
 un_changed();return false;
}
static bool un_model_current(void) {
 if(!un_model_valid||un_model_mode!=un_current_mode()||un_selected!=(confirming?confirm_choice:selected))return false;
 if(un_current_mode()==UN_CONFIRM)return un_selected_current();
 if(un_current_mode()==UN_CATALOG&&selected){
  software_update_row_v1 current;
  if(!un_release_valid||!un_get(&current)||!un_same(&current,&un_release)){un_changed();return false;}
 }
 return true;
}
static void un_prepare(void) {
 if(us&&!cleanup_pending&&!restart_pending){
  software_update_status_v1 next={.struct_size=sizeof(next)};
  if(!us->status(us->context,&next)){cleanup_pending=true;message="Service status unavailable";dirty=true;}
  else {if(memcmp(&ust,&next,sizeof(ust)))dirty=true;ust=next;}
 }
 if(ust.state==SOFTWARE_UPDATE_RETAINED){PORTABLE_APP_RETAIN();cleanup_pending=true;dirty=true;return;}
 if(confirming&&!un_selected_current())return;
 unsigned mode=un_current_mode();
 if(mode==UN_CATALOG&&un_model_valid&&un_model_mode==mode&&un_selected==selected&&selected){
  software_update_row_v1 current;
  if(!un_release_valid||!un_get(&current)||!un_same(&current,&un_release)){un_changed();return;}
 }
 if(mode!=un_mode){portable_wifi_native_invalidate();un_model_valid=false;dirty=true;un_mode=mode;}
}
static void un_remember(void) {
 un_model_valid=true;un_model_mode=un_current_mode();un_selected=confirming?confirm_choice:selected;
 un_release=un_candidate;un_release_valid=un_candidate_valid;
}
