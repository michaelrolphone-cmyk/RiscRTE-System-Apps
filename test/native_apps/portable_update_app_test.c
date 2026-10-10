/* Real portable update app/controller, renderer and adapter. All providers fake;
 * never invokes a host radio, network, device or real user credentials. */
#define PORTABLE_UPDATE_APP
#ifndef PORTABLE_UPDATE_FIRMWARE
#define PORTABLE_UPDATE_FIRMWARE 0
#endif
#define PORTABLE_UPDATE_RTC_UTC_OFFSET_SECONDS 28800
#define PORTABLE_WIFI_INSTANCE 15u
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_INPUT_NAVIGATION
#define UPDATE_RETURN_APP "springboard.elf"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "../../lib/PortableApps/src/adapter.c"
#ifdef TEST_RADIO_POLICY
#define PORTABLE_QUICK_RADIOS
#endif
#include "../../Apps/update_portable.inc"

const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static char last_launch[64];
static unsigned ticks,grant_count,sub_count,frame_count,presents,poll_count,launches;
static unsigned connects,disconnects,scan_starts,scan_polls,scan_cancels,writes,sleeps,alarm_acks;
static unsigned fail_connect,fail_disconnect,fail_release,fail_scan,fail_scan_cancel,fail_poll,fail_store,fail_addresses;
static unsigned acquire_denied,kv_denied,fail_display,late_calls,terminal_sleep,service_calls,alarm_stops,fail_alarm_stop;
static bool uncertain_start,break_radio_on_yield,http_retained,bank_blocked_by_radio;
static unsigned script_count,limit_polls=500,scenario;
static bool native_active,scan_active;
static wifi_link_t fake_link;
static garden_radio_scan_result_v1 fake_scan;
#ifdef TEST_PAPER_UPDATE
#if PORTABLE_DISPLAY_ROTATION == 90
#define UW 800
#define UH 480
#else
#define UW 480
#define UH 800
#endif
#define UF RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1)
#define UI_FLAGS RISC_DISPLAY_INFO_RETAINS_IMAGE
#else
#define UW 240
#define UH 240
#define UF RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)
#define UI_FLAGS 0
#endif
static uint16_t pixels[UW*UH];
static alarm_status_v1 fake_alarm={.api_version=1,.struct_size=sizeof(fake_alarm),.state=ALARM_STATE_READY};
static jmp_buf blocked;
static bool block_expected,blocked_note;
static int sleep_outcome=1;
static struct {unsigned at;unsigned buttons;int x,y;bool navigation_delivered;} script[64];
static struct {char name[16];uint8_t data[64];uint32_t size;} cells[8];
static wifi_api_v1 radio_api;
static risc_runtime_api_v1 runtime_api;
/* Script steps follow the controller's 25 ms cadence, independently of raw
 * provider reads performed by wait slices or capture-only raster work. */
