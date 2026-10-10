/* Production adapter input/state/presentation separation. The driver doubles
 * retain ordered edge reports and one immutable submitted visual snapshot. */
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_INPUT_NAVIGATION_LOCAL
#define main portable_frame_legacy_fixture_main
#include FRAME_FIXTURE
#undef main
static unsigned nav_edges,nav_presses,nav_releases,nav_last;
static bool probe_nav_poll(void*c,risc_input_navigation_frame_v1*out) {
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(nav_edges<120 && mock_ms>=5+(nav_edges/2)*10+(nav_edges%2)*3) {
  if(nav_edges++%2)out->released=RISC_NAV_RIGHT;
  else out->pressed=out->buttons=RISC_NAV_RIGHT;
 }
 return true;
}
static bool probe_nav_foreground(void*c,const risc_input_foreground_v1*x,size_t n){(void)c;(void)x;(void)n;return true;}
static bool probe_nav_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 probe_navigation={1,sizeof(probe_navigation),NULL,probe_nav_poll,probe_nav_foreground,probe_nav_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &probe_navigation;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;}
static risc_touch_event_v1 probe_events[RISC_TOUCH_QUEUE_LENGTH];
static risc_touch_snapshot_v1 probe_snapshot;
static unsigned probe_head,probe_count,probe_scheduled,probe_begins,probe_taps;
static unsigned probe_model,probe_drawn,probe_submissions[256],probe_submit_count,probe_max_lag;
static void probe_push(unsigned kind,unsigned at) {
 assert(probe_count<RISC_TOUCH_QUEUE_LENGTH);
 risc_touch_event_v1 e={.sequence=++probe_snapshot.sequence,.timestamp_ms=at,.kind=kind,.id=1,.x=100,.y=120};
 probe_events[(probe_head+probe_count)%RISC_TOUCH_QUEUE_LENGTH]=e;++probe_count;
 probe_snapshot.timestamp_ms=at;probe_snapshot.contact_count=kind==RISC_TOUCH_EVENT_UP?0:1;
 probe_snapshot.contacts[0]=(risc_touch_contact_v1){.id=1,.x=100,.y=120};
}
static bool probe_poll(void *c,size_t budget) {
 mock_poll_touch(c,budget);
 for(;probe_scheduled<120;probe_scheduled++) {
  unsigned at=5+(probe_scheduled/2)*10+(probe_scheduled%2)*3;
  if(mock_ms<at)break;
  probe_push(probe_scheduled%2?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_DOWN,at);
 }
 return true;
}
static int32_t probe_next(void *c,uint64_t id,risc_touch_event_v1 *event) {
 (void)c;assert(id==1);if(!probe_count)return 0;
 *event=probe_events[probe_head];probe_head=(probe_head+1)%RISC_TOUCH_QUEUE_LENGTH;--probe_count;return 1;
}
static bool probe_snap(void *c,risc_touch_snapshot_v1 *out){(void)c;*out=probe_snapshot;return true;}
static bool probe_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token) {
 assert(probe_submit_count<256);probe_submissions[probe_submit_count++]=probe_drawn;
 return mock_submit(c,f,r,n,o,token);
}
static void probe_case(unsigned format,unsigned latency) {
 begin_test();mock_format=format;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|
  (format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE:0);
 probe_head=probe_count=probe_scheduled=probe_begins=probe_taps=probe_model=probe_submit_count=probe_max_lag=0;
 nav_edges=nav_presses=nav_releases=nav_last=0;
 probe_drawn=~0u;probe_snapshot=(risc_touch_snapshot_v1){.width=format==RISC_DISPLAY_FORMAT_MONO1?480:240,.height=format==RISC_DISPLAY_FORMAT_MONO1?800:240};
 assert(!app_module_init());
 risc_touch_api_v1 input=mock_touch;input.poll=probe_poll;input.next=probe_next;input.snapshot=probe_snap;touch.api=&input;
 risc_display_output_api_v1 output=mock_display;output.submit=probe_submit;display=&output;
 if(format==RISC_DISPLAY_FORMAT_MONO1)assert(paper_presentation_get());else assert(springboard_presentation_get());handoff_finish();
 const t5_app_api_v1 *api=t5_app_get_api(1);assert(api&&api->frame_ready&&api->frame_drain);
 mock_latency=latency;
 while(mock_ms<latency*2+900) {
  if(api->frame_ready()&&probe_drawn!=probe_model) {
   api->clear();api->fill_rect(0,0,240,240,!!(probe_model&1));probe_drawn=probe_model;api->present(true);
  }
  t5_app_input_t event;assert(api->poll(&event,20));
  if(event.buttons&T5_APP_BUTTON_RIGHT){assert(nav_presses<60);nav_presses++;nav_last=mock_ms;}
  if(!event.buttons&&nav_presses>nav_releases)nav_releases++;
  springboard_contact contact;np_contact(&contact);
  if(contact.began)probe_begins++;
  if(contact.valid&&contact.released&&contact.tap_eligible&&!contact.cancelled) {
   ++probe_taps;++probe_model;
   unsigned at=8+(probe_taps-1)*10;
   unsigned lag=mock_ms-at;if(lag>probe_max_lag)probe_max_lag=lag;
   if(latency==2300)assert(mock_ms<latency);
  }
 }
 if(probe_begins!=60||probe_taps!=60||probe_model!=60||probe_drawn!=60)fprintf(stderr,"format=%u time=%u begins=%u taps=%u model=%u drawn=%u frame=%llu token=%llu nav=%u\n",format,mock_ms,probe_begins,probe_taps,probe_model,probe_drawn,(unsigned long long)surface.frame,(unsigned long long)paper_token,nav_presses);
 assert(probe_begins==60&&probe_taps==60&&probe_model==60&&probe_drawn==60);
 assert(probe_submissions[0]==0&&probe_submissions[probe_submit_count-1]==60);
 assert(probe_max_lag<=8);
 if(latency==2300)assert(probe_submit_count==2);
 assert(nav_presses==60&&nav_releases==60&&nav_last<610);
 assert(api->frame_drain());
 printf("{\"format\":%u,\"latency_ms\":%u,\"tap_cycles\":%u,\"navigation_press_release_cycles\":%u,\"max_input_lag_ms\":%u,\"submissions\":%u,\"latest_model\":%u}\n",format,latency,probe_taps,nav_presses,probe_max_lag,probe_submit_count,probe_model);
 end_test();
}
#ifndef FRAME_DELIVERY_NO_MAIN
int main(void){setvbuf(stdout,NULL,_IONBF,0);unsigned formats[]={RISC_DISPLAY_FORMAT_RGB565,RISC_DISPLAY_FORMAT_MONO1};unsigned latencies[]={0,17,120,2300};for(unsigned f=0;f<2;f++)for(unsigned l=0;l<4;l++)probe_case(formats[f],latencies[l]);return 0;}

#endif
