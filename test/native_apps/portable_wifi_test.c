/* Real portable Wi-Fi app/controller, renderer and adapter. All providers fake;
 * never invokes a host radio, network, device or real user credentials. */
#define PORTABLE_WIFI_SETTINGS_APP
#define PORTABLE_WIFI_INSTANCE 15u
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_INPUT_NAVIGATION
#define WIFI_RETURN_APP "springboard.elf"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "../../lib/PortableApps/src/adapter.c"
#include "../../Apps/wifi_settings.c"
#ifdef PORTABLE_NOVA_UI
#define TEST_KEY_DELETE 10
#define TEST_KEY_DONE 11
#else
#define TEST_KEY_DELETE 33
#define TEST_KEY_DONE 34
#endif

const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned ticks,grant_count,sub_count,frame_count,presents,poll_count,launches;
static unsigned connects,disconnects,scan_starts,scan_polls,scan_cancels,writes,sleeps,alarm_acks;
static unsigned fail_connect,fail_disconnect,fail_release,fail_scan,fail_scan_cancel,fail_poll,fail_store,fail_addresses;
static unsigned acquire_denied,kv_denied,fail_display,short_api,late_calls,terminal_sleep,service_calls,alarm_stops,fail_alarm_stop;
static bool uncertain_start,break_radio_on_yield;
static unsigned script_count,limit_polls=500,scenario;
static bool native_active,scan_active;
static wifi_link_t fake_link;
static garden_radio_scan_result_v1 fake_scan;
static uint16_t pixels[240*240];
static alarm_status_v1 fake_alarm={.api_version=1,.struct_size=sizeof(fake_alarm),.state=ALARM_STATE_READY};
static jmp_buf blocked;
static bool block_expected,blocked_note;
static int sleep_outcome=1;
static struct {unsigned at;unsigned buttons;int x,y;} script[64];
static struct {char name[16];uint8_t data[64];uint32_t size;} cells[8];
static wifi_api_v1 radio_api;
static risc_runtime_api_v1 runtime_api;
static void observed(void){if(terminal_sleep)++late_calls;}
static bool fake_health(risc_runtime_health_v1 *h){h->uptime_ms=ticks;return poll_count<limit_polls;}
static void fake_yield(uint32_t ms){ticks+=ms;if(break_radio_on_yield){break_radio_on_yield=false;fake_link=WIFI_LINK_DOWN;fail_disconnect=1;}if(block_expected && blocked_note)longjmp(blocked,1);}
static bool fake_diag(const char *s){assert(!strstr(s,"testpass") && !strstr(s,"Fixture"));if(strstr(s,"retained"))blocked_note=true;return true;}
static bool fake_launch(const char *s){assert(!strcmp(s,WIFI_RETURN_APP));assert(!opened && !wg.api && !wk.api && !native_active);++launches;return true;}
static bool fake_info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=240,.height=240,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool fake_frame(void*c,uint32_t format,risc_display_surface_v1*out){(void)c;observed();assert(!frame_count);frame_count=1;*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=240,.height=240,.stride_bytes=480,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){(void)c;observed();assert(f==1 && frame_count);frame_count=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 (void)c;(void)r;(void)n;(void)o;observed();assert(f==1 && frame_count);if(fail_display)return false;frame_count=0;*t=++presents;
 const char *dir=getenv("PORTABLE_WIFI_FRAME_DIR");if(dir && presents<=12){char path[512];snprintf(path,sizeof(path),"%s/wifi-%u-%u.ppm",dir,scenario,presents);FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;++i){unsigned v=pixels[i];unsigned char rgb[]={(unsigned char)((v>>11)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};assert(fwrite(rgb,1,3,file)==3);}fclose(file);}return true;
}
static bool fake_present(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){(void)c;observed();assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present};
static uint64_t fake_subscribe(void*c){(void)c;observed();++sub_count;return 1;}
static bool fake_unsubscribe(void*c,uint64_t id){(void)c;observed();assert(id==1 && sub_count);--sub_count;return true;}
static bool fake_touch_poll(void*c,size_t n){(void)c;observed();assert(n==1);++poll_count;return true;}
static int32_t fake_next(void*c,uint64_t id,risc_touch_event_v1*e){(void)c;(void)id;(void)e;observed();return 0;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;observed();memset(out,0,sizeof(*out));out->width=out->height=240;for(unsigned i=0;i<script_count;++i)if(script[i].at==poll_count && script[i].x>=0){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=script[i].x,.y=script[i].y};
#if PORTABLE_TOUCH_ROTATION == 180
 out->contacts[0].x=239-out->contacts[0].x;out->contacts[0].y=239-out->contacts[0].y;
#endif
 }return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_subscribe,fake_unsubscribe,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*out){(void)c;observed();*out=(risc_input_navigation_frame_v1){0};for(unsigned i=0;i<script_count;++i)if(script[i].at==poll_count)out->pressed=out->released=script[i].buttons;return true;}
