/* Deterministic timing probe: real Settings controller/view and Watch adapter.
 * Models display latency; it does not claim physical panel measurements. */
#define main original_settings_fixture_main
#include "portable_alarm_settings_test.c"
#undef main
static unsigned latency, submitted_at, done_at, raw_samples, busy_raw_samples;
static unsigned logical_polls, changes, longest_poll, max_scroll;
static unsigned drag_start=100,drag_end=500;
static bool probing;
static bool timed_health(risc_runtime_health_v1*out){out->uptime_ms=ticks;return ticks<1500;}
static bool timed_snapshot(void*c,risc_touch_snapshot_v1*out){
 (void)c;*out=(risc_touch_snapshot_v1){.width=240,.height=240};++raw_samples;
 if(probing && ticks<done_at)++busy_raw_samples;
 if(ticks>=drag_start && ticks<drag_end){
  unsigned elapsed=ticks-drag_start,y=180-elapsed*120/(drag_end-drag_start);
  out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=120,.y=(uint16_t)y};
 }
 return true;
}
static bool timed_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*d,size_t n,
 const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 bool ok=frame_submit(c,f,d,n,o,t);submitted_at=ticks;done_at=ticks+latency;return ok;
}
static bool timed_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){
 (void)c;assert(t);s->state=ticks<done_at?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
int main(int argc,char**argv){
 assert(argc>=2);latency=(unsigned)atoi(argv[1]);drag_start=latency+100;
 drag_end=drag_start+(argc==3?(unsigned)atoi(argv[2]):400);
 scenario=100;assert(app_module_init()==0);
 risc_display_output_api_v1 timed_display=display_api;timed_display.submit=timed_submit;timed_display.present_status=timed_status;
 risc_touch_api_v1 timed_touch=touch_api;timed_touch.snapshot=timed_snapshot;
 risc_runtime_api_v1 timed_runtime=runtime_api;timed_runtime.health=timed_health;
 display=&timed_display;touch.api=&timed_touch;rt=&timed_runtime;probing=true;
 const t5_app_api_v1*api=t5_app_get_api(1);api->settings_render(0,0);
 while(ticks<latency+900){
  unsigned before=ticks;int old=sv_scroll[0];t5_app_input_t event={0};assert(api->poll(&event,20));++logical_polls;
  unsigned elapsed=ticks-before;if(elapsed>longest_poll)longest_poll=elapsed;
  if(sv_scroll[0]!=old)++changes;
  if((unsigned)sv_scroll[0]>max_scroll)max_scroll=(unsigned)sv_scroll[0];
 }
 app_module_fini();assert(!grants&&!subscriptions&&!frames&&!kv_writes&&!rtc_writes);
 printf("{\"display_latency_ms\":%u,\"drag_duration_ms\":%u,\"physical_samples\":%u,\"samples_while_present_busy\":%u,\"logical_polls\":%u,\"scroll_state_changes\":%u,\"longest_logical_poll_ms\":%u,\"max_scroll_pixels\":%u,\"presentations\":%u}\n",latency,drag_end-drag_start,raw_samples,busy_raw_samples,logical_polls,changes,longest_poll,max_scroll,presents);
 return 0;
}
