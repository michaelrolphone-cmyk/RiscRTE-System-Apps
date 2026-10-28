/* Actual Settings controller, root/editor presentation and explicit saves. */
#define PORTABLE_SLEEP_SETTINGS
#ifndef PORTABLE_SETTINGS_BOTH_TIMERS
#define PORTABLE_SETTINGS_IDLE_TIMER
#endif
#define TEST_SETTINGS_CUSTOM_RUNTIME
#pragma push_macro("main")
#undef main
#define main inherited_settings_main
#include "portable_settings_test.c"
#undef main
#pragma pop_macro("main")

#ifdef TEST_SETTINGS_FRAME_CAPTURE
static void test_settings_frame_capture(void) {
 const char *dir=getenv("SETTINGS_TIMER_FRAME_DIR");if(!dir)return;
 if(sv_page!=SV_TIMER&&sv_page!=SV_SLEEP)return;
 char path[512];snprintf(path,sizeof(path),"%s/page-%u-deep-%u-value-%u-phase-%u.ppm",dir,sv_page,(unsigned)timer_deep,sv_page==SV_TIMER?(unsigned)timer_choice:sleep_choice,(unsigned)(strstr(sv_page==SV_TIMER?timer_message:sleep_message,"Saving")?1:strstr(sv_page==SV_TIMER?timer_message:sleep_message,"unconfirmed")?2:0));
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",TEST_SETTINGS_RASTER_WIDTH,TEST_SETTINGS_RASTER_HEIGHT);
 for(unsigned i=0;i<TEST_SETTINGS_RASTER_WIDTH*TEST_SETTINGS_RASTER_HEIGHT;++i){unsigned v=framebuffer[i];unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}fclose(f);
}
#endif
static uint8_t records[3][5];
static uint32_t record_sizes[3];
static int32_t record_status[3]={RISC_KEY_VALUE_NOT_FOUND,RISC_KEY_VALUE_NOT_FOUND,RISC_KEY_VALUE_NOT_FOUND};
static unsigned record_writes[3];
#ifdef TEST_SETTINGS_CUSTODY
static unsigned retained_barriers;
static bool retain_write;
static bool selected_retain(void){assert(!native_custody_retained);++retained_barriers;return true;}
#endif
static bool write_failure,committed_error,verification_error,verification_blocked;
static risc_runtime_api_v1 selected_runtime;
static int record_index(const char *key) {
 return !strcmp(key,PORTABLE_SLEEP_KEY)?0:!strcmp(key,PORTABLE_SLEEP_IDLE_KEY)?1:!strcmp(key,PORTABLE_SLEEP_DEEP_KEY)?2:-1;
}
static int32_t selected_get(void *context,const char *key,void *data,uint32_t cap,uint32_t *size) {
 (void)context;active_provider();int i=record_index(key);*size=0;
 if(verification_error&&verification_blocked)return RISC_KEY_VALUE_IO;
 if(i<0)return RISC_KEY_VALUE_NOT_FOUND;
 *size=record_sizes[i];if(record_status[i]!=RISC_KEY_VALUE_OK)return record_status[i];
 if(*size>cap)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(data,records[i],*size);return RISC_KEY_VALUE_OK;
}
static int32_t selected_put(void *context,const char *key,const void *data,uint32_t size) {
 (void)context;active_provider();int i=record_index(key);assert(i>=0&&i<3&&size<=5);
 assert(!strcmp(i?timer_message:sleep_message,"Saving - not confirmed"));
 assert(!sp_dirty&&portable_paper_frame_idle());
#ifdef TEST_SETTINGS_ASYNC
 assert(async_pending_complete);
 assert(async_pending_value==(i?timer_choice:sleep_choice));
#endif
 record_writes[i]++;
#ifdef TEST_SETTINGS_CUSTODY
 if(retain_write)return RISC_KEY_VALUE_CONTEXT;
#endif
 if(write_failure)return RISC_KEY_VALUE_IO;
 memcpy(records[i],data,size);record_sizes[i]=size;record_status[i]=RISC_KEY_VALUE_OK;verification_blocked=true;return committed_error?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 selected_kv={1,sizeof(selected_kv),NULL,selected_get,selected_put};
static bool selected_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g) {
 bool ok=test_acquire(name,version,id,g);
 if(ok&&!strcmp(name,RISC_KEY_VALUE_CAPABILITY))g->api=&selected_kv;
 return ok;
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&selected_runtime:NULL;}
static void set_mode(unsigned mode) {
 record_status[0]=RISC_KEY_VALUE_OK;record_sizes[0]=4;
 records[0][0]=0x53;records[0][1]=1;records[0][2]=mode;records[0][3]=mode^0xa5u;
}
static void set_idle(unsigned seconds) {
 record_status[1]=RISC_KEY_VALUE_OK;record_sizes[1]=5;
 records[1][0]=0x54;records[1][1]=1;records[1][2]=seconds;records[1][3]=seconds>>8;
 records[1][4]=records[1][2]^records[1][3]^0xa5u;
}
static void begin(void) {
 assert(!grants&&!subscriptions&&!frame_count);
 scenario=100;polls=ticks=input_count=0;write_failure=committed_error=verification_error=verification_blocked=false;
 selected_runtime=runtime_api;selected_runtime.acquire=selected_acquire;
 #ifdef TEST_SETTINGS_CUSTODY
 selected_runtime.retain_invocation=selected_retain;
#endif
 assert(app_module_init()==0);
#ifdef TEST_SETTINGS_CUSTODY
 assert(settings_store&&settings_store!=&selected_kv);
#else
 assert(settings_store==&selected_kv);
#endif
 #ifdef PORTABLE_SETTINGS_BOTH_TIMERS
 assert(SETTINGS_ROW_COUNT==8&&SETTINGS_TIMER_ROW==6);
#else
 assert(SETTINGS_ROW_COUNT==7&&SETTINGS_TIMER_ROW==6);
#endif
 settings_render(0,0);polls=0;
}
static void end(void) {
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
 #ifndef PORTABLE_SETTINGS_BOTH_TIMERS
 assert(!record_writes[2]);
 #endif
}
static void root(unsigned mode,unsigned seconds,bool unreadable) {
 t5_app_setting_t item={0};assert(settings_get(0,4,&item));
 const char *name=mode==PORTABLE_SLEEP_DEEP?"Deep":mode==PORTABLE_SLEEP_LIGHT?"Light":"Light then Deep";
 assert(!strcmp(item.value,record_status[0]==RISC_KEY_VALUE_IO?"Unavailable":name));
 assert(settings_get(0,SETTINGS_TIMER_ROW,&item));
 char expected[32];snprintf(expected,sizeof(expected),"%u seconds",seconds);
 assert(!strcmp(item.label,"Idle sleep timer")&&!strcmp(item.value,unreadable?"Unconfirmed":expected));
 /* Both rows render through the real compact layout, including scrolling. */
 settings_render(0,4);settings_render(0,SETTINGS_TIMER_ROW);
}
static void cancel(unsigned row) {
 polls=input_count=0;tap(3,60,213);
 assert(settings_activate(0,row)==T5_APP_SETTING_NO_CHANGE);
}
int main(void) {
 begin();root(SETTINGS_SLEEP_DEFAULT,PORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS,false);
 cancel(4);assert(sleep_choice==SETTINGS_SLEEP_DEFAULT);
 cancel(SETTINGS_TIMER_ROW);assert(timer_choice==PORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS*1000u);
 assert(!record_writes[0]&&!record_writes[1]);end();
 for(unsigned mode=0;mode<3;mode++){
  set_mode(mode);set_idle(60);begin();root(mode,60,false);
  cancel(4);assert(sleep_choice==mode);cancel(SETTINGS_TIMER_ROW);assert(timer_choice==60000);end();
 }
 for(unsigned fault=0;fault<3;fault++){
  set_mode(1);set_idle(60);
  if(fault==0){records[0][0]=0;records[1][0]=0;}
  else record_status[0]=record_status[1]=fault==1?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_NOT_FOUND;
  begin();root(SETTINGS_SLEEP_DEFAULT,PORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS,fault!=2);
  cancel(4);assert(sleep_choice==SETTINGS_SLEEP_DEFAULT);
  if(fault==0)assert(!strcmp(sleep_message,"Invalid: default shown"));
  cancel(SETTINGS_TIMER_ROW);assert(timer_choice==PORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS*1000u);end();
 }
 assert(!record_writes[0]&&!record_writes[1]);
 for(unsigned mode=0;mode<3;mode++){
  record_status[0]=RISC_KEY_VALUE_NOT_FOUND;begin();
  tap(3,100,80+38*mode);tap(7,170,213);
  uint8_t result=settings_activate(0,4);
  if(result!=T5_APP_SETTING_UPDATED||records[0][2]!=mode)fprintf(stderr,"save mode=%u result=%u choice=%u stored=%u polls=%u writes=%u failed=%u status=%s\n",mode,result,sleep_choice,records[0][2],polls,record_writes[0],failed,sleep_message);
  assert(result==T5_APP_SETTING_UPDATED&&records[0][2]==mode);end();
  begin();root(mode,PORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS,false);end();
 }
 /* Saving changes only idle, and a new invocation reads the verified value. */
 set_idle(60);begin();tap(3,185,105);tap(7,170,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW)==T5_APP_SETTING_UPDATED);end();
 begin();root(PORTABLE_SLEEP_HYBRID,65,false);end();
 /* Draft cancellation and failed writes never install the proposed value. */
 unsigned before=record_writes[1];begin();tap(3,185,105);tap(7,60,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW)==T5_APP_SETTING_NO_CHANGE);end();
 assert(record_writes[1]==before);begin();root(PORTABLE_SLEEP_HYBRID,65,false);end();
 begin();write_failure=true;tap(3,185,105);tap(7,170,213);tap(11,60,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW)==T5_APP_SETTING_NO_CHANGE&&timer_choice==65000);end();
 begin();root(PORTABLE_SLEEP_HYBRID,65,false);end();
 /* A committed IO result is accepted only after independent readback; failed
  * readback must not display Saved even if the write may have persisted. */
 set_idle(60);begin();committed_error=true;tap(3,185,105);tap(7,170,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW)==T5_APP_SETTING_UPDATED);end();
 begin();root(PORTABLE_SLEEP_HYBRID,65,false);end();
 begin();verification_error=true;tap(3,185,105);tap(7,170,213);tap(11,60,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW)==T5_APP_SETTING_NO_CHANGE);
 assert(!strcmp(timer_message,"Save unconfirmed - retry"));end();
 begin();root(PORTABLE_SLEEP_HYBRID,70,false);end();
 const unsigned bounds[]={5,3600};
 for(unsigned i=0;i<2;i++){
  set_idle(bounds[i]);begin();tap(3,i?185:40,105);tap(7,170,213);
  assert(settings_activate(0,SETTINGS_TIMER_ROW)==T5_APP_SETTING_UPDATED&&timer_choice==bounds[i]*1000u);end();
 }