static bool fake_foreground(void*c,const risc_input_foreground_v1*a,size_t n){(void)c;(void)a;(void)n;observed();return true;}
static bool fake_reset(void*c){(void)c;observed();return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
static int32_t fake_get(void*c,const char*key,void*buf,uint32_t cap,uint32_t*size){(void)c;observed();*size=0;if(fail_store)return RISC_KEY_VALUE_IO;for(unsigned i=0;i<8;++i)if(!strcmp(cells[i].name,key)){if(cap<cells[i].size){*size=cells[i].size;return RISC_KEY_VALUE_BUFFER_SMALL;}memcpy(buf,cells[i].data,cells[i].size);*size=cells[i].size;return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*key,const void*buf,uint32_t n){(void)c;observed();++writes;if(fail_store)return RISC_KEY_VALUE_IO;assert(n<=64 && strlen(key)<=15);unsigned i=0;for(;i<8;++i)if(!cells[i].name[0] || !strcmp(cells[i].name,key))break;assert(i<8);strcpy(cells[i].name,key);memcpy(cells[i].data,buf,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,fake_get,fake_put};
static bool fake_connect(void*c,const char*s,const char*p){(void)c;observed();assert(!scan_active && !native_active);assert(s[0] && strlen(s)<=32 && (!p[0] || strlen(p)>=8));++connects;if(fail_connect){if(uncertain_start){native_active=true;fail_disconnect=1;}return false;}native_active=true;fake_link=WIFI_LINK_JOINING;return true;}
static void fake_disconnect_legacy(void*c){(void)c;assert(!"Legacy unchecked disconnect must never be used");}
static bool fake_disconnect(void*c){(void)c;observed();++disconnects;if(fail_disconnect)return false;native_active=scan_active=false;fake_link=WIFI_LINK_DOWN;return true;}
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
static bool fake_acquire(const char*name,uint32_t v,uint64_t id,risc_runtime_capability_v1*out){
 observed();assert(out->struct_size==sizeof(*out) && v==1);if(!strcmp(name,"net.wifi")){assert(id==15);if(acquire_denied)return false;out->api=&radio_api;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(id==6);if(kv_denied)return false;out->api=&kv_api;}
 else {assert(!id);if(!strcmp(name,"display.output"))out->api=&display_api;else if(!strcmp(name,"input.touch.raw"))out->api=&touch_api;else if(!strcmp(name,"input.navigation"))out->api=&nav_api;else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))out->api=&alarm_api;else return false;}
 ++grant_count;return true;
}
static bool fake_release(risc_runtime_capability_v1*g){observed();assert(g->api && grant_count);if(g->api==&radio_api){assert(!native_active || fail_disconnect);if(fail_release)return false;}--grant_count;g->api=NULL;return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime_api:NULL;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!native_active && !scan_active && !wg.api && !wifi && !sub_count && !surface.frame);++sleeps;if(sleep_outcome==-2)terminal_sleep=1;return sleep_outcome;}
static void event(unsigned at,unsigned buttons,int x,int y){assert(script_count<64);script[script_count].at=at;script[script_count].buttons=buttons;script[script_count].x=x;script[script_count++].y=y;}
static void start(void){
 runtime_api=(risc_runtime_api_v1){1,sizeof(runtime_api),fake_health,fake_yield,fake_diag,fake_launch,fake_acquire,fake_release};
 radio_api=(wifi_api_v1){.api_version=1,.struct_size=short_api?WIFI_PREFIX_V1_SIZE:sizeof(radio_api),.connect=fake_connect,.disconnect=fake_disconnect_legacy,.status=fake_status,.rssi=fake_rssi,.addresses=fake_addresses,.scan_start=fake_scan_start,.scan_poll=fake_scan_poll,.scan_cancel=fake_scan_cancel,.disconnect_checked=fake_disconnect};
 fake_scan=(garden_radio_scan_result_v1){.struct_size=sizeof(fake_scan),.state=GARDEN_RADIO_SCAN_RUNNING};
 assert(app_module_init()==0);assert(wifi_open());
}
static void draft(void){strcpy(credentials.ssid,"Fixture network");strcpy(credentials.password,"testpass123");open_network=false;}
static void tick(unsigned amount){ticks+=amount;wifi_tick();}
static void render(void){wifi_make_view();portable_wifi_render(&view);}
static void finish(void){fail_disconnect=fail_release=fail_scan_cancel=0;app_module_fini();assert(!opened && !grant_count && !sub_count && !frame_count && !native_active);for(unsigned i=0;i<sizeof(credentials);++i)assert(((uint8_t*)&credentials)[i]==0);for(unsigned i=0;i<sizeof(editor);++i)assert(!editor[i]);}
int main(int argc,char**argv){assert(argc==2);scenario=(unsigned)atoi(argv[1]);
 if(scenario==6)acquire_denied=1;
 if(scenario==7)kv_denied=1;
 if(scenario==8)short_api=1;
 start();render();
 switch(scenario){
 case 0:draft();wifi_connect();assert(joining && connects==1 && native_active);fake_link=WIFI_LINK_UP;tick(250);assert(!joining && link_state==WIFI_LINK_UP && strstr(ip_text,"192.0.2.10"));wifi_activate(6);assert(saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED);assert(wifi_disconnect());assert(!native_active);break;
 case 1:draft();fail_connect=1;wifi_connect();assert(!joining && connects==1);fail_connect=0;wifi_connect();assert(joining && connects==2);wifi_activate(4);assert(!joining && !native_active);break;
 case 2:draft();wifi_connect();tick(30001);assert(!joining && !native_active && strstr(wifi_message,"timed out"));wifi_connect();fake_link=WIFI_LINK_DOWN;tick(250);assert(!joining && strstr(wifi_message,"failed"));break;
 case 3:wifi_scan_start();assert(scanning && page==WP_SCAN);fake_scan.state=GARDEN_RADIO_SCAN_DONE;fake_scan.count=2;strcpy(fake_scan.entries[0].ssid,"Fixture open");fake_scan.entries[0].auth=0;strcpy(fake_scan.entries[1].ssid,"Fixture private");fake_scan.entries[1].auth=2;tick(250);assert(!scanning && scan_active && scan_result.count==2);render();wifi_activate(1);assert(page==WP_PASSWORD && !scan_active && !native_active && !strcmp(credentials.ssid,"Fixture private"));strcpy(editor,"testpass123");wifi_key(TEST_KEY_DONE);wifi_connect();assert(connects==1);break;
 case 4:wifi_scan_start();assert(!wifi_back() && page==WP_ROOT && !native_active);wifi_scan_start();tick(15001);assert(!scanning && !native_active);break;
 case 5:wifi_scan_start();fake_scan.state=GARDEN_RADIO_SCAN_DONE;fake_scan.count=17;tick(250);assert(!scanning && !native_active);wifi_scan_start();fail_poll=1;tick(250);assert(!scanning && !native_active);break;
 case 6:assert(!wifi);draft();wifi_connect();assert(!connects);acquire_denied=0;wifi_connect();assert(connects==1);break;
 case 7:assert(!wifi_store);draft();wifi_activate(6);assert(saved_state!=PORTABLE_WIFI_CREDENTIALS_LOADED && !writes);wifi_connect();assert(connects==1);break;
 case 8:assert(!wifi && !wg.api);draft();wifi_connect();assert(!connects);break;
 case 9:draft();wifi_edit(WP_PASSWORD);strcpy(editor,"discarded");assert(!wifi_back() && page==WP_ROOT && !strcmp(credentials.password,"testpass123") && !editor[0]);wifi_edit(WP_SSID);memset(editor,'s',32);editor[32]=0;key_page=2;wifi_key(1);assert(strlen(editor)==32);wifi_key(TEST_KEY_DELETE);assert(strlen(editor)==31);wifi_key(TEST_KEY_DONE);assert(strlen(credentials.ssid)==31);break;
 case 10:wifi_edit(WP_PASSWORD);for(unsigned i=0;i<100;++i)wifi_key(1);assert(strlen(editor)==63);render();assert(!strstr(view.entry,"a") && strlen(view.entry)==25);wifi_key(TEST_KEY_DONE);assert(strlen(credentials.password)==63);break;
 case 11:draft();wifi_activate(6);wifi_activate(7);assert(page==WP_FORGET);wifi_activate(0);assert(page==WP_ROOT && credentials.ssid[0]);wifi_activate(7);wifi_activate(1);assert(!credentials.ssid[0] && !credentials.password[0] && saved_state==PORTABLE_WIFI_CREDENTIALS_EMPTY);break;
 case 12:draft();fail_store=1;wifi_activate(6);assert(saved_state!=PORTABLE_WIFI_CREDENTIALS_LOADED);fail_store=0;wifi_activate(6);assert(saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED);break;
 case 13:draft();wifi_connect();fail_disconnect=1;assert(!wifi_back() && opened && !launches && cleanup_pending);fail_disconnect=0;assert(wifi_back() && launches==1 && !opened);break;
 case 14:draft();wifi_connect();assert(portable_wifi_suspend() && !native_active && !wg.api && credentials.password[0] && wk.api);portable_wifi_resume();assert(wg.api && !joining && connects==1 && credentials.password[0]);assert(portable_wifi_suspend());acquire_denied=1;portable_wifi_resume();assert(!wifi && !native_active);break;
 case 15:draft();wifi_connect();fail_disconnect=1;unsigned live=grant_count;assert(!portable_wifi_suspend() && wg.api && grant_count==live && native_active);fail_disconnect=0;assert(portable_wifi_suspend());break;
 case 16:draft();wifi_connect();fail_release=1;assert(!portable_wifi_suspend() && wg.api && !native_active);fail_release=0;assert(portable_wifi_suspend() && !wg.api);break;
 case 17:draft();wifi_edit(WP_PASSWORD);render();sleep_outcome=-2;ticks+=60001;t5_app_input_t in;assert(!poll(&in,25) && native_sleep_retained && sleeps==1);unsigned retained_live=grant_count;app_module_fini();assert(!late_calls && grant_count==retained_live && wk.api && editor[0] && !wg.api);/* Simulated runtime resume only for test-process leak cleanup. */terminal_sleep=0;native_sleep_retained=false;failed=false;break;
 case 18:draft();wifi_connect();render();ticks+=60001;t5_app_input_t wake;assert(poll(&wake,25) && sleeps==1 && !wake.buttons && !wake.tapped && !joining && wg.api && connects==1);break;
 case 19:draft();wifi_edit(WP_PASSWORD);render();fake_alarm.state=ALARM_STATE_ALERT;fake_alarm.occurrence=(alarm_token_v1){1,1,1,1};event(poll_count+4,0,100,180);bool consumed=false;assert(alarm_foreground(&consumed) && consumed && alarm_acks==1);assert(page==WP_PASSWORD && !strcmp(editor,"testpass123") && !launches);break;
 case 20:draft();wifi_scan_start();fail_scan_cancel=1;assert(!wifi_back() && cleanup_pending && scan_active);fail_scan_cancel=0;assert(!wifi_back() && page==WP_ROOT && !scan_active);break;
 case 21:draft();wifi_connect();fail_disconnect=1;block_expected=true;if(!setjmp(blocked)){app_module_fini();assert(!"Unsafe fini must retain invocation");}block_expected=false;assert(opened && wg.api && wk.api && native_active);break;
 case 22:/* Real app_main touch nesting: SSID, one lowercase key, Done, Back. */assert(portable_wifi_close());event(poll_count+3,0,60,72);event(poll_count+6,0,50,
#ifdef PORTABLE_NOVA_UI
 104
#else
 80
#endif
 );event(poll_count+9,0,190,190);event(poll_count+12,0,20,18);app_main();assert(launches==1 && !opened && !connects && !writes);break;
 case 23:/* Navigation Back cancels draft before root Back queues launch. */assert(portable_wifi_close());event(poll_count+3,T5_APP_BUTTON_CONFIRM,-1,0);event(poll_count+6,T5_APP_BUTTON_BACK,-1,0);event(poll_count+9,T5_APP_BUTTON_BACK,-1,0);app_main();assert(launches==1 && !opened && !writes);break;
 case 24:wifi_scan_start();fake_scan.state=GARDEN_RADIO_SCAN_DONE;fake_scan.count=1;memset(fake_scan.entries[0].ssid,'x',33);tick(250);assert(!scanning && !native_active);break;
 case 25:wifi_edit(WP_PASSWORD);
#ifdef PORTABLE_NOVA_UI
 for(key_page=0;key_page<12;++key_page)for(unsigned k=0;k<8;k++){editor[0]=0;wifi_key(k);assert((unsigned char)editor[0]==portable_nova_key_character(key_page,k));}
#else
 for(key_page=0;key_page<3;++key_page)for(unsigned k=0;k<32;++k){editor[0]=0;wifi_key(k);unsigned ch=32+key_page*32+k;if(ch<=126)assert((unsigned char)editor[0]==ch);else assert(!editor[0]);}
#endif
 break;
 case 26:fail_scan=1;wifi_scan_start();assert(!scanning && !native_active);fail_scan=0;wifi_scan_start();assert(scanning);break;
 case 27:draft();wifi_activate(2);assert(open_network && !credentials.password[0]);wifi_connect();assert(connects==1);break;
 case 28:draft();wifi_connect();fake_link=(wifi_link_t)99;tick(250);assert(!cleanup_pending && !native_active);assert(wifi_disconnect());break;
 case 29:fail_display=1;render();assert(failed);fail_display=0;break;
 case 30:draft();wifi_scan_start();fake_scan.state=GARDEN_RADIO_SCAN_DONE;fake_scan.count=1;strcpy(fake_scan.entries[0].ssid,"Unsupported");fake_scan.entries[0].auth=255;tick(250);wifi_activate(0);assert(page==WP_SCAN && !connects);break;
 case 31:/* Footer hit testing cannot activate rows; all keyboard keys bounded. */wifi_make_view();assert(portable_wifi_hit(&view,30,192)<0 && portable_wifi_hit(&view,30,220)<0);wifi_edit(WP_SSID);wifi_make_view();
#ifdef PORTABLE_NOVA_UI
 assert(portable_wifi_hit(&view,223,175)==7 && portable_wifi_hit(&view,223,231)==11 && portable_wifi_hit(&view,224,231)<0);
#else
 assert(portable_wifi_hit(&view,227,171)==31 && portable_wifi_hit(&view,227,206)==34 && portable_wifi_hit(&view,228,206)<0);
#endif
break;
 case 32:draft();wifi_connect();fail_disconnect=1;render();ticks+=60001;t5_app_input_t refused;assert(poll(&refused,25) && !failed && !sleeps && cleanup_pending && native_active && sub_count);fail_disconnect=0;wifi_activate(5);assert(!cleanup_pending && !native_active);break;
 case 33:draft();wifi_activate(9);assert(page==WP_HELP);render();assert(!wifi_back() && page==WP_ROOT && !launches);break;
 case 34:/* Empty password editor never silently converts protected to open. */draft();wifi_edit(WP_PASSWORD);editor[0]=0;wifi_key(TEST_KEY_DONE);assert(!open_network);wifi_connect();assert(!connects);break;
 case 35:draft();fail_connect=1;uncertain_start=true;wifi_connect();assert(cleanup_pending && native_active);unsigned before=service_calls;bool pending=false;assert(alarm_foreground(&pending) && !pending && service_calls==before);fail_disconnect=0;wifi_activate(5);assert(!cleanup_pending && !native_active);assert(alarm_foreground(&pending) && service_calls>before);break;
 case 36:fail_scan=1;uncertain_start=true;wifi_scan_start();assert(cleanup_pending && native_active);bool scan_pending=false;unsigned scan_before=service_calls;assert(alarm_foreground(&scan_pending) && service_calls==scan_before);break;
 case 37:draft();wifi_connect();fake_link=WIFI_LINK_DOWN;fail_disconnect=1;bool link_pending=false;unsigned link_before=service_calls;assert(alarm_foreground(&link_pending) && cleanup_pending && service_calls==link_before);break;
 case 38:draft();wifi_connect();wifi_edit(WP_PASSWORD);render();fake_alarm.state=ALARM_STATE_ALERT;fake_alarm.occurrence=(alarm_token_v1){1,1,1,1};break_radio_on_yield=true;bool paused=false;assert(alarm_foreground(&paused) && paused && cleanup_pending && alarm_stops && !alarm_modal);assert(page==WP_PASSWORD && !strcmp(editor,"testpass123"));fail_disconnect=0;assert(wifi_disconnect());fake_alarm.state=ALARM_STATE_READY;fake_alarm.occurrence=(alarm_token_v1){0};break;
 case 39:wifi_scan_start();fake_scan.state=GARDEN_RADIO_SCAN_DONE;tick(250);assert(scan_owned && !scanning);fail_poll=fail_scan_cancel=1;bool completed_pending=false;unsigned completed_before=service_calls;assert(alarm_foreground(&completed_pending) && cleanup_pending && service_calls==completed_before);break;
 case 40:draft();wifi_connect();fail_disconnect=1;assert(!wifi_disconnect());unsigned old_writes=writes;wifi_activate(6);assert(writes==old_writes);wifi_page(WP_FORGET);wifi_activate(1);assert(writes==old_writes && credentials.ssid[0] && cleanup_pending);break;
 case 41:wifi_scan_start();fake_scan.state=GARDEN_RADIO_SCAN_DONE;tick(250);assert(scan_owned && !scanning && native_active);assert(!wifi_back() && page==WP_ROOT && !scan_owned && !native_active);break;
 case 42:draft();wifi_connect();fake_link=WIFI_LINK_UP;fail_addresses=1;assert(portable_wifi_services_safe() && !native_active && !cleanup_pending && link_state==WIFI_LINK_DOWN && !strcmp(status_text,"Disconnected"));break;
 case 43:draft();wifi_connect();fake_link=WIFI_LINK_UP;fail_addresses=1;uncertain_start=true;unsigned address_before=service_calls;bool address_pending=false;assert(alarm_foreground(&address_pending) && cleanup_pending && native_active && service_calls==address_before && !strcmp(status_text,"Cleanup required"));break;
 case 44:draft();wifi_connect();wifi_edit(WP_PASSWORD);render();fake_alarm.state=ALARM_STATE_ALERT;fake_alarm.occurrence=(alarm_token_v1){1,1,1,1};break_radio_on_yield=true;fail_alarm_stop=1;block_expected=true;if(!setjmp(blocked)){bool trapped=false;(void)alarm_foreground(&trapped);assert(!"Expected bounded stop-only retention");}block_expected=false;assert(alarm_stops==3 && opened && wg.api && wk.api && alarm_failed_cleaned);fail_alarm_stop=0;break;
 case 45:draft();wifi_activate(6);assert(!draft_dirty);unsigned saved_writes=writes;wifi_edit(WP_SSID);strcpy(editor,"New draft");wifi_key(TEST_KEY_DONE);wifi_make_view();assert(draft_dirty && !strcmp(view.values[6],"Draft not saved") && writes==saved_writes);wifi_activate(6);assert(!draft_dirty && writes>saved_writes);break;
 case 46:draft();wifi_connect();render();fail_display=1;render();assert(failed && native_active);t5_app_input_t display_error;assert(!poll(&display_error,25));assert(!native_active && !scan_active && !wg.api && wk.api && !native_sleep_retained);/* This is the Runtime pre-fini barrier point: no fini has run. */fail_display=0;break;
 case 47:draft();wifi_connect();render();limit_polls=poll_count; t5_app_input_t input_error;assert(!poll(&input_error,25));assert(!native_active && !wg.api && wk.api && !native_sleep_retained);break;
 default:assert(!"Unknown scenario");
 }
 finish();printf("Portable Wi-Fi scenario %u passed\n",scenario);return 0;
}
