/* Actual atomic Settings controller at each native display geometry. */
#include <stdint.h>
static unsigned test_width,test_height,test_stride,test_size,native_script_started;
#include SETTINGS_ATOMIC_FIXTURE
static unsigned native_latency,native_submitted,native_checks;
static bool native_probe,native_probe_checked,native_probe_active,native_held_checked;
static bool native_back,native_exit,native_modal_active,native_back_sent;
static unsigned native_requested_mode;
static bool native_health(risc_runtime_health_v1*out){active_provider();assert(ticks<100000);if(native_probe_active&&!native_probe_checked&&ticks-native_script_started>=native_latency/2){assert(!submissions);native_probe_checked=true;}if(native_probe_active&&!native_held_checked&&ticks-native_script_started>=native_latency+100){assert(!submissions);native_held_checked=true;}out->uptime_ms=ticks;return true;}
static bool native_pending;static uint8_t *native_frozen;
static risc_display_output_api_v1 native_display;
static bool native_info(void*c,risc_display_info_v1*out){bool ok=display_info(c,out);if(native_latency)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool native_acquire_frame(void*c,uint32_t f,risc_display_surface_v1*out){assert(!native_pending);return frame_acquire(c,f,out);}
static bool native_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 assert(!native_pending);bool ok=frame_submit(c,f,r,n,o,t);if(ok&&native_latency){native_pending=true;native_submitted=ticks;memcpy(native_frozen,framebuffer,test_size);}return ok;
}
static bool native_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 if(!native_latency)return frame_status(c,t,out);
 assert(native_pending&&!memcmp(native_frozen,framebuffer,test_size));++native_checks;
 out->state=ticks-native_submitted<native_latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)native_pending=false;
 return true;
}
static bool native_exit_requested(void){return native_exit&&native_modal_active&&ticks-native_script_started>=60;}
static bool native_nav_poll(void*c,risc_input_navigation_frame_v1*out){
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(native_back&&native_modal_active&&!native_back_sent&&ticks-native_script_started>=60){assert(native_pending);native_back_sent=true;out->buttons=out->pressed=RISC_NAV_BACK;}return true;
}
static risc_input_navigation_api_v1 native_navigation;
static bool native_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 bool ok=acquire_checked(n,v,i,g);if(ok&&!strcmp(n,"display.output"))g->api=&native_display;
 if(ok&&!strcmp(n,"input.navigation"))g->api=&native_navigation;
 return ok;
}
static void native_choice_script(unsigned chosen,unsigned desired,bool cancel) {
 unsigned margin=test_width<200?8:20,gap=test_width<200?6:12;
 unsigned rh=test_height<200?32:44,top=test_height<200?28:56;
 unsigned fh=test_height<200?28:44,footer=test_height-8-fh,rows=(footer-18-top)/rh;
 unsigned first=chosen>=rows?chosen-rows+1:0,step=native_latency/20+4,t=step+8;
 if(native_probe){
  unsigned half=(test_width-2*margin-gap)/2;
  tap(3,test_width/2,top+rh/2);tap(7,margin+half+gap+half/2,footer+fh/2);
  /* Held contact begins before completed presentation and crosses it. */
  for(unsigned at=native_latency/20-2;at<=native_latency/20+3;at++)tap(at,margin+half+gap+half/2,footer+fh/2);
 }
 while(desired<first){tap(t,test_width/2,top+rh/2);tap(t+1,test_width/2,top+rh/2+rh);first--;t+=step;}
 while(desired>=first+rows){tap(t,test_width/2,top+rh/2);tap(t+1,test_width/2,top+rh/2-rh);first++;t+=step;}
 tap(t,test_width/2,top+(desired-first)*rh+rh/2);t+=step;
 unsigned half=(test_width-2*margin-gap)/2;
 tap(t,cancel?margin+half/2:margin+half+gap+half/2,footer+fh/2);
 if(mode==3||mode==4)tap(t+step+30,margin+half/2,footer+fh/2);
}
int main(int argc,char **argv) {
 assert(argc==4||argc==5);if(argc==5)native_latency=(unsigned)atoi(argv[4]);test_width=(unsigned)atoi(argv[1]);test_height=(unsigned)atoi(argv[2]);mode=(unsigned)atoi(argv[3]);assert(mode<=10);native_requested_mode=mode;native_probe=mode==8;native_back=mode==9;native_exit=mode==10;if(mode>=8){assert(native_latency>=120);mode=native_probe?0:2;}
 test_stride=test_width*2+6;test_size=test_stride*test_height;framebuffer=malloc(test_size);assert(framebuffer);memset(framebuffer,0xa5,test_size);
 native_frozen=malloc(test_size);assert(native_frozen);native_navigation=nav_api;native_navigation.poll=native_nav_poll;native_display=display_api;native_display.get_info=native_info;native_display.acquire=native_acquire_frame;native_display.submit=native_submit;native_display.present_status=native_status;
 scenario=100;checked_runtime=runtime_api;checked_runtime.health=native_health;checked_runtime.exit_requested=native_exit_requested;checked_runtime.acquire=native_acquire;checked_runtime.release=release_checked;checked_runtime.retain_invocation=retain_checked;
 assert(app_module_init()==0);assert(settings_atomic_api&&settings_store);settings_render(0,0);assert(portable_paper_frame_drain());polls=0;native_script_started=ticks;
 bool format=mode==1||mode==6,alerts=mode==5||mode==7,cancel=mode==2||mode==6||mode==7;
 unsigned alert_default;portable_alert_load(settings_store,&alert_default);
 if(!native_back&&!native_exit)native_choice_script(format?0:alerts?alert_default-1:2,1,cancel);
 native_probe_active=native_probe;native_modal_active=true;
 uint8_t result=settings_activate(0,format?2:alerts?SETTINGS_ALERT_ROW:4);
 native_probe_active=native_modal_active=false;if(native_probe)assert(native_probe_checked&&native_held_checked);if(native_back)assert(native_back_sent);
 assert((result==T5_APP_SETTING_UPDATED)==(mode<2||mode==5));
 assert(!format_writes&&!kv_writes&&!writes&&!retained);
 assert(submissions==((cancel||mode==3)?0u:1u));
 if(mode==5)assert(!strcmp(saved_key,"alert_mode")&&saved_size==1&&saved[0]==PORTABLE_ALERT_SOUND);
 else if(mode<2)assert(!strcmp(saved_key,mode==1?"time_format":"sleep_mode")&&saved_size==4&&saved[2]==1);
 else assert(!saved_size);
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count&&!token);polls=0;input_count=0;
 assert(app_module_init()==0);unsigned loaded;
 if(mode==5)assert(portable_alert_load(settings_store,&loaded)==PORTABLE_ALERT_LOADED&&loaded==PORTABLE_ALERT_SOUND);
 else if(mode==1)assert(portable_time_format_load(settings_store,&loaded)==PORTABLE_TIME_FORMAT_LOADED&&loaded==PORTABLE_TIME_FORMAT_24);
 else {assert(settings_sleep_load(&loaded)==(mode==0?PORTABLE_SLEEP_LOADED:PORTABLE_SLEEP_MISSING));if(mode==0)assert(loaded==PORTABLE_SLEEP_DEEP);}
 app_module_fini();assert(!grants&&!retained);
 const uint8_t*p=(const uint8_t*)framebuffer;for(unsigned y=0;y<test_height;y++)for(unsigned x=test_width*2;x<test_stride;x++)assert(p[y*test_stride+x]==0xa5);
 assert(!native_pending);if(native_latency)assert(native_checks>0);free(native_frozen);free(framebuffer);printf("Native atomic %ux%u case%u: preference save/cancel/error/reentry and custody passed\n",test_width,test_height,native_requested_mode);return 0;
}