#ifdef PORTABLE_SETTINGS_BOTH_TIMERS
 /* Existing controller edits independent subsequent interval, not idle. */
 unsigned idle_writes=record_writes[1];
 for(unsigned fault=0;fault<3;fault++) {
  record_status[2]=fault==0?RISC_KEY_VALUE_OK:fault==1?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_NOT_FOUND;
  record_sizes[2]=5;memset(records[2],0,5);begin();
  cancel(SETTINGS_TIMER_ROW+1);assert(timer_deep&&timer_choice==300000);end();
 }

 record_status[2]=RISC_KEY_VALUE_NOT_FOUND;begin();
 t5_app_setting_t item={0};assert(settings_get(0,SETTINGS_TIMER_ROW+1,&item));
 assert(!strcmp(item.label,"Deep sleep timer")&&!strcmp(item.value,"300 seconds"));
 cancel(SETTINGS_TIMER_ROW+1);assert(timer_deep&&timer_choice==300000);end();
 begin();tap(3,185,105);tap(7,170,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW+1)==T5_APP_SETTING_UPDATED);end();
 assert(record_writes[1]==idle_writes&&record_writes[2]==1);
 begin();assert(settings_get(0,SETTINGS_TIMER_ROW+1,&item)&&!strcmp(item.value,"360 seconds"));
 cancel(SETTINGS_TIMER_ROW+1);assert(timer_choice==360000);end();
 unsigned deep_writes=record_writes[2];begin();tap(3,185,105);tap(7,60,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW+1)==T5_APP_SETTING_NO_CHANGE);end();
 assert(record_writes[2]==deep_writes&&record_writes[1]==idle_writes);
 begin();write_failure=true;tap(3,185,105);tap(7,170,213);tap(11,60,213);
 assert(settings_activate(0,SETTINGS_TIMER_ROW+1)==T5_APP_SETTING_NO_CHANGE&&timer_choice==360000);end();
 begin();assert(settings_get(0,SETTINGS_TIMER_ROW+1,&item)&&!strcmp(item.value,"360 seconds"));end();
 const unsigned deep_bounds[]={60,3600};
 for(unsigned i=0;i<2;i++) {
  unsigned seconds=deep_bounds[i];record_status[2]=RISC_KEY_VALUE_OK;record_sizes[2]=5;
  records[2][0]=0x54;records[2][1]=1;records[2][2]=seconds;records[2][3]=seconds>>8;
  records[2][4]=records[2][2]^records[2][3]^0xa5u;
  begin();tap(3,i?185:40,105);tap(7,170,213);
  assert(settings_activate(0,SETTINGS_TIMER_ROW+1)==T5_APP_SETTING_UPDATED&&timer_choice==seconds*1000u);end();
 }
 assert(record_writes[1]==idle_writes);
#endif
 printf("Settings defaults mode=%u idle=%us: root/editor reads, all saved modes, valid saved 60s precedence, malformed/read failure, cancellation, verified saves, failed save, bounds and clean repeated invocation PASS\n",(unsigned)SETTINGS_SLEEP_DEFAULT,(unsigned)PORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS);
 return 0;
}
