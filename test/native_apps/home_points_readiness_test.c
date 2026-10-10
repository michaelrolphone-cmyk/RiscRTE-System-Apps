/* Actual Home controller, adapter and selected alarm service. Only hardware,
 * storage and Runtime providers are fixtures. The linker wrapper observes the
 * real adapter API without replacing its result or the service implementation. */
#define SPARSE_FIXTURE_MAIN unused_readiness_sparse_main
#define SPARSE_FIXTURE_MAIN_LINKAGE static __attribute__((unused))
#include "sparse_clock_startup_test.c"
#include "PortableHomePointsCatalog.h"
#include "PointsCatalog.h"
#include "PointsCatalogLedger.h"
#include "PointsServiceProjection.h"
#include "RiscProviderV2.h"
#include "RiscPlatformClockV1.h"
#include "RiscPlatformRealtimeV1.h"
#include "RiscBoundKeyValueV1.h"
#include "RiscBoundAppDataV1.h"

extern const risc_driver_v2 *t5_driver_get(uint32_t);
extern int __real_portable_home_points_catalog(portable_points_catalog_view *);
typedef struct {uint8_t *bytes;uint32_t size;uint64_t revision;} stored_file;
static stored_file stored[2];
static const risc_driver_v2 *service_driver;
static const alarm_service_v1 *real_service;
static unsigned provider_io,legacy_gets,legacy_puts,catalog_writes,ledger_writes;
static unsigned projection_reads,pending_reads,ready_reads,error_reads,retained_io;
static unsigned sample_ms[64],submit_ms[16],submit_projection[16];
static unsigned ready_at,inflight_ready_at,ready_generation,latest_generation;
static int last_projection;
static bool service_fenced,updated;
static risc_realtime_control_api_v1 readiness_realtime;
static risc_input_navigation_api_v1 readiness_nav;
static twatch_rtc_api_v1 readiness_rtc_api;
static bool readiness_navigation(void *,risc_input_navigation_frame_v1 *);
static int32_t readiness_native(void *,risc_realtime_snapshot_v1 *);
static bool readiness_rtc(void *,twatch_rtc_time_v1 *);
static uint8_t first_image[sizeof(pixels)],last_image[sizeof(pixels)];
static uint32_t elapsed(void){return (uint32_t)(ms-start_ms);}
static bool error_case(void){return which("home-storage-error")||which("home-rtc-error");}
static void provider_call(void){assert(!service_fenced&&!terminal);provider_io++;}
static unsigned file_slot(const char *name){
 if(!strcmp(name,POINTS_CATALOG_FILE))return 0;
 assert(!strcmp(name,POINTS_LEDGER_FILE));return 1;
}
static int32_t service_get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *size){
 (void)c;(void)key;(void)out;(void)capacity;provider_call();legacy_gets++;*size=0;
 return RISC_BOUND_KEY_VALUE_NOT_FOUND;
}
static int32_t service_put(void *c,const char *key,const void *bytes,uint32_t size){
 (void)c;(void)key;(void)bytes;(void)size;provider_call();legacy_puts++;assert(!"No legacy startup write is allowed");return RISC_BOUND_KEY_VALUE_IO;
}
static int32_t service_stat(void *c,const char *name,uint32_t *size,uint64_t *revision){
 (void)c;provider_call();*size=0;*revision=0;
 if(which("home-retained")){service_fenced=true;retained_io=provider_io;return RISC_APP_DATA_CONTEXT;}
 if(which("home-storage-error"))return RISC_APP_DATA_IO;
 stored_file *file=&stored[file_slot(name)];if(!file->bytes)return RISC_APP_DATA_NOT_FOUND;
 *size=file->size;*revision=file->revision;return RISC_APP_DATA_OK;
}
static int32_t service_read(void *c,const char *name,uint64_t revision,void *out,uint32_t capacity,uint32_t *size,uint64_t *actual){
 (void)c;provider_call();stored_file *file=&stored[file_slot(name)];
 assert(file->bytes&&revision==file->revision&&capacity>=file->size);
 memcpy(out,file->bytes,file->size);*size=file->size;*actual=file->revision;return RISC_APP_DATA_OK;
}
static int32_t service_replace(void *c,const char *name,uint64_t revision,const void *bytes,uint32_t size){
 (void)c;provider_call();unsigned slot=file_slot(name);if(slot==0)catalog_writes++;else ledger_writes++;
 assert(slot==1);stored_file *file=&stored[slot];assert(revision==file->revision);
 uint8_t *copy=malloc(size);assert(copy);memcpy(copy,bytes,size);free(file->bytes);
 file->bytes=copy;file->size=size;file->revision++;return RISC_APP_DATA_OK;
}
static uint64_t service_monotonic(void *c){(void)c;provider_call();return elapsed();}
static int32_t service_time(void *c,risc_realtime_snapshot_v1 *out){
 (void)c;provider_call();if(which("home-rtc-error"))return RISC_REALTIME_IO;
 *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=RISC_REALTIME_VALID,
  .epoch_seconds=epoch+elapsed()/1000u,.nanoseconds=(elapsed()%1000u)*1000000u,
  .monotonic_before_us=(uint64_t)elapsed()*1000u,.monotonic_after_us=(uint64_t)elapsed()*1000u};return RISC_REALTIME_OK;
}
static const risc_bound_key_value_v1 service_keys={1,sizeof(service_keys),NULL,service_get,service_put};
static const risc_bound_app_data_v1 service_files={1,sizeof(service_files),NULL,service_stat,service_read,service_replace};
static const risc_platform_clock_api_v1 service_clock={1,sizeof(service_clock),NULL,service_monotonic,NULL};
static const risc_platform_realtime_api_v1 service_realtime={1,sizeof(service_realtime),NULL,service_time};
static const risc_provider_dependency_v1 service_dependencies[]={
 {"storage.key-value.bound",1,&service_keys},{"storage.app-data.bound",1,&service_files},
 {"platform.clock",1,&service_clock},{"platform.realtime",1,&service_realtime}
};
static void save_catalog_fixture(bool empty,bool changed){
 points_config legacy=empty?(points_config){.revision=9}:points_default_config();
 points_meta meta=points_default_meta();points_catalog catalog={0};
 assert(!points_catalog_migrate(&catalog,&legacy,&meta,POINTS_TIME_NATIVE_UTC));
 if(changed){points_catalog_item event=catalog.events[4];event.minute=1;
  assert(!points_catalog_update_event(&catalog,&event));}
 uint32_t size,used;assert(points_catalog_size(catalog.event_count,catalog.type_count,&size));
 uint8_t *bytes=malloc(size);assert(bytes&&!points_catalog_encode(&catalog,bytes,size,&used));
 free(stored[0].bytes);stored[0]=(stored_file){bytes,used,stored[0].revision+1};points_catalog_dispose(&catalog);
}
static void check_factory(void){
 points_config legacy=points_default_config();points_meta meta=points_default_meta();points_catalog catalog={0};
 assert(legacy.revision==1&&!points_catalog_migrate(&catalog,&legacy,&meta,POINTS_TIME_NATIVE_UTC));
 assert(catalog.revision==1&&catalog.event_count==7);
 static const uint8_t hours[]={4,5,6,9,12,14,16},minutes[]={30,30,0,0,0,15,30};
 for(unsigned i=0;i<7;i++)assert(catalog.events[i].revision==1&&catalog.events[i].enabled&&
  catalog.events[i].weekdays==30&&catalog.events[i].hour==hours[i]&&catalog.events[i].minute==minutes[i]);
 points_catalog_dispose(&catalog);
}
int __wrap_portable_home_points_catalog(portable_points_catalog_view *out){
 /* Read-only copied service state is safe while an immutable image is pending. */
 assert(!frames);unsigned before=provider_io;
 portable_points_catalog_view saved=*out;int result=__real_portable_home_points_catalog(out);
 assert(provider_io==before);assert(projection_reads<64);sample_ms[projection_reads++]=elapsed();
 last_projection=result;
 if(result==2){pending_reads++;assert(!memcmp(&saved,out,sizeof(saved)));}
 else if(result==1){ready_reads++;assert(portable_points_catalog_view_valid(out));
  if(!ready_at){ready_at=elapsed();ready_generation=out->snapshot;}latest_generation=out->snapshot;
  if(which("home-empty"))assert(!out->count&&out->catalog_revision==9);
  else assert(out->count&&out->catalog_revision>=1);
 }else if(result==-1)error_reads++;
 return result;
}
static bool readiness_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){safe();assert(version==2&&!instance);
  grant->api=real_service;grant->slot=++grants;grant->generation=1;if(grants>high_water)high_water=grants;return true;}
 if(!obtain(name,version,instance,grant))return false;
 if(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY))grant->api=&readiness_realtime;
 else if(!strcmp(name,"input.navigation"))grant->api=&readiness_nav;
 else if(!strcmp(name,"rtc.clock"))grant->api=&readiness_rtc_api;
 return true;
}
static bool readiness_drop(risc_runtime_capability_v1 *grant){
 if(grant->api==&readiness_realtime)grant->api=&realtime;
 return drop(grant);
}
static bool readiness_health(risc_runtime_health_v1 *out){
 io();out->uptime_ms=ms;assert(elapsed()<10000);
 if(!updated&&elapsed()>=1100&&(which("home-generation")||which("home-catalog-change"))){
  assert(ready_reads);if(which("home-catalog-change"))save_catalog_fixture(false,true);
  assert(real_service->refresh(real_service->context)==ALARM_PENDING);updated=true;
 }
 return true;
}
static bool readiness_navigation(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;safe();*out=(risc_input_navigation_frame_v1){0};
 if(error_case()&&elapsed()<2200&&presents>=2&&polls%4==0)out->pressed=out->released=RISC_NAV_BACK;
 else if(elapsed()>=2400&&!raw_present_pending)out->pressed=out->released=RISC_NAV_CONFIRM;
 return true;
}
static bool readiness_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;safe();*out=(risc_touch_snapshot_v1){.width=480,.height=800};return true;
}
static int32_t readiness_native(void *c,risc_realtime_snapshot_v1 *out){
 /* The boot recovery sample is valid; the first actual foreground samples
  * then report temporarily unset time without creating an RTC custody error. */
 if(which("home-delayed-time")&&native_reads&&elapsed()<1200){safe();assert(c==&native_context&&native_live);native_reads++;
  *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=RISC_REALTIME_UNSET,
   .monotonic_before_us=(uint64_t)ms*1000u,.monotonic_after_us=(uint64_t)ms*1000u};return RISC_REALTIME_OK;}
 return read_native(c,out);
}
static bool readiness_rtc(void *c,twatch_rtc_time_v1 *out){
 if(which("home-delayed-time")&&elapsed()<1200){(void)c;(void)out;safe();rtc_reads++;return false;}
 return read_rtc(c,out);
}
static bool readiness_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *rect,size_t count,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(presents<16);submit_ms[presents]=elapsed();submit_projection[presents]=(unsigned)(last_projection+2);
 if(!presents)memcpy(first_image,pixels,sizeof(pixels));
 memcpy(last_image,pixels,sizeof(pixels));
 return frame_submit(c,frame,rect,count,options,token);
}
static bool readiness_present_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)c;safe();assert(token==presents);
 if(which("home-inflight")&&presents==1&&elapsed()>=100){
  /* A second admitted client advances the real singleton while the first
   * submitted image is in flight. No provider result is fabricated. */
  assert(real_service->step(real_service->context)==ALARM_OK);
  points_catalog_projection projection={.struct_size=sizeof(projection)};
  if(points_service_project(real_service,&projection)==ALARM_OK&&!inflight_ready_at){
   assert(raw_present_pending);inflight_ready_at=elapsed();}
 }
 unsigned duration=which("home-inflight")&&presents==1?900u:140u;
 out->state=(uint32_t)(ms-raw_submitted_at)<duration?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE){memcpy(physical,pixels,sizeof(physical));raw_present_pending=false;}
 return true;
}
static bool readiness_wait(void *c,risc_display_present_token_v1 token,uint32_t timeout,risc_display_present_status_v1 *out){
 bool result=frame_wait(c,token,timeout,out);raw_present_pending=false;return result;
}
static void write_images(const char *prefix){
 char path[1024];const uint8_t *images[]={first_image,last_image};const char *names[]={"first","last"};
 for(unsigned i=0;i<2;i++){assert(snprintf(path,sizeof(path),"%s.%s.pbm",prefix,names[i])>0);
  FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P4\n800 480\n");
  assert(fwrite(images[i],1,sizeof(pixels),file)==sizeof(pixels));assert(!fclose(file));}
}
int main(int argc,char **argv){
 assert(argc==3);test=argv[1];state_path=argv[2];raw_async=true;epoch=1791369720u;
 if(which("home-wrap"))ms=UINT32_MAX-600u;
 start_ms=ms;check_factory();
 if(which("home-empty"))save_catalog_fixture(true,false);
 service_driver=t5_driver_get(2);assert(service_driver);real_service=service_driver->capability;
 assert(service_driver->start(service_dependencies,sizeof(service_dependencies)/sizeof(service_dependencies[0])));
 /* Repeated reads of actual not-yet-reconciled state remain pending and do
  * not touch storage, time or the caller's supplied output. */
 for(unsigned i=0;i<12;i++){points_catalog_projection projection={.struct_size=sizeof(projection)},before=projection;
  unsigned calls=provider_io;assert(points_service_project(real_service,&projection)==ALARM_PENDING);
  assert(provider_io==calls&&!memcmp(&before,&projection,sizeof(before)));}
 if(error_case()){for(unsigned i=0;i<64;i++){
   int result=real_service->step(real_service->context);if(result<0)break;}
  points_catalog_projection projection={.struct_size=sizeof(projection)};
  assert(points_service_project(real_service,&projection)==(which("home-storage-error")?ALARM_STORAGE:ALARM_RTC));}
 readiness_realtime=realtime;readiness_realtime.read=readiness_native;
 readiness_nav=navigation;readiness_nav.poll=readiness_navigation;
 readiness_rtc_api=rtc_api;readiness_rtc_api.read=readiness_rtc;
 runtime.acquire=readiness_acquire;runtime.release=readiness_drop;runtime.health=readiness_health;
 panel.history.base=d;panel.history.base.struct_size=sizeof(panel);panel.history.base.get_info=raw_display_info;
 panel.history.base.acquire=frame_acquire;panel.history.base.release=frame_release;
 panel.history.base.submit=readiness_submit;panel.history.base.present_status=readiness_present_status;
 panel.history.base.wait_present=readiness_wait;panel.history.base.set_brightness=bright;
 panel.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;panel.history.extension_version=1;panel.history.seed_previous=seed_previous;
 panel.power_tag=RISC_DISPLAY_POWER_TAG;panel.power_version=1;panel.prepare=panel_prepare;panel.resume=panel_resume;
 touch_power.base=t;touch_power.base.struct_size=sizeof(touch_power);touch_power.base.snapshot=readiness_touch;
 touch_power.power_tag=RISC_TOUCH_POWER_TAG;touch_power.power_version=1;touch_power.prepare=touch_prepare;touch_power.resume=touch_resume;
 power=(x4_power_deep_v1){{1,sizeof(power),NULL,read_key,NULL},X4_POWER_DEEP_TAG,1,deep};
 native_valid=true;in_main=true;assert(app_module_init()==0);assert(!grants&&!starts&&!kv_reads&&!native_reads);
 app_main();
 if(which("home-retained")){
  assert(terminal&&service_fenced&&portable_app_sleep_retained()&&barriers==1);
  assert(provider_io==retained_io);unsigned calls=provider_io,held=grants;
  portable_points_catalog_view view={.struct_size=sizeof(view)};assert(__real_portable_home_points_catalog(&view)==-2);
  assert(real_service->step(NULL)==ALARM_RETAINED);assert(!service_driver->quiesce());
  app_module_fini();assert(provider_io==calls&&grants==held&&!launches);
 }else{
  assert(!terminal&&launches==1&&!strcmp(launched,"springboard.elf"));app_module_fini();
  assert(!grants&&!subs&&!frames&&!native_live&&!home_acquires&&!home_reads);
#ifdef READINESS_EXPECT_BASELINE
  points_catalog_projection actual={.struct_size=sizeof(actual)};
  assert(points_service_project(real_service,&actual)==ALARM_OK&&actual.count);
  assert(presents==1&&projection_reads==1&&error_reads==1&&!ready_reads&&!pending_reads);
  assert(service_driver->quiesce());write_images(state_path);
  printf("%s: DEFECT REPRODUCED: service ready with %u rows, Home remains its sole error frame; copies=%u\n",test,actual.count,projection_reads);
  return 42;
#endif
  assert(service_driver->quiesce());
  assert(projection_reads<=1u+elapsed()/250u+3u);
  for(unsigned i=1;i<projection_reads;i++)assert(sample_ms[i]-sample_ms[i-1]>=250u);
  if(error_case())assert(error_reads&&!ready_reads&&!pending_reads&&submit_projection[0]==1);
  else {
   assert(ready_reads&&latest_generation);
   points_catalog_ledger ledger={0};
   assert(stored[1].bytes&&!points_catalog_ledger_decode(&ledger,stored[1].bytes,stored[1].size));
   assert(ledger.count==(which("home-empty")?0u:7u));
   if(!which("home-empty")&&!which("home-catalog-change")){
    assert(!stored[0].bytes&&ledger.revision==1);
    for(unsigned i=0;i<ledger.count;i++)assert(ledger.entries[i].revision==1);
   }
   points_catalog_ledger_dispose(&ledger);
   if(which("home-delayed-time"))assert(ready_at>=1200&&!pending_reads&&presents==2);
   else {assert(pending_reads==1&&submit_projection[0]==4&&ready_at<1000);assert(presents==(which("home-catalog-change")?3u:2u));}
   if(which("home-inflight"))assert(inflight_ready_at&&inflight_ready_at<900&&ready_at>=inflight_ready_at&&ready_at<900);
   if(which("home-generation")||which("home-catalog-change"))assert(updated&&latest_generation>ready_generation);
   assert(memcmp(first_image,last_image,sizeof(pixels)));
  }
 }
 assert(!legacy_puts&&!catalog_writes);write_images(state_path);
 printf("%s: presents=%u copies=%u pending=%u ready=%u errors=%u ready_ms=%u ledger_writes=%u generation=%u/%u PASS\n",
  test,presents,projection_reads,pending_reads,ready_reads,error_reads,ready_at,ledger_writes,ready_generation,latest_generation);
 for(unsigned i=0;i<2;i++)free(stored[i].bytes);
 return 0;
}
