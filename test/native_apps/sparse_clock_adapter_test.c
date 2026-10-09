/* Production adapter + deterministic provider fixtures. No native provider,
 * Runtime promotion or hardware execution is claimed by this test. */
#include <stdlib.h>
static void *fixture_malloc(size_t);
#define malloc fixture_malloc
#include "../../lib/PortableApps/src/adapter.c"
#undef malloc
#include <assert.h>
#include <stdio.h>
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned calls,acquires,releases,live,subscriptions,touch_polls,nav_polls;
static unsigned frame_count,submits,waits,health_reads,kv_reads,radio_calls,sleep_calls,mode_calls;
static unsigned now,fail_acquire,fail_release,fail_case,acquire_per_kind[9],sleep_status;
static bool uncertain,partial,held,home_held,replay,nav_held,async_display;
static uint8_t framebuffer[800*480/8],saved_history[800*480/8];
static unsigned present_checks,allocations,fail_allocation;
static void *fixture_malloc(size_t n){return ++allocations==fail_allocation?NULL:malloc(n);}
static void io(void){assert(!uncertain);++calls;}
static bool fx_health(risc_runtime_health_v1 *out){io();health_reads++;out->uptime_ms=now;if(fail_case==10){uncertain=true;return false;}return true;}
static void fx_yield(uint32_t n){io();now+=n;}
static bool fx_diagnostic(const char *line){io();(void)line;return true;}
static bool fx_launch(const char *name){io();(void)name;return true;}
static bool fx_info(void *c,risc_display_info_v1 *out){
 io();(void)c;*out=(risc_display_info_v1){.width=800,.height=480,
 .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),
 .flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT};
 if(async_display)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;
 if(fail_case==1)out->width=0;
 return true;
}
static bool fx_frame(void *c,uint32_t format,risc_display_surface_v1 *out){
 io();(void)c;assert(!frame_count);frame_count=1;
 *out=(risc_display_surface_v1){.frame=9,.pixels=framebuffer,.width=800,.height=480,
 .stride_bytes=100,.size_bytes=sizeof(framebuffer),.pixel_format=format};return true;
}
static void fx_frame_release(void *c,risc_display_frame_v1 frame){io();(void)c;assert(frame==9&&frame_count);frame_count=0;}
static bool fx_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 io();(void)c;(void)r;(void)n;(void)options;assert(frame_count&&frame==9);frame_count=0;
 *token=++submits;present_checks=0;return true;
}
static bool fx_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 io();(void)c;assert(token==submits);
 out->state=++present_checks<25?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool fx_wait(void *c,risc_display_present_token_v1 token,uint32_t timeout,risc_display_present_status_v1 *out){
 io();(void)c;assert(token==submits&&timeout);waits++;now+=90;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool fx_seed(void *c,risc_display_frame_v1 frame){io();(void)c;assert(frame_count&&frame==9);if(fail_case==7){uncertain=true;return false;}return true;}
static risc_display_output_api_v1_history fx_display={.base={
 .api_version=1,.struct_size=sizeof(fx_display),.get_info=fx_info,.acquire=fx_frame,
 .release=fx_frame_release,.submit=fx_submit,.present_status=fx_present,.wait_present=fx_wait},.extension_tag=RISC_DISPLAY_HISTORY_TAG,.extension_version=1,.seed_previous=fx_seed};
static uint64_t fx_subscribe(void *c){io();(void)c;if(fail_case==2){uncertain=true;return 0;}subscriptions++;return 3;}
static bool fx_unsubscribe(void *c,uint64_t s){io();(void)c;assert(s==3&&subscriptions);if(fail_case==3){uncertain=true;return false;}subscriptions--;return true;}
static bool fx_touch_poll(void *c,size_t n){io();(void)c;assert(n==1&&subscriptions);touch_polls++;return true;}
static int32_t fx_touch_next(void *c,uint64_t s,risc_touch_event_v1 *out){
 io();(void)c;assert(s==3);if(replay){replay=false;*out=(risc_touch_event_v1){.id=0,.kind=RISC_TOUCH_EVENT_BUTTON_DOWN};return 1;}return 0;
}
static bool fx_snapshot(void *c,risc_touch_snapshot_v1 *out){
 io();(void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(held){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=200,.y=300};}
 if(home_held)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
 return true;
}
static const risc_touch_api_v1 fx_touch={1,sizeof(fx_touch),NULL,fx_subscribe,fx_unsubscribe,fx_touch_poll,fx_touch_next,fx_snapshot};
static bool fx_navigation_poll(void *c,risc_input_navigation_frame_v1 *out){io();(void)c;nav_polls++;*out=(risc_input_navigation_frame_v1){.buttons=nav_held?RISC_NAV_HOME:0,.pressed=nav_held?RISC_NAV_HOME:0};return true;}
static bool fx_foreground(void *c,const risc_input_foreground_v1 *claims,size_t count){io();(void)c;(void)claims;(void)count;if(fail_case==4){uncertain=true;return false;}return true;}
static bool fx_reset(void *c){io();(void)c;if(fail_case==5){uncertain=true;return false;}return true;}
static const risc_input_navigation_api_v1 fx_navigation={1,sizeof(fx_navigation),NULL,fx_navigation_poll,fx_foreground,fx_reset};
static bool fx_battery(void *c,risc_battery_sample_v1 *out){io();(void)c;*out=(risc_battery_sample_v1){3900,50,0};return true;}
static const risc_battery_gauge_api_v1 fx_gauge={1,sizeof(fx_gauge),NULL,fx_battery};
static int32_t fx_alarm_status(void *c,alarm_status_v1 *out){io();(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t fx_alarm_step(void *c){io();(void)c;return ALARM_OK;}
static int32_t fx_alarm_ack(void *c,const alarm_token_v1 *t){io();(void)c;(void)t;return ALARM_OK;}
static int32_t fx_alarm_prepare(void *c,alarm_sleep_v1 *s){io();(void)c;(void)s;return ALARM_OK;}
static int32_t fx_alarm_stop(void *c){io();(void)c;if(fail_case==6){uncertain=true;return ALARM_OUTPUT;}return ALARM_OK;}
static const alarm_service_v1 fx_alarms={1,sizeof(fx_alarms),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_stop};
static int32_t fx_kv_get(void *c,const char *key,void *data,uint32_t cap,uint32_t *size){io();(void)c;(void)key;(void)data;(void)cap;kv_reads++;*size=0;if(fail_case==11&&!strcmp(key,PORTABLE_RADIO_KEY))return RISC_KEY_VALUE_IO;return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fx_kv_put(void *c,const char *key,const void *data,uint32_t size){io();(void)c;(void)key;(void)data;(void)size;return RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 fx_kv={1,sizeof(fx_kv),NULL,fx_kv_get,fx_kv_put};
static wifi_link_t fx_wifi_status(void *c){io();(void)c;radio_calls++;if(fail_case==11){uncertain=true;return WIFI_LINK_UP;}return WIFI_LINK_DOWN;}
static bool fx_wifi_off(void *c){io();(void)c;radio_calls++;return true;}
static const wifi_api_v1 fx_wifi={.api_version=1,.struct_size=sizeof(fx_wifi),.status=fx_wifi_status,.disconnect_checked=fx_wifi_off};
static bool fx_ble_enable(void *c,bool on){io();(void)c;(void)on;radio_calls++;return true;}
static bool fx_ble_status(void *c,uint8_t *out){io();(void)c;radio_calls++;*out=PORTABLE_BLUETOOTH_OFF;if(fail_case==12){uncertain=true;*out=PORTABLE_BLUETOOTH_ON;}return true;}
static const portable_bluetooth_control_v1 fx_ble={.api_version=1,.struct_size=sizeof(fx_ble),.set_enabled=fx_ble_enable,.status=fx_ble_status};
static bool fx_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out){
 io();(void)id;assert(version==1&&out->struct_size==sizeof(*out));unsigned kind=9;
 const char *names[]={"display.output","alarm.service","input.touch.raw","input.navigation","board.battery","storage.key-value","net.wifi","bluetooth.hci","rtc.clock"};
 const void *apis[]={&fx_display,&fx_alarms,&fx_touch,&fx_navigation,&fx_gauge,&fx_kv,&fx_wifi,&fx_ble,NULL};
 for(unsigned i=0;i<9;i++)if(!strcmp(name,names[i]))kind=i;
 assert(kind<8);acquires++;acquire_per_kind[kind]++;
 if(acquires==fail_acquire){if(partial){out->api=apis[kind];out->slot=++live;}uncertain=true;return false;}
 out->api=apis[kind];out->slot=++live;return true;
}
static bool fx_release(risc_runtime_capability_v1 *grant){
 io();assert(grant->api&&live);releases++;
 if(releases==fail_release){uncertain=true;return false;}
 assert(grant->api!=&desk_wifi_control&&grant->api!=&desk_ble_control);
 live--;grant->api=NULL;return true;
}
static unsigned native_fences,clock_reads;
static bool clock_retained;
bool portable_desk_clock_time(uint8_t *h,uint8_t *m){clock_reads++;if(clock_retained){portable_desk_adapter_retain();return false;}*h=13;*m=42;return true;}

static bool fx_retain(void){native_fences++;return true;}
static const risc_runtime_api_v1 fx_runtime={.api_version=1,.struct_size=sizeof(fx_runtime),.health=fx_health,
 .yield_ms=fx_yield,.diagnostic=fx_diagnostic,.request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release,
 .retain_invocation=fx_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&fx_runtime:NULL;}
int portable_desk_clock_mode(void){mode_calls++;return 0;}
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a){
 io();assert(r==rt&&d==display&&!b&&a==alarms.api);assert(!subscriptions&&!navigation_ready);sleep_calls++;
 if(sleep_status==2){uncertain=true;return -2;}return 0;
}
static void assert_minimal(void){
 assert(acquire_per_kind[0]==1&&acquire_per_kind[1]==1);
 for(unsigned i=2;i<9;i++)assert(!acquire_per_kind[i]);
 assert(!subscriptions&&!navigation_ready&&!gauge&&!kv_reads&&!radio_calls&&!touch_polls&&!nav_polls&&!mode_calls);
 assert(paper_previous&&alarm_pixels&&display);
}
static void assert_fenced(void){
 unsigned before=calls;
 assert(portable_app_sleep_retained()&&!portable_desk_adapter_ready()&&native_fences==1);
 portable_desk_adapter_invalidate();portable_desk_adapter_begin();assert(!portable_desk_adapter_present(false));
 assert(portable_desk_adapter_seed()==0);assert(!portable_desk_adapter_sleep());
 assert(!portable_desk_adapter_foreground());assert(portable_desk_adapter_start(PORTABLE_DESK_START_TIMER)==-2);
 assert(portable_desk_adapter_upgrade(0)==-2);assert(!t5_app_get_api(1));assert(!t5_battery_get_api(1));
 t5_app_input_t input={0};assert(!poll(&input,2));t5_battery_state_t battery_state={0};assert(!read_battery(&battery_state));
 assert(millis_now()==0);input_service();app_module_fini();app_module_fini();
 assert(calls==before);
}
int main(int argc,char **argv){
 assert(argc>=2);unsigned scenario=(unsigned)atoi(argv[1]);
 assert(!portable_desk_adapter_ready());assert(app_module_init()==0);assert(!calls&&!acquires&&!health_reads);
 assert(!portable_desk_adapter_ready()&&!t5_app_get_api(1));
 if(scenario==0){app_module_fini();app_module_fini();assert(!calls);assert(!portable_desk_adapter_start(1));return 0;}
 if(scenario==1 || scenario==2){
  async_display=scenario==2;assert(portable_desk_adapter_start(1)==1);assert_minimal();
  unsigned before=calls;assert(portable_desk_adapter_start(1)==1);assert(!portable_desk_adapter_start(2));assert(!portable_desk_adapter_start(99));assert(!portable_desk_adapter_upgrade(-3));assert(calls==before);
  assert(paper_presentation_get());assert_minimal();
  portable_desk_adapter_begin();assert(portable_desk_adapter_seed()==1);assert(portable_desk_adapter_present(true));assert(paper_previous_valid);
  t5_app_input_t input={0};assert(poll(&input,30));assert(!input.buttons&&!input.tapped);assert_minimal();
  assert(portable_desk_adapter_sleep());assert(portable_desk_adapter_timer_only()&&sleep_calls==1);assert_minimal();
  app_module_fini();assert(!live&&!frame_count&&!subscriptions);before=calls;app_module_fini();assert(calls==before);return 0;
 }
 if(scenario==3 || scenario==4){
  assert(portable_desk_adapter_start(scenario==3?1:2)==1);
  if(scenario==3){
   portable_desk_adapter_begin();assert(portable_desk_adapter_seed()==1);
   unsigned before=calls;assert(!portable_desk_adapter_upgrade(0));assert(calls==before);
   assert(portable_desk_adapter_present(true));memcpy(saved_history,paper_previous,sizeof(saved_history));
   const void *mapping=display,*history=paper_previous;held=home_held=nav_held=replay=true;
   assert(portable_desk_adapter_upgrade(0)==1);assert(display==mapping&&paper_previous==history&&paper_previous_valid);
   assert(!memcmp(saved_history,paper_previous,sizeof(saved_history))&&acquire_per_kind[0]==1);
  }
  assert(!portable_desk_adapter_timer_only()&&quick.loaded&&desk_radios_loaded&&subscriptions==1);
  assert(acquire_per_kind[2]==1&&acquire_per_kind[3]==1&&acquire_per_kind[4]==1&&kv_reads&&radio_calls);
  unsigned before=calls;assert(portable_desk_adapter_upgrade(1)==1);assert(portable_desk_adapter_start(2)==1);assert(!portable_desk_adapter_start(1));assert(calls==before);
  held=home_held=nav_held=replay=true;input_service();assert(!input_sample.down&&!input_sample.released&&!home_pending&&!navigation_pending);
  input_service();assert(!input_sample.down&&!home_pending&&!navigation_pending);
  held=home_held=nav_held=false;input_service();assert(!input_sample.released&&!home_pending&&!navigation_pending);
  held=true;input_service();assert(input_sample.down&&input_sample.tap_eligible);
  held=false;input_service();assert(input_sample.released&&input_sample.tap_eligible);
  app_module_fini();assert(!live&&!subscriptions&&!frame_count);return 0;
 }
 if(scenario==5 || scenario==6){
  assert(argc==3);fail_acquire=(unsigned)atoi(argv[2]);partial=scenario==6;
  assert(portable_desk_adapter_start(2)==-2);assert(acquires==fail_acquire);assert_fenced();return 0;
 }
 if(scenario==7){
  assert(argc==3);fail_acquire=(unsigned)atoi(argv[2]);assert(fail_acquire>2);
  assert(portable_desk_adapter_start(1)==1);assert(portable_desk_adapter_upgrade(0)==-2);assert(acquires==fail_acquire);assert_fenced();return 0;
 }
 if(scenario==8){fail_case=1;assert(portable_desk_adapter_start(1)==0);assert(!native_sleep_retained);app_module_fini();assert(!live);return 0;}
 if(scenario==9){assert(argc==3);fail_case=(unsigned)atoi(argv[2]);assert(portable_desk_adapter_start(2)==-2);assert_fenced();return 0;}
 if(scenario==10){
  assert(argc==3);assert(portable_desk_adapter_start(2)==1);fail_release=releases+(unsigned)atoi(argv[2]);app_module_fini();assert(releases==fail_release);assert_fenced();return 0;
 }
 if(scenario==11){assert(portable_desk_adapter_start(1)==1);sleep_status=2;assert(!portable_desk_adapter_sleep());assert_fenced();return 0;}
 if(scenario==12){assert(portable_desk_adapter_start(1)==1);assert(portable_desk_adapter_upgrade(-4)==-2);uncertain=true;assert_fenced();return 0;}
 if(scenario==13){assert(portable_desk_adapter_start(2)==1);fail_case=3;app_module_fini();assert_fenced();return 0;}
 if(scenario==14){assert(portable_desk_adapter_start(2)==1);fail_case=4;app_module_fini();assert_fenced();return 0;}
 if(scenario==15){assert(portable_desk_adapter_start(2)==1);fail_case=6;failed=true;app_module_fini();assert_fenced();return 0;}
 if(scenario==16){assert(argc==3);fail_release=(unsigned)atoi(argv[2]);assert(portable_desk_adapter_start(2)==-2);assert(releases==fail_release);assert_fenced();return 0;}
 if(scenario==17){fail_allocation=1;assert(portable_desk_adapter_start(1)==0);assert(!native_sleep_retained);app_module_fini();assert(!live);return 0;}
 if(scenario==18){fail_allocation=2;assert(portable_desk_adapter_start(1)==1);assert(!paper_previous);portable_desk_adapter_begin();assert(portable_desk_adapter_seed()==0);assert(portable_desk_adapter_present(true));app_module_fini();assert(!live);return 0;}
 if(scenario==19){
  assert(portable_desk_adapter_start(1)==1);portable_desk_adapter_begin();fail_case=7;
  assert(portable_desk_adapter_seed()==-2);assert(frame_count&&desk_retained_surface.frame==9&&!surface.frame);
  memcpy(saved_history,framebuffer,sizeof(framebuffer));np_pixel(1,1,0,255);fill(0,0,10,10,0);
  assert(!memcmp(saved_history,framebuffer,sizeof(framebuffer)));assert_fenced();return 0;
 }
 if(scenario==20){
  assert(portable_desk_adapter_start(2)==1);quick_radios.valid=quick_radios.available=true;
  fail_acquire=acquires+3;
  assert(!desk_radios_apply(&quick_radios,&quick.ui,rt,PQA_WIFI));assert_fenced();return 0;
 }
 if(scenario==21){
  assert(portable_desk_adapter_start(1)==1);unsigned before=calls;
  assert(app_module_init()!=0);assert(portable_desk_adapter_timer_only()&&calls==before);
  app_module_fini();assert(!live);return 0;
 }
 if(scenario==22){
  assert(portable_desk_adapter_start(2)==1);risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
  assert(rt->acquire("net.wifi",1,15,&g));assert(g.api==&fx_wifi);assert(rt->release(&g));
  assert(rt->acquire("bluetooth.hci",1,16,&g));assert(g.api==&fx_ble);assert(rt->release(&g));
  app_module_fini();assert(!live);return 0;
 }
 if(scenario==23){assert(portable_desk_adapter_start(2)==1);fail_case=10;input_service();assert_fenced();return 0;}
 if(scenario==24){assert(portable_desk_adapter_start(2)==1);fail_case=3;assert(!portable_desk_adapter_sleep());assert_fenced();return 0;}
 if(scenario==25 || scenario==26){fail_case=scenario==25?11:12;assert(portable_desk_adapter_start(2)==-2);assert_fenced();return 0;}
 if(scenario==27 || scenario==28){
  assert(portable_desk_adapter_start(2)==1);unsigned before=acquires;char value[6];
  quick.hour_24=true;clock_retained=scenario==28;quick_time(value);
  assert(clock_reads==1&&acquires==before&&!strcmp(value,clock_retained?"--:--":"13:42"));
  if(clock_retained){uncertain=true;assert_fenced();}else {app_module_fini();assert(!live);}return 0;
 }
 assert(0);
}
