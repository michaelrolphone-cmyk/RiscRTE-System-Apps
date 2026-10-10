/* Actual selected Settings API, root navigation and preference editors.
 * Reuse native provider doubles; all tested controller calls are production. */
#define main unused_native_settings_main
#include "portable_native_time_settings_test.c"
#undef main

static const char *const keys[]={PORTABLE_SLEEP_KEY,PORTABLE_TIME_FORMAT_KEY,
  PORTABLE_DESK_FACE_KEY,PORTABLE_DESK_DIRECTION_KEY,PORTABLE_READER_LANGUAGE_KEY};
static uint8_t records[5][4]={{0x53,1,0,0xa5}};
static unsigned sizes[5]={4},reads[5],preference_puts[5],page_visits[32];
static unsigned sleep_status;
static int record_index(const char *key) {
  for(unsigned i=0;i<5;++i)if(!strcmp(keys[i],key))return (int)i;
  return -1;
}
static int32_t lock_get(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *size) {
  int i=record_index(key);if(i<0)return kv_get(ctx,key,out,cap,size);
  io();assert(ctx==leases&&kind_live(K_KV));++reads[i];*size=sizes[i];
  if(i==0&&sleep_status==5)return RISC_KEY_VALUE_IO;
  if(!*size)return RISC_KEY_VALUE_NOT_FOUND;
  if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
  memcpy(out,records[i],*size);return RISC_KEY_VALUE_OK;
}
static int32_t lock_put(void *ctx,const char *key,const void *data,uint32_t size) {
  int i=record_index(key);if(i<0)return kv_put(ctx,key,data,size);
  io();assert(ctx==leases&&kind_live(K_KV)&&size==4);++preference_puts[i];
  assert(i!=0);memcpy(records[i],data,size);sizes[i]=size;return RISC_KEY_VALUE_OK;
}
static bool lock_show(void *ctx,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,
 size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
  assert(sv_page<32);++page_visits[sv_page];assert(sv_page!=SV_SLEEP);
  return frame_show(ctx,frame,damage,count,options,token);
}
static bool lock_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
  if(!acquire(name,version,instance,out))return false;
  if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {
    static risc_key_value_v1 api;api=kv_api;api.get=lock_get;api.put=lock_put;out->api=&api;
  } else if(!strcmp(name,"display.output")) {
    static risc_display_output_api_v1 api;api=display_api;api.submit=lock_show;out->api=&api;
  }
  return true;
}
static bool lock_launch(const char *path) {
  io();assert(!strcmp(path,PORTABLE_RETURN_APP));++launches;return true;
}
static void inputs(void) {memset(nav_buttons,0,sizeof(nav_buttons));contact_count=0;next_at=polls+5;}
static void choose(const t5_app_api_v1 *api,unsigned row,unsigned page) {
  inputs();nav(RISC_NAV_RIGHT);nav(RISC_NAV_CONFIRM);
  assert(api->settings_activate(0,row)==T5_APP_SETTING_UPDATED);
  assert(sv_page==SV_ROOT&&page_visits[page]);
}
int main(int argc,char **argv) {
  assert(argc==2);sleep_status=(unsigned)atoi(argv[1]);assert(sleep_status<6);
  test_name="home-desk-lock";zone_id="UTC";native_epoch=1768505696;
  records[0][2]=(uint8_t)(sleep_status<3?sleep_status:0);records[0][3]=records[0][2]^0xa5;
  if(sleep_status==3)records[0][0]^=0x80;
  if(sleep_status==4)sizes[0]=0;
  uint8_t saved[4];memcpy(saved,records[0],4);unsigned saved_size=sizes[0];
  configure_records();runtime.acquire=lock_acquire;runtime.request_launch=lock_launch;
  assert(app_module_init()==0);in_main=true;
  const t5_app_api_v1 *api=t5_app_get_api(1);assert(api);
  assert(api->settings_count(0)==11&&SETTINGS_ABOUT_ROW==5&&SETTINGS_ALERT_ROW==6&&
    SETTINGS_FACE_ROW==7&&SETTINGS_FLIP_ROW==8&&SETTINGS_LANGUAGE_ROW==9&&SETTINGS_DESK_DIRECTION_ROW==10);
  const char *const labels[]={"Set Time","Time Zone","Time Format","RTC Basis","Top-right key",
    "About","Alarm Alerts","Clock face","Flip UI 180°","Language","Desk orientation"};
  for(unsigned i=0;i<11;++i) {
    t5_app_setting_t item;assert(api->settings_get(0,i,&item)&&!strcmp(item.label,labels[i]));
    if(i==4)assert(item.type==T5_APP_SETTING_VALUE&&!strcmp(item.value,"Desk clock"));
    else if(i!=3)assert(item.type==T5_APP_SETTING_ACTION);
  }
  assert(!reads[0]);
  unsigned calls=provider_calls,polled=polls;
  for(unsigned i=0;i<20;++i)assert(api->settings_activate(0,4)==T5_APP_SETTING_NO_CHANGE);
  assert(provider_calls>=calls&&polls==polled&&!reads[0]&&!preference_puts[0]&&sv_page==SV_ROOT);
  assert(api->settings_activate(1,4)==T5_APP_SETTING_ERROR);
  api->settings_render(0,5);assert(sv_page==SV_ROOT);
  uint32_t category=0;int32_t selected=0;
  assert(4>=sp_first&&4<sp_first+sp_rows());
  assert(api->settings_touch(80,SP_TOP+(4-sp_first)*SP_ROW+20,&category,&selected)==T5_APP_SETTING_NO_CHANGE);
  assert(selected==5&&sv_page==SV_ROOT&&!reads[0]&&!preference_puts[0]);
  choose(api,2,SV_TIME_FORMAT);assert(preference_puts[1]==1);
  choose(api,SETTINGS_FACE_ROW,SV_DESK_FACE);assert(preference_puts[2]==1);
  choose(api,SETTINGS_DESK_DIRECTION_ROW,SV_DESK_DIRECTION);assert(preference_puts[3]==1);
  choose(api,SETTINGS_FLIP_ROW,SV_FLIP);assert(other_puts==1);
  choose(api,SETTINGS_LANGUAGE_ROW,SV_LANGUAGE);assert(preference_puts[4]==1);
  inputs();nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);
  assert(api->settings_activate(0,1)==T5_APP_SETTING_UPDATED&&zone_puts==1);
  assert(page_visits[SV_TIMEZONE_REGIONS]&&page_visits[SV_TIMEZONE_CITIES]);
  /* Root navigation can cross the readonly row and still open its neighbor. */
  inputs();for(unsigned i=0;i<5;++i)nav(RISC_NAV_DOWN);
  nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);
  nav(RISC_NAV_BACK);end_app();app_main();
  assert(page_visits[SV_ABOUT]&&!page_visits[SV_SLEEP]&&launches==1);
  assert(!reads[0]&&!preference_puts[0]&&sizes[0]==saved_size&&!memcmp(saved,records[0],4));
  assert(!rtc_writes&&!seeds&&!basis_puts&&!retained);
  in_main=false;in_fini=true;app_module_fini();in_fini=false;
  assert(!live&&!subscriptions&&!frames&&!barriers);
  printf("Home desk-lock Settings: saved state %u, readonly row and navigation passed\n",sleep_status);
}
