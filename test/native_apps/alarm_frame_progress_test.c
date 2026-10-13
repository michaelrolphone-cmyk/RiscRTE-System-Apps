/* Production poll + alarm client + sliced raster, provider/RTOS doubles only.
 * A deferred alarm keeps logical input quarantined, but cannot starve an
 * immutable frame or the scheduler needed to finish native storage work. */
#define TEST_NATIVE_TOOLBAR_QUICK
#define main toolbar_fixture_main
#include "portable_native_toolbar_test.c"
#undef main
static bool pending_forever;
static unsigned blocked_at,alarm_steps,submitted_at;
static bool still_loading(void){return pending_forever || (unsigned)(ticks-blocked_at)<64u;}
static int32_t busy_status(void *context,alarm_status_v1 *out){
 (void)context;io();*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),
  .state=still_loading()?ALARM_STATE_LOADING:ALARM_STATE_READY};return ALARM_OK;
}
static int32_t busy_step(void *context){(void)context;io();++alarm_steps;return still_loading()?ALARM_PENDING:ALARM_OK;}
static int32_t busy_refresh(void *context){(void)context;io();return ALARM_OK;}
static int32_t busy_ack(void *context,const alarm_token_v1 *token){(void)token;return busy_refresh(context);}
static int32_t busy_stop(void *context){(void)context;io();++alarm_stops;return ALARM_OK;}
static alarm_service_descriptor_v2 busy_alarm={.base={2,sizeof(busy_alarm),NULL,
 busy_status,busy_step,busy_refresh,busy_ack,fx_alarm_prepare,busy_stop},
 .tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION};
static bool async_info(void *context,risc_display_info_v1 *out){
 if(!fx_info(context,out))return false;
 out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return true;
}
static bool async_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){
 submitted_at=ticks;return fx_submit(c,f,r,n,o,t);
}
static bool async_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 if(!fx_present(c,token,out))return false;
 if((unsigned)(ticks-submitted_at)<8u)out->state=RISC_DISPLAY_PRESENT_ACTIVE;
 return true;
}
static risc_display_output_api_v1 async_display;
static bool selected_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){
  io();assert(version==2&&!instance);++acquires;++live;
  *out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=&busy_alarm.base};return true;
 }
 bool ok=fx_acquire(name,version,instance,out);
 if(ok&&!strcmp(name,"display.output"))out->api=&async_display;
 return ok;
}
int main(int argc,char **argv){
 assert(argc==3);const bool expected_broken=!strcmp(argv[2],"original-starves");
 const bool hardware=!strcmp(argv[1],"busy-hardware");pending_forever=strcmp(argv[1],"busy-worker")!=0;
 assert(hardware||!strcmp(argv[1],"busy-queued")||!strcmp(argv[1],"busy-worker"));
 async_display=fx_display;async_display.get_info=async_info;
 async_display.submit=async_submit;async_display.present_status=async_status;
 fx_runtime.acquire=selected_acquire;assert(app_module_init()==0);
 const paper_presentation *view=paper_presentation_get();assert(view);
 /* Seed an existing app-owned underlay, then queue the startup image through
  * the production latest-image/raster path, never a fabricated frame lease. */
 view->begin();view->text(20,20,300,"FOREGROUND",1,false,true);present(false);
 assert(alarm_pixels_valid);
 assert(portable_paper_frame_ready());
 view->begin();view->text(20,20,300,"QUEUED STARTUP FRAME",1,false,true);present(false);
 assert(raster_sealed&&!surface.frame&&!paper_token);
 const unsigned before_presents=presents;
 if(hardware){for(unsigned n=0;raster_sealed&&n<128;++n)assert(raster_progress());assert(paper_token&&!raster_sealed);}
 blocked_at=ticks;unsigned before_steps=alarm_steps;
 for(unsigned pass=0;pass<128;++pass){
  t5_app_input_t event={0};assert(poll(&event,20));
  assert(!event.buttons&&!event.tapped&&!event.exit_requested);
  assert(!failed&&!retained&&!alarm_modal&&!alarm_error_seen&&!alarm_stops);
 }
 printf("%s: submits=%u scheduler_ms=%u queued=%u token=%llu loading=%u\n",argv[1],
  presents-before_presents,ticks-blocked_at,(unsigned)raster_sealed,
  (unsigned long long)paper_token,(unsigned)(alarms.status.state==ALARM_STATE_LOADING));
 assert(alarm_steps-before_steps<=256); /* bounded service work per poll */
 if(expected_broken){
  assert(ticks==blocked_at&&!portable_paper_frame_idle()&&alarms.status.state==ALARM_STATE_LOADING);
  assert(hardware?paper_token!=0:raster_sealed);return 0;
 }
 assert(ticks>blocked_at&&presents==before_presents+1&&portable_paper_frame_idle());
 assert(pending_forever?alarms.status.state==ALARM_STATE_LOADING:alarms.status.state==ALARM_STATE_READY);
 pending_forever=false;ticks=blocked_at+128;
 bool consumed=true;assert(alarm_foreground(&consumed)&&!consumed&&alarms.status.state==ALARM_STATE_READY);
 app_module_fini();assert(!live&&!frames&&!subscriptions);
 puts("PASS: queued graphics, async display and native worker progress; input remains quarantined while loading");return 0;
}
