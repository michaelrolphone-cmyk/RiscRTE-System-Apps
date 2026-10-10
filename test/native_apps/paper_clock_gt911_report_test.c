/* Full production Clock/controller/adapter, with selected GT911 raw provider. */
#ifndef CLOCK_GT_INCLUDE_FIXTURE
#define main clock_fixture_main
#define risc_runtime_get_api clock_fixture_runtime
#include "paper_clock_test.c"
#undef main
#undef risc_runtime_get_api
#endif
const risc_touch_api_v1 *clock_gt911_start(void);
void clock_gt911_report(const risc_touch_snapshot_v1 *value,uint64_t when);
void clock_gt911_stop(void);
unsigned clock_gt911_emitted(unsigned kind);
static const risc_touch_api_v1 *gt911;
static risc_touch_snapshot_v1 requested;
static unsigned captured_at;
static bool gt_initialized;
static unsigned gt_first_dispatch_ms,gt_last_dispatch_ms;
static bool repeated_report,display_busy,display_pending,injected_multi;
static unsigned display_delay,display_submitted_at;
#ifndef CLOCK_GT_EXTERNAL_RUNTIME
static risc_display_output_api_v1 gt_display;
#endif
static bool gt_script(risc_touch_snapshot_v1 *out) {
#ifdef CLOCK_GT_REFUSAL_RETRY
 if(scenario==9)return clock_gt_refusal_script(out);
#endif
 if(scenario!=18)return snapshot(NULL,out);
 /* This completed one-finger swipe precedes the later multi-contact report.
  * Slow final settlement captures the later report without erasing the swipe. */
 *out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(script_ms>=50&&script_ms<125) {
  out->contact_count=1;
  out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=script_ms>=51?285:240,.y=360};
  if(script_ms>=75&&script_ms<100) {
   out->contact_count=2;
   out->contacts[1]=(risc_touch_contact_v1){.id=2,.x=320,.y=440};
  }
 }
 return true;
}
static void trace_snapshot(const char *what,const risc_touch_snapshot_v1 *s) {
 printf("%s seq=%llu timestamp=%llu contacts=%u",what,
  (unsigned long long)s->sequence,(unsigned long long)s->timestamp_ms,s->contact_count);
 for(unsigned i=0;i<s->contact_count;i++)printf(" id%u=(%u,%u)",s->contacts[i].id,s->contacts[i].x,s->contacts[i].y);
 putchar('\n');
}
static uint64_t gt_subscribe(void *c) {
 (void)c;subscribe(NULL);
 /* The hardware report cursor survives subscribers being closed/reopened.
  * Reopening a client must not erase a still-held physical contact's UP. */
 if(!gt_initialized){captured_at=script_ms;assert(gt_script(&requested));gt_initialized=true;}
 return gt911->subscribe(gt911->context);
}
static bool gt_unsubscribe(void *c,uint64_t id) {
 (void)c;assert(gt911->unsubscribe(gt911->context,id));return unsubscribe(NULL,1);
}
static bool gt_poll(void *c,size_t n) {
 assert(poll_touch(c,n));unsigned current=script_ms;
 while(captured_at<current) {
  script_ms=++captured_at;risc_touch_snapshot_v1 next;assert(gt_script(&next));
  bool changed=next.contact_count!=requested.contact_count||next.buttons!=requested.buttons||
   memcmp(next.contacts,requested.contacts,sizeof(next.contacts));
  if(changed||(repeated_report&&script_ms==76)) {
   clock_gt911_report(&next,ms-current+captured_at);
   injected_multi|=next.contact_count>1;
   risc_touch_snapshot_v1 actual;assert(gt911->snapshot(gt911->context,&actual));
   trace_snapshot(changed?"ready_changed":"ready_unchanged",&actual);
  }
  requested=next;
 }
 script_ms=current;return gt911->poll(gt911->context,n);
}
static int32_t gt_next(void *c,uint64_t id,risc_touch_event_v1 *out) {
 (void)c;int32_t result=gt911->next(gt911->context,id,out);
 if(result==1) {
  if(!gt_first_dispatch_ms)gt_first_dispatch_ms=ms;
  gt_last_dispatch_ms=ms;
  printf("dispatch_edge ms=%u seq=%llu timestamp=%llu kind=%u id=%u x=%u y=%u\n",
   ms,(unsigned long long)out->sequence,(unsigned long long)out->timestamp_ms,
   out->kind,out->id,out->x,out->y);
  risc_touch_snapshot_v1 latest;assert(gt911->snapshot(gt911->context,&latest));
  trace_snapshot("snapshot_at_dispatch",&latest);
  assert(out->kind<6);clock_input_delivered[out->kind]++;
 }
 return result;
}
static bool gt_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 (void)c;return gt911->snapshot(gt911->context,out);
}
static const risc_touch_api_v1 gt_touch={1,sizeof(gt_touch),NULL,gt_subscribe,gt_unsubscribe,gt_poll,gt_next,gt_snapshot};
static bool gt_info(void *c,risc_display_info_v1 *out) {
 assert(get_info(c,out));out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return true;
}
static bool gt_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *rect,
 size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
 assert(!display_pending);bool ok=submit(c,frame,rect,count,options,token);
 if(ok){display_submitted_at=ms;display_pending=true;printf("display_submit ms=%u token=%llu\n",ms,(unsigned long long)*token);}
 return ok;
}
static bool gt_present_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
 (void)c;assert(token==presents);
 bool pending=display_busy||(ms-display_submitted_at)<display_delay;
 if(display_pending&&!pending)printf("display_complete ms=%u token=%llu\n",ms,(unsigned long long)token);
 display_pending=pending;out->state=pending?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool gt_health(risc_runtime_health_v1 *out) {
 out->uptime_ms=ms;return script_ms<(display_busy?12000u:2500u);
}
static bool gt_diagnostic(const char *message) {
 printf("diagnostic ms=%u %s\n",ms,message);return diagnostic(message);
}
#ifndef CLOCK_GT_EXTERNAL_RUNTIME
static bool gt_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out) {
 if(!strcmp(name,"input.touch.raw")){assert(version==1&&!id);out->api=&gt_touch;grants++;return true;}
 if(!strcmp(name,"display.output")){assert(version==1&&!id);out->api=&gt_display;grants++;return true;}
 return acquire(name,version,id,out);
}
static bool gt_launch(const char *name) {
 assert(!display_pending&&!frames);
 assert(ms-display_submitted_at>=display_delay);
 printf("clock_launch ms=%u target=%s\n",ms,name);return launch_app(name);
}
static risc_runtime_api_v1 gt_runtime;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {return version==1?&gt_runtime:NULL;}
int main(int argc,char **argv) {
 assert(argc==3||argc==4);setbuf(stdout,NULL);repeated_report=argc==4;
 scenario=(unsigned)atoi(argv[1]);assert(scenario==9||scenario==14||scenario==15||scenario==16||scenario==18);
 if(!strcmp(argv[2],"slow"))display_delay=200;
 else if(!strcmp(argv[2],"busy"))display_busy=true;
 else assert(!strcmp(argv[2],"fast"));
 gt911=clock_gt911_start();ms=1000;gt_runtime=rt;gt_runtime.acquire=gt_acquire;gt_runtime.request_launch=gt_launch;
 gt_runtime.health=gt_health;gt_runtime.diagnostic=gt_diagnostic;
 gt_display=d;gt_display.get_info=gt_info;gt_display.submit=gt_submit;gt_display.present_status=gt_present_status;
 assert(app_module_init()==0);app_main();app_module_fini();
 assert(!grants&&!subs&&!frames);
 if(display_busy) {
  assert(!launches&&presents==1);
  assert(diagnostics==1&&strstr(last_diagnostic,"display-timeout"));
  assert(ms-display_submitted_at>=10000);
 }else {
  assert(!display_pending&&!diagnostics);
  if(scenario==14||scenario==18)assert(launches==1&&!strcmp(launched,"springboard.elf"));
  else if(scenario==9)assert(launches==2&&!strcmp(launched,"springboard.elf"));
  else assert(!launches);
  assert(presents==(scenario==9?2u:1u));
 }
 if(scenario==16) {
  assert(clock_gt911_emitted(RISC_TOUCH_EVENT_DOWN)==1&&clock_gt911_emitted(RISC_TOUCH_EVENT_UP)==1);
  /* The compatibility profile cannot dispatch until its first present ends.
   * A permanent first-frame failure must preserve capture but never navigate. */
  unsigned delivered=display_busy?0u:1u;
  assert(clock_input_delivered[RISC_TOUCH_EVENT_DOWN]==delivered&&clock_input_delivered[RISC_TOUCH_EVENT_UP]==delivered);
 }
 if(scenario==18&&(display_delay||display_busy))assert(injected_multi);
 printf("Clock actual GT911 scenario=%u display=%s launches=%u diagnostics=%u clean lifecycle\n",scenario,argv[2],launches,diagnostics);
 clock_gt911_stop();return 0;
}
#endif
