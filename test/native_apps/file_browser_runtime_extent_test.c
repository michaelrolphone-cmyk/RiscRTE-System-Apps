/* Exercise the real Files adapter's guarded Runtime copy and input poll. */
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_CHECKED_STORAGE_RUNTIME
#define main unused_browser_fixture_main
#include "portable_file_browser_test.c"
#undef main

static bool host_exit;
static unsigned host_exit_reads, host_clock_reads, host_retains;
static bool host_exit_requested(void) { ++host_exit_reads; return host_exit; }
static bool host_monotonic(uint32_t *out) { ++host_clock_reads; *out=test_ticks; return true; }
static bool host_retain(void) { ++host_retains; return true; }
static int32_t host_alarm_status(void *c,alarm_status_v1 *out) {
 (void)c; *out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY}; return ALARM_OK;
}
static int32_t host_alarm_step(void *c) { (void)c; return ALARM_OK; }
static int32_t host_alarm_ack(void *c,const alarm_token_v1 *token) { (void)c;(void)token;return ALARM_OK; }
static int32_t host_alarm_prepare(void *c,alarm_sleep_v1 *sleep) { (void)c;(void)sleep;return ALARM_OK; }
static const alarm_service_v1 host_alarm={1,sizeof(host_alarm),NULL,host_alarm_status,host_alarm_step,host_alarm_step,host_alarm_ack,host_alarm_prepare,host_alarm_step};
static bool host_battery_read(void *c,risc_battery_sample_v1 *out) { (void)c;*out=(risc_battery_sample_v1){3900,70,0};return true; }
static const risc_battery_gauge_api_v1 host_battery={1,sizeof(host_battery),NULL,host_battery_read};
static bool host_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY))grant->api=&host_alarm;
 else if(!strcmp(name,"board.battery"))grant->api=&host_battery;
 else return test_acquire(name,version,instance,grant);
 ++test_grants;return true;
}
int main(int argc,char **argv) {
 assert(argc==2);const char *mode=argv[1];
 const size_t prefix=RISC_RUNTIME_DIAGNOSTIC_CHECKPOINT_V1_SIZE;
 const size_t clock_end=prefix+sizeof(bool(*)(uint32_t*));
 const size_t native_size=clock_end+sizeof(bool(*)(void));
 size_t advertised=!strcmp(mode,"old")?prefix:!strcmp(mode,"clock")?clock_end:
   !strcmp(mode,"future")?native_size+32:native_size;
 unsigned char *native=malloc(advertised);assert(native);memset(native,0xa5,advertised);
 risc_runtime_api_v1 base=test_runtime;base.acquire=host_acquire;base.retain_invocation=host_retain;base.struct_size=(uint32_t)advertised;
 memcpy(native,&base,prefix);
 bool (*clock_fn)(uint32_t*)=host_monotonic;
 bool (*exit_fn)(void)=!strcmp(mode,"null")?NULL:host_exit_requested;
 if(advertised>=clock_end)memcpy(native+prefix,&clock_fn,sizeof(clock_fn));
 if(advertised>=native_size)memcpy(native+clock_end,&exit_fn,sizeof(exit_fn));
 test_runtime_override=(const risc_runtime_api_v1 *)native;
 test_volume=(risc_storage_volume_api_v1){1,sizeof(test_volume),NULL,test_ready,test_ready,test_label,test_stat,test_diropen,test_dirnext,test_dirclose,test_open,test_read,NULL,NULL,test_close,NULL,test_error_get};
 memset(fb_pixels,0xa5,sizeof(fb_pixels));
 assert(app_module_init()==0);assert(rt==&settings_runtime);assert(fb_open());fb_draw();
 /* These are the production bytes incorrectly called as 0x01010000. */
 assert(!native_sleep_retained&&!alarm_modal&&alarm_pixels_valid&&display_settled);
 t5_app_input_t input={0};
 assert(t5_app_get_api(1)->poll(&input,0));assert(!input.exit_requested);
 assert(rt->struct_size<=sizeof(settings_runtime));
 assert(rt->struct_size==(advertised<sizeof(settings_runtime)?advertised:sizeof(settings_runtime)));
 if(advertised>=clock_end)assert(host_clock_reads);else assert(!host_clock_reads);
 if(advertised>=native_size&&exit_fn) {
  assert(host_exit_reads);host_exit=true;
  unsigned touches=test_polls,ticks=test_ticks,reads=host_exit_reads;
  assert(t5_app_get_api(1)->poll(&input,10));
  assert(input.exit_requested&&input.buttons==T5_APP_BUTTON_BACK);
  assert(host_exit_reads==reads+1&&test_polls==touches&&test_ticks==ticks);
  assert(portable_file_browser_close());
  /* The production controller also exits cleanly without a synthetic launch. */
  app_main();assert(!test_launches&&!fb_grant.api);
 } else {assert(!host_exit_reads);assert(portable_file_browser_close());}
 app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!host_retains);
 free(native);printf("Files Runtime extent %s PASS\n",mode);return 0;
}
