/* Production Settings entry, paper hit routing and checked preference helper.
 * Only providers are substituted; app state changes through actual input. */
#define main unused_native_settings_main
#include "portable_native_time_settings_test.c"
#undef main
static uint8_t direction_record[4]={0x44,1,0,0xa5};
static bool direction_exists,direction_readback_fault;
static unsigned direction_puts,direction_reads,direction_pages,direction_choices;
static bool direction_saw_unconfirmed,direction_saw_unavailable,direction_saw_invalid;
static int32_t direction_get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *used){
 if(strcmp(key,PORTABLE_DESK_DIRECTION_KEY))return kv_get(c,key,out,capacity,used);
 io();assert(c==leases&&kind_live(K_KV));++direction_reads;*used=0;
 if(which("unavailable"))return RISC_KEY_VALUE_IO;
 if(!direction_exists)return RISC_KEY_VALUE_NOT_FOUND;
 assert(capacity>=4);*used=4;memcpy(out,direction_record,4);
 if(which("invalid")||direction_readback_fault)((uint8_t*)out)[3]^=1;
 return RISC_KEY_VALUE_OK;
}
static int32_t direction_put(void *c,const char *key,const void *data,uint32_t size){
 if(strcmp(key,PORTABLE_DESK_DIRECTION_KEY))return kv_put(c,key,data,size);
 io();assert(c==leases&&kind_live(K_KV));assert(size==4);++direction_puts;
 const uint8_t *record=data;assert(record[0]==0x44&&record[1]==1&&record[2]<=1&&record[3]==(uint8_t)(record[2]^0xa5));
 if(which("write-fail")||(which("write-retry")&&direction_puts==1))return RISC_KEY_VALUE_IO;
 memcpy(direction_record,data,4);direction_exists=true;
 if(which("readback-fail"))direction_readback_fault=true;
 return RISC_KEY_VALUE_OK;
}
static bool direction_show(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,size_t count,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 if(sv_page==SV_DESK_DIRECTION){
  ++direction_pages;direction_choices|=1u<<desk_direction_choice;
  direction_saw_unconfirmed|=desk_direction_unconfirmed;
  direction_saw_unavailable|=!strcmp(desk_direction_message,"Storage unavailable");
  direction_saw_invalid|=!strcmp(desk_direction_message,"Invalid saved - choose direction");
 }
 if(options->intent!=RISC_DISPLAY_PRESENT_CLEAN)assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
 return frame_show(c,frame,damage,count,options,token);
}
static bool direction_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!acquire(name,version,instance,out))return false;
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){
  static risc_key_value_v1 api;api=*(const risc_key_value_v1*)out->api;api.get=direction_get;api.put=direction_put;out->api=&api;
 }
 if(!strcmp(name,"display.output")){
  static risc_display_output_api_v1 api;api=display_api;api.submit=direction_show;out->api=&api;
 }
 return true;
}
int main(int argc,char **argv){
 assert(argc==2);test_name=argv[1];zone_id="America/Denver";native_epoch=1768505696;configure_records();
 assert(SETTINGS_DESK_DIRECTION_ROW+1==SETTINGS_ROW_COUNT);
 direction_exists=which("save-left")||which("invalid");
 if(which("save-left")){direction_record[2]=1;direction_record[3]=1^0xa5;}
 runtime.acquire=direction_acquire;
 nav(RISC_NAV_UP);nav(RISC_NAV_CONFIRM); /* Last appended row, using app navigation. */
 if(which("unavailable")||which("invalid")){nav(RISC_NAV_BACK);nav(RISC_NAV_HOME);}
 else {
  tap(LOGICAL_WIDTH/2,which("save-left")?180:300);
  if(which("cancel"))tap(100,FOOTER_Y);
  else if(which("home"))nav(RISC_NAV_HOME);
  else {
   tap(SAVE_X,FOOTER_Y);
   if(which("write-retry"))tap(SAVE_X,FOOTER_Y);
   if(which("write-fail")||which("readback-fail"))tap(100,FOOTER_Y);
   if(which("save-reopen")){nav(RISC_NAV_CONFIRM);tap(LOGICAL_WIDTH/2,180);tap(100,FOOTER_Y);}
  }
  nav(RISC_NAV_HOME);
 }
 assert(app_module_init()==0);in_main=true;app_main();
 assert(!retained&&launches==1&&direction_pages>0&&sv_page==SV_ROOT&&!seeds&&!rtc_writes&&!basis_puts&&!zone_puts&&!other_puts&&!radio_puts&&!quick_puts);
 if(which("cancel")||which("home")||which("unavailable")||which("invalid"))assert(!direction_puts);
 else if(which("write-fail"))assert(direction_puts==1&&!direction_exists&&direction_saw_unconfirmed&&desk_direction_unconfirmed);
 else if(which("readback-fail"))assert(direction_puts==1&&direction_saw_unconfirmed&&desk_direction_unconfirmed);
 else {
  assert(direction_puts==(which("write-retry")?2u:1u)&&direction_exists&&!desk_direction_unconfirmed);
  assert(direction_record[2]==(which("save-left")?0:1));
  if(which("write-retry"))assert(direction_saw_unconfirmed);
 }
 if(which("unavailable"))assert(direction_saw_unavailable);
 if(which("invalid"))assert(direction_saw_invalid);
 if(!which("unavailable")&&!which("invalid"))assert(direction_choices==3);
 in_main=false;in_fini=true;app_module_fini();in_fini=false;
 assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"case\":\"%s\",\"reads\":%u,\"writes\":%u,\"stored_direction\":%u,\"unconfirmed_seen\":%s,\"pages\":%u,\"width\":%u,\"height\":%u}\n",test_name,direction_reads,direction_puts,direction_record[2],direction_saw_unconfirmed?"true":"false",direction_pages,LOGICAL_WIDTH,LOGICAL_HEIGHT);
 return 0;
}
