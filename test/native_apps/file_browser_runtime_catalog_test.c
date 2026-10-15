#define PORTABLE_RUNTIME_APP_CATALOG
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
static uint32_t catalog_rows=24;
static uint64_t catalog_generation=1;
static int catalog_error;
static bool catalog_release_error;
static unsigned catalog_reads,catalog_gets;
static bool catalog_invalidate;
static int32_t catalog_snapshot(void *c,risc_app_catalog_snapshot_v1 *out){
 (void)c;++catalog_reads;if(catalog_error)return catalog_error;
 *out=(risc_app_catalog_snapshot_v1){sizeof(*out),catalog_rows,catalog_generation};return 0;
}
static int32_t catalog_get(void *c,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1 *out){
 (void)c;++catalog_reads;if(catalog_invalidate && ++catalog_gets==2)++catalog_generation;if(generation!=catalog_generation)return RISC_APP_CATALOG_STALE;
 assert(index<catalog_rows);memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->flags=RISC_APP_CATALOG_AVAILABLE;
 snprintf(out->id,sizeof(out->id),"app%u",index);snprintf(out->file_name,sizeof(out->file_name),"app%u.elf",index);snprintf(out->display_name,sizeof(out->display_name),"Application %u",index);if(!index)strcpy(out->file_name,"wifi_settings.elf");return 0;
}
static const risc_app_catalog_v1 catalog_api={1,sizeof(catalog_api),NULL,catalog_snapshot,catalog_get};
static bool host_release(risc_runtime_capability_v1 *g){
 if(g->api==&catalog_api && catalog_release_error)return false;
 return test_release(g);
}
static bool host_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 if(!strcmp(name,RISC_APP_CATALOG_CAPABILITY))grant->api=&catalog_api;
 else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))grant->api=&host_alarm;
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
 risc_runtime_api_v1 base=test_runtime;base.acquire=host_acquire;base.release=host_release;base.retain_invocation=host_retain;base.struct_size=(uint32_t)advertised;
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

 assert(refresh() && count()==24);t5_app_manifest_t selected={0};assert(get(23,&selected)&&!strcmp(selected.file_name,"app23.elf"));
 portable_app_catalog_launch destination;assert(portable_catalog_launch_prepare(&runtime_catalog,23,&destination));
 ++catalog_generation;assert(!portable_catalog_launch_valid(&runtime_catalog,&destination));
 catalog_rows=128;assert(refresh()&&count()==128&&get(127,&selected));
 catalog_rows=129;assert(!refresh()&&count()==0);
 catalog_rows=0;assert(refresh()&&count()==0);
 catalog_rows=24;assert(refresh());
 unsigned launch_before=test_launches;
 assert(launch(0)&&test_launches==launch_before+1);
 catalog_invalidate=true;catalog_gets=0;
 assert(!launch(0)&&test_launches==launch_before+1);
 catalog_invalidate=false;assert(refresh());
 if(!strcmp(mode,"retained") || !strcmp(mode,"context") || !strcmp(mode,"release")) {
  if(!strcmp(mode,"release"))catalog_release_error=true;
  else {catalog_error=!strcmp(mode,"context")?RISC_APP_CATALOG_CONTEXT:RISC_APP_CATALOG_RETAINED;assert(!refresh());}
  app_module_fini();assert(native_custody_retained&&host_retains==1&&runtime_catalog.grant.api==&catalog_api);
  unsigned reads=catalog_reads,grants=test_grants;app_module_fini();assert(reads==catalog_reads&&grants==test_grants);puts("Files catalog terminal custody PASS");return 0;
 }
 app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!host_retains);
 free(native);printf("Files Runtime extent %s PASS\n",mode);return 0;
}