static unsigned input_step(void){return ticks/25u;}
static void observed(void){if(terminal_sleep)++late_calls;}
static bool fake_health(risc_runtime_health_v1 *h){h->uptime_ms=ticks;return (script_count?input_step():poll_count)<limit_polls;}
static void fake_yield(uint32_t ms){ticks+=ms;if(scenario>=24&&scenario<=25&&connects&&fake_link==WIFI_LINK_JOINING)fake_link=WIFI_LINK_UP;if(break_radio_on_yield){break_radio_on_yield=false;fake_link=WIFI_LINK_DOWN;fail_disconnect=1;}if(block_expected && blocked_note)longjmp(blocked,1);}
static bool fake_diag(const char *s){assert(!strstr(s,"testpass") && !strstr(s,"Fixture"));if(strstr(s,"retained"))blocked_note=true;return true;}
static bool fake_launch(const char *s){
#ifdef TEST_PAPER_UPDATE
assert(!strcmp(s,UPDATE_RETURN_APP)||!strcmp(s,"default.elf"));
#else
assert(!strcmp(s,UPDATE_RETURN_APP));
#endif
assert(!opened && !uwg.api && !ukg.api && !usg.api && !ucg.api && !native_active);snprintf(last_launch,sizeof(last_launch),"%s",s);++launches;return true;}
static bool fake_info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=UW,.height=UH,.supported_formats=UF,.flags=UI_FLAGS};return true;}
static bool fake_frame(void*c,uint32_t format,risc_display_surface_v1*out){(void)c;observed();assert(!frame_count);frame_count=1;*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=UW,.height=UH,.stride_bytes=format==RISC_DISPLAY_FORMAT_MONO1?(UW+7)/8:UW*2,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){(void)c;observed();assert(f==1 && frame_count);frame_count=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 (void)c;(void)r;(void)n;(void)o;observed();assert(f==1 && frame_count);if(fail_display)return false;frame_count=0;*t=++presents;
 const char *dir=getenv("PORTABLE_UPDATE_FRAME_DIR");if(dir && presents<=12){char path[512];snprintf(path,sizeof(path),"%s/update-%u-%u.ppm",dir,scenario,presents);FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n%d %d\n255\n",UW,UH);for(unsigned i=0;i<UW*UH;++i){unsigned v=surface_format==RISC_DISPLAY_FORMAT_MONO1?((((uint8_t*)pixels)[i/8]&(0x80u>>(i%8)))?0:65535):pixels[i];unsigned char rgb[]={(unsigned char)((v>>11)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};assert(fwrite(rgb,1,3,file)==3);}fclose(file);}return true;
}
static bool fake_present(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){(void)c;observed();assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present};
/* Strict raw provider: capture queues each edge, and snapshot describes the
 * same sequence. Snapshot-only mutations are not valid touch reports. */
static risc_touch_snapshot_v1 fake_touch_state;
static risc_touch_event_v1 fake_touch_events[RISC_TOUCH_QUEUE_LENGTH];
static unsigned fake_touch_head,fake_touch_count;
static bool fake_script_snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;memset(out,0,sizeof(*out));out->width=UW;out->height=UH;
#ifdef TEST_PAPER_UPDATE
out->width=480;out->height=800;
#endif
for(unsigned i=0;i<script_count;++i)if(script[i].at==input_step() && script[i].x>=0){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=script[i].x,.y=script[i].y};
#if PORTABLE_TOUCH_ROTATION == 180
 out->contacts[0].x=239-out->contacts[0].x;out->contacts[0].y=239-out->contacts[0].y;
#endif
 }return true;}
