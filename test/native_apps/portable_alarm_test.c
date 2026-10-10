#define PORTABLE_ALARM_CLIENT
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_RETURN_APP "springboard.elf"
#define main original_settings_main
#include "portable_settings_test.c"
#undef main
#include <setjmp.h>
static jmp_buf retained;
static alarm_status_v1 alarm_fake={.api_version=1,.struct_size=sizeof(alarm_fake)};
static unsigned service_steps,acks,stop_calls,retries,phases,pending_status_calls;
static bool output_bad,live_display;
static unsigned alarm_scenario;
static unsigned normal_after_failure;
#ifdef PORTABLE_AUDIO_SESSION
static bool application_audio=true,audio_close_bad;
static unsigned application_audio_stops;
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
static bool continuous_capture;
bool portable_audio_capture_active(void){return continuous_capture&&application_audio;}
static unsigned audio_resumes;
void portable_audio_capture_resume(void){if(continuous_capture&&!audio_close_bad&&!application_audio){application_audio=true;audio_resumes++;}}
#endif
bool portable_audio_services_safe(void){return !audio_close_bad;}
bool portable_audio_suspend(void){
 if(audio_close_bad)return false;
 if(application_audio){application_audio=false;application_audio_stops++;}
 return true;
}
#endif
static bool reset_after_ack(void*c){(void)c;return acks==0;}
static int32_t service_status(void*c,alarm_status_v1*out){(void)c;*out=alarm_fake;return ALARM_OK;}
static int32_t service_step(void*c){(void)c;assert(!surface.frame);service_steps++;if(failed)normal_after_failure++;
#ifdef PORTABLE_AUDIO_SESSION
 if(alarm_fake.state==ALARM_STATE_ALERT||alarm_fake.state==ALARM_STATE_DISMISSING||alarm_fake.state==ALARM_STATE_CUE)assert(!application_audio);
#endif
 if(alarm_fake.state==ALARM_STATE_CUE){if(alarm_scenario!=14&&++phases>=5){alarm_fake.state=ALARM_STATE_READY;alarm_fake.output_uncertain=0;}return ALARM_OK;}
 if(alarm_fake.state==ALARM_STATE_LOADING){if(++phases==(alarm_scenario==8?10u:3u)){
   alarm_fake.state=ALARM_STATE_ALERT;
   if(alarm_scenario==8){
     alarm_fake.occurrence=(alarm_token_v1){ALARM_KIND_COUNTDOWN,8,88,10};
     /* Schedule a fresh gesture after the second identity is announced.
      * Poll cadence is independent of service/presentation cadence. */
     tap(polls+3,100,180);
   }
#ifdef PORTABLE_AUDIO_SESSION
   else if(alarm_scenario==1)alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};
#endif
 }}
 if(alarm_fake.state==ALARM_STATE_DISMISSING){if(++phases==3){
   if(alarm_scenario==8&&acks==1){
     /* Production VERIFY_OCC clears active, then reloads all records before
        it can discover a second due identity. BACK cannot exit this gap. */
     alarm_fake.state=ALARM_STATE_LOADING;alarm_fake.occurrence=(alarm_token_v1){0};phases=0;
   }else if(alarm_scenario==6&&acks==1){alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence.generation++;}
   else{alarm_fake.state=ALARM_STATE_READY;alarm_fake.occurrence=(alarm_token_v1){0};}
 }}
 return ALARM_OK;}
static int32_t service_refresh(void*c){(void)c;retries++;alarm_fake.state=ALARM_STATE_READY;return ALARM_PENDING;}
static int32_t service_ack(void*c,const alarm_token_v1*t){(void)c;assert(!memcmp(t,&alarm_fake.occurrence,sizeof(*t)));acks++;phases=0;if(alarm_scenario==7&&acks==1){alarm_fake.occurrence.generation++;return ALARM_STALE;}alarm_fake.state=ALARM_STATE_DISMISSING;return ALARM_PENDING;}
static int32_t service_prepare(void*c,alarm_sleep_v1*s){(void)c;(void)s;assert(!"unexpected sleep");return ALARM_INVALID;}
static int32_t service_stop(void*c){(void)c;
#ifdef PORTABLE_AUDIO_SESSION
 assert(!application_audio);
#endif
 stop_calls++;assert(stop_calls<=3);return stop_calls<3?ALARM_PENDING:output_bad?ALARM_OUTPUT:ALARM_OK;}
