/* Real controller/profile codec, with the native NVS backend supplied by the
 * C++ bridge. Only radio/text/peripherals and outer grants are synthetic. */
#define PORTABLE_WIFI_REMEMBER_CONNECTED
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"
extern void persistence_open(const char *path);
extern void persistence_fault(int mode);
extern unsigned persistence_writes(void);
extern int32_t persistence_get(void*,const char*,void*,uint32_t,uint32_t*);
extern int32_t persistence_put(void*,const char*,const void*,uint32_t);
static risc_key_value_v1 persistent={1,sizeof(persistent),NULL,persistence_get,persistence_put};
static bool persistent_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!workflow_acquire(n,v,id,g))return false;
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(id==6);g->api=&persistent;}
 return true;
}
static void reopen(void){
 assert(portable_wifi_close());runtime_api.acquire=persistent_acquire;assert(wifi_open());
}
static void drain_save(void){unsigned n=0;while(remember_pending||profile_work){wifi_profiles_step();assert(++n<256);}}
static void offline(void){assert(!wifi_disconnect());stopped();}
static void connect_value(const char* name,const char* password){
 strcpy(credentials.ssid,name);strcpy(credentials.password,password);open_network=!password[0];draft_dirty=true;
 wifi_connect();assert(joining&&remember_join);
 progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;tick(250);
 assert(remember_pending&&!remember_join);drain_save();
 assert(saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED&&!draft_dirty);
 const char *message=wifi_message;tick(250);assert(wifi_message==message);offline();
}
static void check(unsigned index,const char* name,const char* password){
 portable_wifi_credentials value={{0},{0}};
 assert(portable_wifi_profile_load(wifi_store,index,&value)==0);
 assert(!strcmp(value.ssid,name)&&!strcmp(value.password,password));portable_wifi_credentials_clear(&value);
}
int main(int argc,char **argv){
 assert(argc==3);persistence_open(argv[2]);workflow_start(true);reopen();
 if(!strcmp(argv[1],"write")){
  unsigned before=persistence_writes();
  strcpy(credentials.ssid,"Synthetic failed");strcpy(credentials.password,"synthetic-pass");wifi_connect();
  progress.failure=RISC_RADIO_FAILURE_SETUP;stopped();wifi_profiles_step();assert(persistence_writes()==before);
  connect_value("Synthetic one","synthetic-one-pass");connect_value("Synthetic two","synthetic-two-pass");
  check(0,"Synthetic one","synthetic-one-pass");check(1,"Synthetic two","synthetic-two-pass");
  reopen();assert(!strcmp(credentials.ssid,"Synthetic one"));
  wifi_saved_open(0);while(saved_loading_active)wifi_profiles_step();assert(saved_rows==2);
  wifi_activate(1);assert(!strcmp(credentials.ssid,"Synthetic two")&&!strcmp(credentials.password,"synthetic-two-pass"));
  wifi_edit(WP_PASSWORD);assert(text_request.reserved&RISC_TEXT_ENTRY_REQUEST_MASKED);
  text_done(RISC_TEXT_ENTRY_ACCEPTED,"synthetic-two-edited",0);wifi_activate(6);drain_save();
  check(0,"Synthetic one","synthetic-one-pass");check(1,"Synthetic two","synthetic-two-edited");
 }else if(!strcmp(argv[1],"reboot")){
  assert(!strcmp(credentials.ssid,"Synthetic one"));
  check(0,"Synthetic one","synthetic-one-pass");check(1,"Synthetic two","synthetic-two-edited");
  wifi_saved_select(1);wifi_activate(7);while(profile_work)wifi_profiles_step();assert(page==WP_FORGET);
  persistence_fault(1);wifi_activate(1);assert(page==WP_FORGET&&forget_index_valid&&!strcmp(credentials.ssid,"Synthetic two"));
  check(1,"Synthetic two","synthetic-two-edited");persistence_fault(0);wifi_activate(1);
  portable_wifi_credentials v;assert(portable_wifi_profile_load(wifi_store,1,&v)==PORTABLE_WIFI_CREDENTIALS_EMPTY);
  check(0,"Synthetic one","synthetic-one-pass");
  connect_value("Synthetic three","synthetic-three-pass");check(1,"Synthetic three","synthetic-three-pass");
 }else if(!strcmp(argv[1],"fault")){
  check(0,"Synthetic one","synthetic-one-pass");
  strcpy(credentials.ssid,"Synthetic four");strcpy(credentials.password,"synthetic-four-pass");draft_dirty=true;wifi_connect();
  progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;tick(250);
  service_busy=true;unsigned before=persistence_writes();wifi_profiles_step();assert(remember_pending&&persistence_writes()==before);
  assert(!portable_wifi_close()&&portable_wifi_stop_pending()&&opened);
  service_busy=false;persistence_fault(1);drain_save();assert(saved_state!=0&&draft_dirty);
  const char *message=wifi_message;tick(250);assert(wifi_message==message);
  persistence_fault(0);wifi_activate(6);drain_save();assert(saved_state==0);offline();
  check(0,"Synthetic one","synthetic-one-pass");check(1,"Synthetic three","synthetic-three-pass");check(2,"Synthetic four","synthetic-four-pass");
 }else if(!strcmp(argv[1],"snapshot")){
  strcpy(credentials.ssid,"Synthetic submitted");strcpy(credentials.password,"synthetic-submitted-pass");draft_dirty=true;wifi_connect();
  /* A late UI edit cannot change which credentials actually associated. */
  strcpy(credentials.ssid,"Synthetic unsent");strcpy(credentials.password,"synthetic-unsent-pass");
  progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;tick(250);drain_save();
  check(3,"Synthetic submitted","synthetic-submitted-pass");assert(draft_dirty);offline();
  connect_value("Synthetic open","");check(4,"Synthetic open","");
  assert(service_begins==service_ends);
 }else assert(0);
 assert(!remember_pending&&!profile_work);finish();puts("Selected Wi-Fi persistent multi-profile workflow PASS");
}