static void fake_touch_emit(unsigned kind,risc_touch_contact_v1 point){
 assert(fake_touch_count<RISC_TOUCH_QUEUE_LENGTH);
 fake_touch_events[(fake_touch_head+fake_touch_count++)%RISC_TOUCH_QUEUE_LENGTH]=(risc_touch_event_v1){
  .sequence=++fake_touch_state.sequence,.timestamp_ms=ticks,.kind=kind,.id=point.id,.x=point.x,.y=point.y};
}
static uint64_t fake_subscribe(void*c){observed();++sub_count;fake_touch_head=fake_touch_count=0;assert(fake_script_snapshot(c,&fake_touch_state));return 1;}
static bool fake_unsubscribe(void*c,uint64_t id){(void)c;observed();assert(id==1 && sub_count);--sub_count;return true;}
static bool fake_touch_poll(void*c,size_t n){
 observed();assert(n==1);++poll_count;risc_touch_snapshot_v1 next;assert(fake_script_snapshot(c,&next));
 bool old_down=fake_touch_state.contact_count!=0,new_down=next.contact_count!=0;
 bool same=old_down&&new_down&&fake_touch_state.contacts[0].id==next.contacts[0].id;
 if(old_down&&!same)fake_touch_emit(RISC_TOUCH_EVENT_UP,fake_touch_state.contacts[0]);
 if(new_down){
  if(!same)fake_touch_emit(RISC_TOUCH_EVENT_DOWN,next.contacts[0]);
  else if(memcmp(&fake_touch_state.contacts[0],&next.contacts[0],sizeof(next.contacts[0])))fake_touch_emit(RISC_TOUCH_EVENT_MOVE,next.contacts[0]);
 }
 next.sequence=fake_touch_state.sequence;next.timestamp_ms=ticks;fake_touch_state=next;return true;
}
static int32_t fake_next(void*c,uint64_t id,risc_touch_event_v1*e){
 (void)c;observed();assert(id==1);if(!fake_touch_count)return 0;
 *e=fake_touch_events[fake_touch_head];fake_touch_head=(fake_touch_head+1)%RISC_TOUCH_QUEUE_LENGTH;--fake_touch_count;return 1;
}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;observed();*out=fake_touch_state;return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_subscribe,fake_unsubscribe,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*out){
 (void)c;observed();*out=(risc_input_navigation_frame_v1){0};
 /* Raster capture can poll touch without reducing a navigation frame. Keep
  * each scripted navigation edge pending until its provider is polled. */
 for(unsigned i=0;i<script_count;++i)if(script[i].at<=input_step()&&script[i].buttons&&!script[i].navigation_delivered){
  out->pressed=out->released=script[i].buttons;script[i].navigation_delivered=true;break;
 }
 return true;
}
static bool fake_foreground(void*c,const risc_input_foreground_v1*a,size_t n){(void)c;(void)a;(void)n;observed();return true;}
static bool fake_reset(void*c){(void)c;observed();return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
static int32_t fake_get(void*c,const char*key,void*buf,uint32_t cap,uint32_t*size){(void)c;observed();*size=0;if(fail_store)return RISC_KEY_VALUE_IO;for(unsigned i=0;i<8;++i)if(!strcmp(cells[i].name,key)){if(cap<cells[i].size){*size=cells[i].size;return RISC_KEY_VALUE_BUFFER_SMALL;}memcpy(buf,cells[i].data,cells[i].size);*size=cells[i].size;return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*key,const void*buf,uint32_t n){(void)c;observed();++writes;if(fail_store)return RISC_KEY_VALUE_IO;assert(n<=64 && strlen(key)<=15);unsigned i=0;for(;i<8;++i)if(!cells[i].name[0] || !strcmp(cells[i].name,key))break;assert(i<8);strcpy(cells[i].name,key);memcpy(cells[i].data,buf,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,fake_get,fake_put};
static bool fake_connect(void*c,const char*s,const char*p){(void)c;observed();assert(!scan_active && !native_active);assert(s[0] && strlen(s)<=32 && (!p[0] || strlen(p)>=8));++connects;if(fail_connect){if(uncertain_start){native_active=true;fail_disconnect=1;}return false;}native_active=true;fake_link=WIFI_LINK_JOINING;return true;}
static void fake_disconnect_legacy(void*c){(void)c;assert(!"Legacy unchecked disconnect must never be used");}
static bool fake_disconnect(void*c){(void)c;observed();if(http_retained)return false;++disconnects;if(fail_disconnect)return false;native_active=scan_active=false;fake_link=WIFI_LINK_DOWN;return true;}
static wifi_link_t fake_status(void*c){(void)c;observed();return fake_link;}
static int8_t fake_rssi(void*c){(void)c;observed();return -43;}
static bool fake_addresses(void*c,wifi_ipv4_v1*s,wifi_ipv4_v1*a){(void)c;observed();if(!s || !a)return false;if(fail_addresses){if(uncertain_start)fail_disconnect=1;return false;}*a=(wifi_ipv4_v1){0};*s=(wifi_ipv4_v1){.address={192,0,2,10}};return fake_link==WIFI_LINK_UP;}
static bool fake_scan_start(void*c){(void)c;observed();++scan_starts;assert(!native_active);if(fail_scan){if(uncertain_start){native_active=scan_active=true;fail_disconnect=1;}return false;}native_active=scan_active=true;return true;}
static bool fake_scan_poll(void*c,garden_radio_scan_result_v1*out){(void)c;observed();++scan_polls;assert(out->struct_size>=sizeof(*out));if(fail_poll)return false;*out=fake_scan;return true;}
static bool fake_scan_cancel(void*c){(void)c;observed();++scan_cancels;if(fail_scan_cancel)return false;scan_active=native_active=false;return true;}
static int32_t fake_alarm_status(void*c,alarm_status_v1*out){(void)c;observed();assert(!cleanup_pending);++service_calls;*out=fake_alarm;return ALARM_OK;}
static int32_t fake_alarm_step(void*c){(void)c;observed();assert(!cleanup_pending);++service_calls;if(fake_alarm.state==ALARM_STATE_DISMISSING){fake_alarm.state=ALARM_STATE_READY;fake_alarm.occurrence=(alarm_token_v1){0};}return ALARM_OK;}
static int32_t fake_alarm_refresh(void*c){(void)c;return ALARM_OK;}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){(void)c;assert(t->generation==fake_alarm.occurrence.generation);++alarm_acks;fake_alarm.state=ALARM_STATE_DISMISSING;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*out){(void)c;(void)out;return ALARM_OK;}
static int32_t fake_alarm_stop(void*c){(void)c;observed();++alarm_stops;return fail_alarm_stop?ALARM_PENDING:ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_refresh,fake_alarm_ack,fake_alarm_prepare,fake_alarm_stop};

static unsigned checks,begins,activations,restarts,service_cancels;
static bool fail_cancel,fail_service_status,unknown_activation;
static software_update_status_v1 mock_update={.struct_size=sizeof(mock_update),.state=SOFTWARE_UPDATE_IDLE};
static bool update_refresh(void*c,uint64_t utc){(void)c;assert(utc>1704067200ULL);++checks;mock_update.state=SOFTWARE_UPDATE_CATALOG;mock_update.resources_open=true;return true;}
static bool update_step(void*c){(void)c;if(scenario>=24&&scenario<=25){if(mock_update.state==SOFTWARE_UPDATE_PREPARING&&input_step()>=10)mock_update.state=SOFTWARE_UPDATE_VERIFYING;if(mock_update.state==SOFTWARE_UPDATE_VERIFYING&&input_step()>=12)mock_update.state=SOFTWARE_UPDATE_READY;}return true;}
static bool update_status(void*c,software_update_status_v1*out){(void)c;if(fail_service_status)return false;*out=mock_update;return true;}
static bool update_get(void*c,uint32_t i,software_update_row_v1*out){(void)c;assert(i<mock_update.count);strcpy(out->id,"clock");strcpy(out->version,"1.1.0");strcpy(out->installed,"1.0.0");strcpy(out->reason,"Update available");out->availability=SOFTWARE_UPDATE_AVAILABLE;return true;}
static bool update_begin(void*c,uint32_t i,uint64_t utc){(void)c;assert(i<mock_update.count&&utc>1704067200ULL);++begins;mock_update.state=SOFTWARE_UPDATE_PREPARING;mock_update.resources_open=true;return true;}
static bool update_cancel(void*c){(void)c;++service_cancels;if(fail_cancel||http_retained||(bank_blocked_by_radio&&native_active))return false;mock_update.state=mock_update.count?SOFTWARE_UPDATE_LIST:SOFTWARE_UPDATE_IDLE;mock_update.resources_open=false;return true;}
static bool update_activate(void*c){(void)c;assert(!native_active&&!uwg.api);assert(mock_update.state==SOFTWARE_UPDATE_READY);if(unknown_activation){mock_update.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;return false;}++activations;mock_update.state=SOFTWARE_UPDATE_ACTIVATED;return true;}
static bool update_restart(void*c){(void)c;assert(activations);++restarts;return false;}
static const software_update_v1 update_api={1,sizeof(update_api),NULL,update_refresh,update_step,update_status,update_get,update_begin,update_cancel,update_activate,update_restart};
static bool rtc_read(void*c,twatch_rtc_time_v1*out){(void)c;*out=(twatch_rtc_time_v1){2026,10,4,0,12,0,0};return true;}
static const twatch_rtc_api_v1 rtc_api={2,sizeof(rtc_api),NULL,rtc_read,NULL,NULL,NULL};

static bool fake_acquire(const char*name,uint32_t v,uint64_t id,risc_runtime_capability_v1*out){
 observed();assert(out->struct_size==sizeof(*out) && (v==1||v==2));if(!strcmp(name,"net.wifi")){assert(id==15);if(acquire_denied)return false;out->api=&radio_api;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(id==6
#if defined(TEST_RADIO_POLICY) || defined(PORTABLE_QUICK_ACTIONS)
 || id==1
#endif
 );if(kv_denied)return false;out->api=&kv_api;}
 else {assert(!id);if(!strcmp(name,PORTABLE_UPDATE_FIRMWARE?SOFTWARE_UPDATE_FIRMWARE_CAPABILITY:SOFTWARE_UPDATE_APPS_CAPABILITY))out->api=&update_api;else if(!strcmp(name,TWATCH_RTC_CAPABILITY))out->api=&rtc_api;else if(!strcmp(name,"display.output"))out->api=&display_api;else if(!strcmp(name,"input.touch.raw"))out->api=&touch_api;else if(!strcmp(name,"input.navigation"))out->api=&nav_api;else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))out->api=&alarm_api;else return false;}
 ++grant_count;return true;
}
static bool fake_release(risc_runtime_capability_v1*g){observed();assert(g->api && grant_count);if(g->api==&radio_api){assert(!radio_owned || !native_active || fail_disconnect);if(fail_release)return false;}--grant_count;g->api=NULL;return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime_api:NULL;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!native_active && !scan_active && !uwg.api && !uw && !sub_count && !surface.frame);++sleeps;if(sleep_outcome==-2)terminal_sleep=1;return sleep_outcome;}
static void event(unsigned at,unsigned buttons,int x,int y){assert(script_count<64);script[script_count].at=at;script[script_count].buttons=buttons;script[script_count].x=x;script[script_count++].y=y;}