static const alarm_service_v1 service_api={1,sizeof(service_api),NULL,service_status,service_step,service_refresh,service_ack,service_prepare,service_stop};
static bool acquire_alarm(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*out){
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==1&&!instance);out->api=&service_api;out->slot=100;++grants;return true;}
 return runtime_api.acquire(name,version,instance,out);
}
static void yield_retained(uint32_t ms){
#ifdef PORTABLE_AUDIO_SESSION
 if(audio_close_bad&&failed)longjmp(retained,1);
#endif
 if(alarm_failed_cleaned&&output_bad)longjmp(retained,1);
 test_yield(ms);
}
static bool frame_delayed(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){
 (void)c;(void)token;pending_status_calls++;
 assert(!service_steps);if(pending_status_calls<3){live_display=true;out->state=RISC_DISPLAY_PRESENT_ACTIVE;}else{live_display=false;out->state=RISC_DISPLAY_PRESENT_COMPLETE;}return true;}
static unsigned native_sleep_calls;
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,
        const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){
 (void)r;(void)d;(void)g;(void)a;assert(!subscriptions&&!surface.frame);native_sleep_calls++;return -2;
}
#ifndef PORTABLE_ALARM_FIXTURE_MAIN
#define PORTABLE_ALARM_FIXTURE_MAIN main
#endif
int PORTABLE_ALARM_FIXTURE_MAIN(int argc,char**argv){
 char title[24];alarm_status_v1 named={0};
 strcpy(named.label,"LUNCH END");assert(!strcmp(alarm_title(&named,title),"LUNCH END"));
 strcpy(named.label,"WORK START");assert(!strcmp(alarm_title(&named,title),"WORK START"));
 memset(named.label,'X',sizeof(named.label));assert(!strcmp(alarm_title(&named,title),"ALARM"));
 named.label[0]=1;named.label[1]=0;assert(!strcmp(alarm_title(&named,title),"ALARM"));
 named=(alarm_status_v1){.occurrence={ALARM_KIND_COUNTDOWN,0,0,0}};
 assert(!strcmp(alarm_title(&named,title),"COUNTDOWN FINISHED"));
 unsigned test=argc>1?(unsigned)atoi(argv[1]):0;alarm_scenario=test;scenario=100;
 risc_runtime_api_v1 r=runtime_api;r.acquire=acquire_alarm;r.yield_ms=yield_retained;
 /* Fixture initialize calls the original static runtime; acquire service
    manually after setting up its ordinary production display/touch grants. */
 rt=&r;dg.struct_size=sizeof(dg);bg.struct_size=sizeof(bg);
 assert(acquire_alarm("display.output",1,0,&dg));display=dg.api;assert(display_info(NULL,&info));
 assert(portable_touch_open(&touch,&r));failed=false;display_settled=true;
 alarm_pixels=malloc(sizeof(framebuffer));assert(alarm_pixels);assert(portable_alarm_open(&alarms,&r));
 alarm_fake.state=ALARM_STATE_READY;
 risc_input_navigation_api_v1 nav=nav_api;
 if(test==9){nav.reset=reset_after_ack;navigation=&nav;navigation_ready=true;navigation_neutral=true;}
 if(test==0){risc_display_output_api_v1 delayed=display_api;delayed.present_status=frame_delayed;display=&delayed;clear();present(false);assert(display_settled&&pending_status_calls==3&&!service_steps);display=&display_api;}
 clear();fill(0,0,240,240,0x1234);present(false);assert(alarm_pixels_valid);
#ifdef PORTABLE_AUDIO_SESSION
 #ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
 if(test==16){
  continuous_capture=true;last_activity=0;alarm_fake.state=ALARM_STATE_READY;
  for(unsigned minute=1;minute<=20;minute++){ticks=minute*60000u;t5_app_input_t input;assert(poll(&input,1)&&!native_sleep_calls&&application_audio&&!application_audio_stops);}
  continuous_capture=false;application_audio=false;t5_app_input_t input;assert(!poll(&input,1)&&native_sleep_calls==1&&portable_app_sleep_retained());
  unsigned live=grants;app_module_fini();assert(grants==live&&live);
  puts("Continuous capture survives twenty idle deadlines; explicit stop restores idle sleep PASS");return 0;
 }
 if(test==17){
  continuous_capture=true;alarm_fake.state=ALARM_STATE_CUE;bool consumed=false;
  assert(alarm_foreground(&consumed)&&consumed&&!application_audio&&application_audio_stops==1);
  t5_app_input_t input;assert(poll(&input,1)&&application_audio&&audio_resumes==1&&!native_sleep_calls);
  continuous_capture=false;assert(portable_audio_suspend());assert(poll(&input,1)&&!application_audio&&audio_resumes==1);
  app_module_fini();assert(!grants);puts("Requested capture resumes after settled cue; explicit stop stays stopped PASS");return 0;
 }
 #endif
 if(test==11||test==15){
  alarm_fake.state=test==15?ALARM_STATE_CUE:ALARM_STATE_ALERT;alarm_fake.occurrence=test==15?(alarm_token_v1){0}:(alarm_token_v1){1,7,88,9};audio_close_bad=true;
  unsigned before=service_steps,live=grants;
  if(!setjmp(retained)){bool consumed;alarm_foreground(&consumed);assert(!"uncertain audio must retain");}
  assert(service_steps==before&&!stop_calls&&grants==live&&application_audio);
  puts("uncertain app audio retains without alarm/storage calls passed");return 0;
 }
 if(test==12){
  alarm_fake.state=ALARM_STATE_READY;bool consumed;
  assert(alarm_foreground(&consumed)&&!consumed&&application_audio&&!application_audio_stops);
  t5_app_input_t input={0};tap(3,10,10);
  for(unsigned n=0;n<10&&!input.exit_requested;n++)assert(poll(&input,20));
  assert(input.exit_requested&&!application_audio&&application_audio_stops==1);
  assert(return_launches==1);app_module_fini();assert(!grants);
  puts("healthy stream survives idle alarm checks; Back stops before return passed");return 0;
 }
#endif
 if(test==13||test==14){
  alarm_fake.state=ALARM_STATE_CUE;alarm_fake.output_uncertain=1;
  navigation_pending=T5_APP_BUTTON_BACK;input_pending=true;
  bool consumed=false,result=alarm_foreground(&consumed);assert(consumed&&!alarm_modal&&!acks&&!return_launches);
  for(unsigned i=0;i<240*240;i++)assert(framebuffer[i]==0x1234);
  if(test==14){assert(!result&&failed&&stop_calls==3);puts("Non-modal cue timeout performs bounded cleanup without changing frame passed");return 0;}
  assert(result&&!stop_calls&&!input_pending&&!navigation_pending&&alarm_fake.state==ALARM_STATE_READY);
#ifdef PORTABLE_AUDIO_SESSION
  assert(!application_audio&&application_audio_stops==1);
#endif
  app_module_fini();assert(!grants);puts("Non-modal cue reserves outputs, consumes stale input and keeps frame passed");return 0;
 }
 if(test==10){
  ticks=60000;last_activity=0;alarm_fake.state=ALARM_STATE_READY;t5_app_input_t input;
  assert(!poll(&input,20)&&native_sleep_calls==1&&portable_app_sleep_retained()&&!stop_calls);
#ifdef PORTABLE_AUDIO_SESSION
  assert(!application_audio&&application_audio_stops==1);
#endif
  unsigned before=service_steps,live=grants;app_module_fini();
  assert(service_steps==before&&grants==live&&live&&!stop_calls);
  puts("native-retained sleep bypasses service cleanup/fini and keeps grants passed");return 0;
 }
 if(test==4||test==5){failed=true;display_settled=false;output_bad=test==5;unsigned before=service_steps;
   if(!setjmp(retained)){bool consumed;assert(!alarm_foreground(&consumed));assert(!output_bad);assert(!alarm_foreground(&consumed));}
   assert(stop_calls==3&&service_steps==before);puts("alarm display-failure bounded stop-only passed");return 0;}
 if(test==3){alarm_fake.state=ALARM_STATE_BLOCKED;alarm_fake.error=ALARM_RTC;tap(3,100,215);}
 else {alarm_fake.state=test==1?ALARM_STATE_LOADING:ALARM_STATE_ALERT;
#ifdef PORTABLE_AUDIO_SESSION
   alarm_fake.occurrence=test==1?(alarm_token_v1){0}:(alarm_token_v1){1,7,88,9};
#else
   alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};
#endif
   navigation_pending=T5_APP_BUTTON_BACK;input_pending=true;input_sample=(portable_touch_sample){.valid=true,.released=true,.tap_eligible=true,.x=100,.y=180};
   tap(3,100,180);if(test==6||test==7)tap(12,100,180);
   if(test==8){tap(8,100,180); /* stale gesture during durable reload gap */}}
 bool consumed=false;bool result=alarm_foreground(&consumed);
 if(test==9){
   assert(!result&&consumed&&failed&&acks==1&&normal_after_failure==0&&stop_calls==3);
   unsigned before=service_steps;app_module_fini();
   assert(service_steps==before&&stop_calls==3&&!grants&&!subscriptions&&!frame_count);
   puts("alarm reset failure stops before normal service I/O passed");return 0;
 }
 assert(result&&consumed);
#ifdef PORTABLE_AUDIO_SESSION
 assert(!application_audio&&application_audio_stops==1);
#endif
 assert(!return_launches&&stop_calls==0);assert(test==3?retries==1:acks==((test==6||test==7||test==8)?2u:1u));
 for(unsigned i=0;i<240*240;i++)assert(framebuffer[i]==0x1234);
 assert(!input_pending&&!navigation_pending&&!touch.down);
 assert(alarm_fake.state==ALARM_STATE_READY);
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count);puts("alarm retained frame/input/exact dismiss/retry passed");return 0;
}
