/* Production sparse adapter, KV facade and alarm/presentation checkpoints.
 * Only Runtime and physical device callbacks are modeled by the base fixture. */
#define main sparse_fixture_main
#include "sparse_clock_adapter_test.c"
#undef main

static int32_t read_result=RISC_KEY_VALUE_NOT_FOUND;
static int32_t write_result=RISC_KEY_VALUE_OK;
static unsigned controlled_reads,controlled_writes;
static bool loading=true;
static int32_t controlled_get(void *c,const char *k,void *p,uint32_t n,uint32_t *used) {
 (void)c;(void)k;(void)p;(void)n;io();controlled_reads++;*used=0;return read_result;
}
static int32_t controlled_put(void *c,const char *k,const void *p,uint32_t n) {
 (void)c;(void)k;(void)p;(void)n;io();controlled_writes++;return write_result;
}
static const risc_key_value_v1 controlled_kv={1,sizeof(controlled_kv),NULL,controlled_get,controlled_put};
static bool controlled_acquire(const char *name,uint32_t ver,uint64_t id,risc_runtime_capability_v1 *out) {
 if(!fx_acquire(name,ver,id,out))return false;
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY))out->api=&controlled_kv;
 return true;
}
static int32_t loading_status(void *c,alarm_status_v1 *out) {
 int32_t rc=fx_alarm_status(c,out);out->state=loading?ALARM_STATE_LOADING:ALARM_STATE_READY;return rc;
}
static int32_t loading_step(void *c) {(void)c;io();return loading?ALARM_PENDING:ALARM_OK;}
int portable_app_idle_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,
 const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
 (void)r;(void)d;(void)b;(void)a;assert(!"No idle sleep during startup test");return -1;
}
int main(int argc,char **argv) {
 assert(argc==2);const char *kind=argv[1];bool before=getenv("EXPECT_ORIGINAL_FAILURE")!=NULL;
 assert(app_module_init()==0);
 risc_runtime_api_v1 controlled_runtime=fx_runtime;controlled_runtime.acquire=controlled_acquire;
 desk_runtime=&controlled_runtime;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 native_custody_runtime=&controlled_runtime;
#endif
 bool busy=!strcmp(kind,"busy-cold");
 if(busy)read_result=RISC_KEY_VALUE_BUSY;
 async_display=true;
 int started=portable_desk_adapter_start(PORTABLE_DESK_START_FOREGROUND);
 if(busy&&before) {
  assert(started==-2&&native_sleep_retained&&native_fences==1&&controlled_reads==1);
  puts("original busy-cold: actual Quick settings load retains Home before first frame");return 0;
 }
 assert(started==1&&!native_sleep_retained&&!native_fences);
 if(busy) {
  assert(controlled_reads>1&&!quick.ui.brightness_valid&&!quick.ui.volume_valid);
  unsigned held_count=live;read_result=RISC_KEY_VALUE_NOT_FOUND;
  assert(pqa_session_load(&quick,rt)&&quick.ui.brightness_valid&&quick.ui.volume_valid);
  assert(live==held_count&&!native_sleep_retained);
 } else if(strcmp(kind,"alarm-frame")) {
  risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
  assert(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&grant));
  const risc_key_value_v1 *kv=grant.api;uint8_t byte=42;uint32_t used=0;int rc;
  bool put=!strcmp(kind,"busy-put");
  bool context=!strcmp(kind,"context");
  int expected=context?RISC_KEY_VALUE_CONTEXT:!strcmp(kind,"unknown")?-99:RISC_KEY_VALUE_BUSY;
  if(put){write_result=expected;rc=kv->put(kv->context,"quick_bright",&byte,1);}
  else {read_result=expected;rc=kv->get(kv->context,"quick_bright",&byte,1,&used);}
  assert(rc==expected);
  bool terminal=context||!strcmp(kind,"unknown")||before;
  assert(native_sleep_retained==terminal&&native_fences==(terminal?1u:0u));
  if(terminal) {unsigned held_count=live;app_module_fini();assert(live==held_count);puts("typed terminal result remains fenced");return 0;}
  assert(rt->release(&grant));
 } else {
  static alarm_service_descriptor_v2 pending_alarm;
  pending_alarm=fx_alarm_descriptor;
  pending_alarm.base.status=loading_status;pending_alarm.base.step=loading_step;
  alarms.api=&pending_alarm.base;
  assert(portable_paper_frame_ready());pp_begin();fill(20,20,100,80,0);present(true);
#ifdef PORTABLE_RASTER_SNAPSHOT
  assert(raster_sealed&&!submits);
#endif
  unsigned time_before=now,poll_before=touch_polls;
  t5_app_input_t input;
  for(unsigned i=0;i<256;i++) {
   assert(poll_input(&input,20));
   assert(!input.buttons&&!input.tapped&&!input.exit_requested);
   assert(!failed&&!native_sleep_retained);
  }
  if(before) {
   assert(now==time_before&&touch_polls==poll_before&&submits==0);
   puts("original alarm-frame: consumed LOADING polls never yield/render/submit; boot wait cannot finish");
   loading=false;assert(portable_paper_frame_drain());app_module_fini();return 0;
  }
  assert(now>time_before&&touch_polls>poll_before&&submits==1&&portable_paper_frame_idle());
  loading=false;assert(poll_input(&input,20));assert(!failed&&!native_sleep_retained);
  printf("alarm-frame: cold queued frame submitted/completed during pending alarms; elapsed=%u ms\n",now-time_before);
 }
 app_module_fini();assert(!live&&!subscriptions&&!frame_count&&!native_fences);
 printf("%s: production sparse adapter PASS\n",kind);return 0;
}