static void start_fixture(void){
 runtime_api=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime_api),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=fake_acquire,.release=fake_release};
 radio_api=(wifi_api_v1){.api_version=1,.struct_size=sizeof(radio_api),.connect=fake_connect,.disconnect=fake_disconnect_legacy,.status=fake_status,.rssi=fake_rssi,.addresses=fake_addresses,.scan_start=fake_scan_start,.scan_poll=fake_scan_poll,.scan_cancel=fake_scan_cancel,.disconnect_checked=fake_disconnect};
 assert(app_module_init()==0);assert(open_update());
 portable_wifi_credentials saved={0};strcpy(saved.ssid,"Fixture network");strcpy(saved.password,"testpass123");assert(portable_wifi_credentials_save(&kv_api,&saved)==PORTABLE_WIFI_CREDENTIALS_LOADED);portable_wifi_credentials_clear(&saved);writes=0;
}
static void connect_fixture(bool install){assert(connect_saved(install));assert(connecting&&native_active);fake_link=WIFI_LINK_UP;update_tick();assert(!connecting);}
int main(int argc,char**argv){assert(argc==2);scenario=(unsigned)atoi(argv[1]);start_fixture();
 switch(scenario){
 case 40:
  ust.state=SOFTWARE_UPDATE_IDLE;assert(portable_update_idle_ready());
  connecting=true;assert(!portable_update_idle_ready());connecting=false;
  radio_owned=true;assert(!portable_update_idle_ready());radio_owned=false;
  confirming=true;assert(!portable_update_idle_ready());confirming=false;
  restart_pending=true;assert(!portable_update_idle_ready());restart_pending=false;
  install_after_connect=true;assert(!portable_update_idle_ready());install_after_connect=false;
  cleanup_pending=true;assert(!portable_update_idle_ready());cleanup_pending=false;
  ust.state=SOFTWARE_UPDATE_DOWNLOAD;assert(!portable_update_idle_ready());
  ust.state=SOFTWARE_UPDATE_LIST;assert(portable_update_idle_ready());break;
 case 0:connect_fixture(false);assert(checks==1&&!begins);mock_update.state=SOFTWARE_UPDATE_LIST;mock_update.count=1;update_tick();assert(!uwg.api&&!native_active);break;
 case 1:connect_fixture(false);fail_cancel=true;http_retained=true;unsigned before_disconnect=disconnects;assert(!portable_update_suspend()&&native_active&&uwg.api);assert(!portable_update_services_safe()&&disconnects==before_disconnect);fail_cancel=false;http_retained=false;assert(portable_update_suspend()&&!native_active);break;
 case 2:connect_fixture(false);fail_disconnect=1;assert(!portable_update_suspend()&&native_active&&cleanup_pending);unsigned before=service_calls;bool consumed=false;assert(alarm_foreground(&consumed)&&before==service_calls);fail_disconnect=0;assert(portable_update_suspend());break;
 case 3:assert(connect_saved(false));ticks+=30001;update_tick();assert(!connecting&&!native_active&&!checks);break;
 case 4:fail_connect=1;uncertain_start=true;assert(!connect_saved(false)&&cleanup_pending&&native_active);fail_disconnect=0;assert(portable_update_suspend());break;
 case 5:mock_update.count=1;selected=1;connect_fixture(true);mock_update.state=SOFTWARE_UPDATE_READY;update_tick();assert(activations==1&&restart_pending&&!native_active&&!uwg.api);assert(!portable_update_suspend());restart_pending=false;mock_update.state=SOFTWARE_UPDATE_LIST;break;
 case 6:connect_fixture(false);render_update();ticks+=60001;t5_app_input_t input;assert(poll(&input,25)&&sleeps==1&&!native_active&&!uwg.api);break;
 case 7:connect_fixture(false);render_update();sleep_outcome=-2;ticks+=60001;t5_app_input_t input7;assert(!poll(&input7,25)&&native_sleep_retained);unsigned old=grant_count;app_module_fini();assert(!late_calls&&old==grant_count);terminal_sleep=0;native_sleep_retained=false;failed=false;break;
 case 8:connect_fixture(false);render_update();fail_display=1;render_update();t5_app_input_t input8;assert(!poll(&input8,25)&&!native_active&&!uwg.api);fail_display=0;break;
 case 9:fake_link=WIFI_LINK_UP;native_active=true;assert(!connect_saved(false));assert(!connects&&!disconnects&&native_active&&!uwg.api);native_active=false;fake_link=WIFI_LINK_DOWN;break;
 case 10:assert(portable_update_close());event(input_step()+3,T5_APP_BUTTON_CONFIRM,-1,0);event(input_step()+6,T5_APP_BUTTON_BACK,-1,0);app_main();assert(!opened&&launches==1&&!native_active&&!begins);break;
 case 11:mock_update.count=1;mock_update.state=SOFTWARE_UPDATE_LIST;selected=1;ust=mock_update;selected_row=(software_update_row_v1){.struct_size=sizeof(selected_row)};assert(update_get(NULL,0,&selected_row));confirming=true;confirm_choice=0;render_update();assert(!begins&&!activations);break;
 case 12:connect_fixture(false);fail_cancel=true;http_retained=true;block_expected=true;if(!setjmp(blocked)){app_module_fini();assert(!"Expected retained invocation");}block_expected=false;assert(native_active&&usg.api&&ukg.api);fail_cancel=false;http_retained=false;break;
 case 13:connect_fixture(false);render_update();fake_alarm.state=ALARM_STATE_ALERT;fake_alarm.occurrence=(alarm_token_v1){1,1,1,1};break_radio_on_yield=true;bool alarm=false;assert(alarm_foreground(&alarm)&&alarm&&cleanup_pending&&alarm_stops);fail_disconnect=0;assert(portable_update_suspend());fake_alarm.state=ALARM_STATE_READY;break;
 case 14:mock_update.count=1;selected=1;connect_fixture(true);mock_update.state=SOFTWARE_UPDATE_READY;unknown_activation=true;update_tick();assert(!activations&&restart_pending&&ust.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN);assert(!portable_update_services_safe()&&!portable_update_suspend());restart_pending=false;mock_update.state=SOFTWARE_UPDATE_LIST;break;
 case 15:assert(connect_saved(false));fake_link=WIFI_LINK_DOWN;fail_disconnect=1;unsigned joining_before=service_calls;bool joining_alarm=false;assert(alarm_foreground(&joining_alarm)&&cleanup_pending&&service_calls==joining_before);fail_disconnect=0;assert(portable_update_suspend());break;
 case 16:connect_fixture(false);fail_addresses=1;fail_disconnect=1;unsigned address_before=service_calls;bool address_alarm=false;assert(alarm_foreground(&address_alarm)&&cleanup_pending&&service_calls==address_before);fail_disconnect=0;assert(portable_update_suspend());break;
 case 17:connect_fixture(false);bank_blocked_by_radio=true;fail_disconnect=1;assert(!portable_update_suspend()&&native_active&&cleanup_pending);unsigned old_cancels=service_cancels;fail_disconnect=0;assert(portable_update_suspend()&&!native_active&&!cleanup_pending&&!mock_update.resources_open&&service_cancels==old_cancels+2);break;
 case 18:connect_fixture(false);fail_cancel=true;assert(!portable_update_suspend()&&!native_active&&!uwg.api&&usg.api&&cleanup_pending);assert(!portable_update_suspend()&&usg.api);fail_cancel=false;assert(portable_update_suspend());break;
 case 19:connect_fixture(false);fail_release=1;assert(!portable_update_suspend()&&!native_active&&uwg.api&&cleanup_pending);fail_release=0;assert(portable_update_suspend()&&!uwg.api);break;
 case 20:mock_update.count=1;mock_update.state=SOFTWARE_UPDATE_LIST;ust=mock_update;selected=1;message="Select a release; Check refreshes";render_update();break;
 case 21:mock_update.count=1;selected_row=(software_update_row_v1){.struct_size=sizeof(selected_row)};assert(update_get(NULL,0,&selected_row));confirming=true;confirm_choice=0;message="Confirm this selected update";render_update();break;
 case 22:ust.state=SOFTWARE_UPDATE_DOWNLOAD;ust.done=14336;ust.total=32768;message="Inactive bank; Cancel keeps current";render_update();break;
 case 23:ust.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;restart_pending=true;message="Activation uncertain; restart required";render_update();restart_pending=false;break;
 case 24:case 25:assert(portable_update_close());mock_update.count=1;mock_update.state=SOFTWARE_UPDATE_LIST;limit_polls=18;
 event(2,T5_APP_BUTTON_DOWN,-1,0);event(4,T5_APP_BUTTON_CONFIRM,-1,0);event(6,T5_APP_BUTTON_DOWN,-1,0);event(8,T5_APP_BUTTON_CONFIRM,-1,0);
 event(12,scenario==24?T5_APP_BUTTON_BACK:T5_APP_BUTTON_CONFIRM,-1,0);if(scenario==25)event(14,T5_APP_BUTTON_BACK,-1,0);
 app_main();assert(begins==1&&!activations&&!restart_pending&&launches==1&&!opened&&!native_active);break;
 case 26:case 27:assert(portable_update_close());mock_update.count=scenario==27?1:0;mock_update.state=SOFTWARE_UPDATE_DOWNLOAD;mock_update.done=14336;mock_update.total=32768;mock_update.resources_open=true;
 event(2,0,120,225);event(16,T5_APP_BUTTON_BACK,-1,0);app_main();
 assert(mock_update.state==(scenario==27?SOFTWARE_UPDATE_LIST:SOFTWARE_UPDATE_IDLE));assert(!uv.detail&&uv.count==mock_update.count+1&&!strcmp(uv.rows[0],"Check / retry"));
 assert(!strncmp(uv.message,"Cancelled",9)&&presents>=3&&!begins&&!activations&&launches==1);break;
 case 28:mock_update.state=SOFTWARE_UPDATE_DOWNLOAD;ust=mock_update;render_update();assert(!dirty);mock_update.state=SOFTWARE_UPDATE_IDLE;
 assert(portable_update_services_safe()&&dirty);render_update();assert(!uv.detail&&!strcmp(uv.rows[0],"Check / retry"));break;
 case 29:render_update();fail_service_status=true;assert(!portable_update_services_safe()&&dirty&&cleanup_pending);render_update();assert(!strcmp(uv.rows[0],"Retry cleanup"));fail_service_status=false;assert(portable_update_suspend());break;
 case 30:ust.state=SOFTWARE_UPDATE_DOWNLOAD;fail_service_status=true;assert(!portable_update_suspend()&&cleanup_pending&&dirty);render_update();assert(!strcmp(uv.rows[0],"Retry cleanup"));fail_service_status=false;assert(portable_update_suspend()&&ust.state==SOFTWARE_UPDATE_IDLE);break;
 case 31:ust.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;restart_pending=true;message="Activation uncertain; restart required";render_update();assert(!portable_update_suspend());assert(!strcmp(message,"Activation uncertain; restart required"));render_update();assert(!strcmp(uv.message,"Activation uncertain; restart required"));restart_pending=false;break;
 case 32:{render_update();assert(!strcmp(uv.message,"Saved Wi-Fi; Check to connect"));int pen=0;for(const char *s=uv.message;*s;++s){unsigned ch=(unsigned char)*s;assert(ch>=32&&ch<=126);const rps_glyph *g=&rps_text[3*95+ch-32];assert(pen/16+g->advance_q4/16<=204);pen+=g->advance_q4;}break;}
 case 33:mock_update.state=SOFTWARE_UPDATE_RETAINED;ust=mock_update;dirty=false;assert(!portable_update_services_safe()&&cleanup_pending&&dirty);render_update();assert(!strcmp(uv.rows[0],"Retry cleanup")&&!strcmp(uv.message,"Cleanup required; retry Back"));mock_update.state=SOFTWARE_UPDATE_IDLE;assert(portable_update_suspend());break;
 case 34: {
  mock_update.count=5;mock_update.state=SOFTWARE_UPDATE_LIST;ust=mock_update;selected=3;render_update();
#ifdef PORTABLE_NOVA_UI
  assert(portable_wifi_hit(&uv,16,62)==2 && portable_wifi_hit(&uv,223,115)==2);
  assert(portable_wifi_hit(&uv,16,120)==3 && portable_wifi_hit(&uv,223,173)==3);
  assert(portable_wifi_hit(&uv,15,62)==-1 && portable_wifi_hit(&uv,224,120)==-1);
#endif
  assert(!begins&&!activations);break;
 }
 case 35: {
  assert(portable_update_close());mock_update.count=1;mock_update.state=SOFTWARE_UPDATE_LIST;
  event(2,T5_APP_BUTTON_DOWN,-1,0);event(4,T5_APP_BUTTON_CONFIRM,-1,0);
  event(6,0,30,25);event(14,T5_APP_BUTTON_BACK,-1,0);app_main();
  assert(!begins&&!activations&&!opened&&launches==1);break;
 }
 case 36: {
  mock_update.count=1;mock_update.state=SOFTWARE_UPDATE_LIST;ust=mock_update;selected=1;
  selected_row=(software_update_row_v1){.struct_size=sizeof(selected_row)};assert(update_get(NULL,0,&selected_row));
  confirming=true;confirm_choice=1;render_update();assert(!begins&&!activations);
  assert(!strcmp(uv.rows[1],"Install and restart"));break;
 }
 case 37: {
  assert(portable_update_close());mock_update.count=1;mock_update.state=SOFTWARE_UPDATE_LIST;
#ifdef PORTABLE_NOVA_UI
  event(2,0,192,200);event(8,0,120,200);
#else
  event(2,0,192,225);event(8,0,120,225);
#endif
  event(14,T5_APP_BUTTON_BACK,-1,0);event(18,T5_APP_BUTTON_BACK,-1,0);app_main();
  assert(selected==1&&!begins&&!activations&&!opened&&launches==1);break;
 }
#ifdef TEST_RADIO_POLICY
 case 48:{uint8_t r[]={0x51,1,0,0xa5};assert(fake_put(NULL,PORTABLE_RADIO_KEY,r,4)==0);assert(!connect_saved(false)&&!connects&&strstr(message,"off"));r[2]=1;r[3]=0xa4;assert(fake_put(NULL,PORTABLE_RADIO_KEY,r,4)==0);assert(connect_saved(false)&&connects==1);break;}
 case 49:fail_store=1;assert(!connect_saved(false)&&!connects);fail_store=0;break;
#endif
 default:assert(!"Unknown scenario");
 }
 unsigned expected_writes=0;
#ifdef TEST_RADIO_POLICY
 if(scenario==48)expected_writes=2;
#endif
 fail_disconnect=fail_release=fail_cancel=false;app_module_fini();assert(!opened&&!grant_count&&!sub_count&&!frame_count&&!native_active&&writes==expected_writes);printf("Portable update UI scenario %u passed\n",scenario);return 0;
}
