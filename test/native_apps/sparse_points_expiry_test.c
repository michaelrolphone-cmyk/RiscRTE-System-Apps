/* Actual Clock + adapter + product sleep hook, using strict shared doubles.
 * Only the native retained transport and catalog suffix are extended here. */
#define SPARSE_FIXTURE_MAIN unused_sparse_fixture_main
#include "sparse_clock_startup_test.c"
#include "PortableHomePointsCatalog.h"
#include "PointsServiceProjection.h"
static uint8_t retained_bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES];
static portable_desk_record initial,committed;
static unsigned refreshes,steps,copies,statuses,clean_frames,partial_frames;
static bool reconciling,alarm_prepared;
static points_catalog_projection cached;
static uint32_t rtc_now(void){return epoch-946684800u+(ms-start_ms)/1000u;}
static void service_safe(void){safe();assert(!frames&&!raw_present_pending&&!alarm_prepared);}
static int32_t expiry_status(void *c,alarm_status_v1 *out) {
 (void)c;safe();statuses++;
 if(which("status-terminal") || (which("pump-status-terminal")&&steps)){terminal=true;return ALARM_RETAINED;}
 *out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),
  .state=reconciling?ALARM_STATE_LOADING:ALARM_STATE_READY,.snapshot=steps>=3?2:1};
 if(which("uncertain") || (which("step-uncertain")&&steps))out->output_uncertain=1;
 if(which("blocked"))out->state=ALARM_STATE_BLOCKED;
 if(which("owned"))out->state=ALARM_STATE_CUE;
 if(which("rtc-error")&&steps)out->error=ALARM_RTC;
 if(which("due")&&steps)out->state=ALARM_STATE_CUE;
 return ALARM_OK;
}
static int32_t expiry_refresh(void *c) {
 (void)c;service_safe();refreshes++;reconciling=true;
 if(which("refresh-terminal")){terminal=true;return ALARM_RETAINED;}
 return ALARM_PENDING;
}
static int32_t expiry_step(void *c) {
 (void)c;service_safe();steps++;
 if(which("step-terminal")){terminal=true;return ALARM_RETAINED;}
 if(which("rtc-error"))return ALARM_RTC;
 if(steps==3 && !which("budget")) {
  reconciling=false;cached=initial.config.points.view;cached.struct_size=sizeof(cached);cached.snapshot=2;
  cached.catalog_revision=9;cached.seconds=rtc_now();cached.valid_until=cached.seconds+3600;
  cached.count=1;cached.has_previous=0;
  cached.next[0]=(points_catalog_event){.event_id=77,.type_id=90000,.revision=8,.symbol=2,.color=6,
   .deadline=cached.seconds+600,.parent_day=9000};strcpy(cached.next[0].label,"Refreshed full catalog label");
  if(which("stale-result"))cached.valid_until=cached.seconds;
  if(which("future-result")){cached.seconds+=120;cached.valid_until+=120;cached.next[0].deadline+=120;}
  if(which("old-snapshot"))cached.snapshot=1;
  if(which("cross-minute"))ms+=3000;
 }
 return ALARM_OK;
}
static int32_t expiry_projection(void *c,points_catalog_projection *out) {
 (void)c;service_safe();copies++;
 /* Deliberately return old successful data while loading, like the service. */
 if(which("projection-terminal")){terminal=true;return ALARM_OUTPUT;}
 if(which("projection-error"))return ALARM_STORAGE;
 *out=cached;return ALARM_OK;
}
static int32_t expiry_prepare(void *c,alarm_sleep_v1 *out) {
 (void)c;safe();alarm_prepared=true;out->rtc_seconds=epoch+(ms-start_ms)/1000u;out->deadline=0;return ALARM_OK;
}
static int32_t expiry_read(void *c,uint32_t type,uint32_t schema,void *out,uint32_t cap,uint32_t *size,uint32_t *cause) {
 (void)c;safe();assert(type==PORTABLE_DESK_CLOCK_RECORD_TYPE&&schema==3&&cap==408);
 memcpy(out,retained_bytes,408);*size=408;*cause=RISC_BOOT_DEEP_TIMER;return RISC_RETAINED_WAKE_OK;
}
static int32_t expiry_stage(void *c,uint32_t type,uint32_t schema,const void *bytes,uint32_t size) {
 (void)c;safe();assert(type==PORTABLE_DESK_CLOCK_RECORD_TYPE&&schema==3&&size==408);
 assert(!frames&&!native_live&&portable_desk_decode(bytes,size,&committed));
 memcpy(retained_bytes,bytes,size);pending=true;return RISC_RETAINED_WAKE_OK;
}
static const risc_retained_wake_api_v1_extended expiry_wake={
 .base={1,sizeof(expiry_wake),NULL,read_record,stage_record,clear_record},
 .extension_tag=RISC_RETAINED_WAKE_EXTENDED_TAG,.extension_version=1,.max_payload_bytes=512,
 .read_bytes=expiry_read,.stage_bytes=expiry_stage};
static points_service_v2 expiry_service;
static bool expiry_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g) {
 safe();assert(strcmp(name,RISC_PROVIDER_PROMOTION_CAPABILITY));
 const void *api=NULL;
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==2&&!id);api=&expiry_service;}
 else if(!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY))api=&expiry_wake;
 else return obtain(name,version,id,g);
 g->api=api;g->slot=++grants;g->generation=1;if(grants>high_water)high_water=grants;return true;
}
static bool expiry_brightness(void *c,uint16_t level,uint16_t max){assert(!level);return bright(c,level,max);}
static bool expiry_seed(void *c,risc_display_frame_v1 frame) {
 /* The prior-image pixel oracle lives in the retained renderer suite. Here
  * assert that refreshing a window never offers a seed for different data. */
 assert(!refreshes);memcpy(physical,pixels,sizeof(physical));loaded_pixels=true;
 return seed_previous(c,frame);
}
static bool expiry_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *o,risc_display_present_token_v1 *out) {
 assert(o->intent!=RISC_DISPLAY_PRESENT_LOW_LATENCY);
 if(o->intent==RISC_DISPLAY_PRESENT_CLEAN)clean_frames++;else partial_frames++;
 return frame_submit(c,f,r,n,o,out);
}
static int32_t expiry_deep(void *c,uint32_t duration) {
 (void)c;io();assert(pending&&duration&&duration<=60000&&!subs&&!frames&&dark&&panel_off&&!native_live);
 assert(!touch_off&&!sd_off&&alarm_prepared);entries++;terminal=true;longjmp(entry,1);
}
static void save_image(const char *path){FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n800 480\n");assert(fwrite(physical,1,sizeof(physical),f)==sizeof(physical));assert(!fclose(f));
 char record[1024];assert(snprintf(record,sizeof(record),"%s.record",path)>0);f=fopen(record,"wb");assert(f);
 assert(fwrite(retained_bytes,1,sizeof(retained_bytes),f)==sizeof(retained_bytes));assert(!fclose(f));}
int main(int argc,char **argv) {
 assert(argc==3);test=argv[1];state_path=argv[2];raw_async=getenv("EXPIRY_ASYNC")!=NULL;
 runtime.acquire=expiry_acquire;
 panel.history.base=d;panel.history.base.struct_size=sizeof(panel);panel.history.base.get_info=raw_display_info;
 panel.history.base.acquire=frame_acquire;panel.history.base.release=frame_release;panel.history.base.submit=expiry_submit;
 panel.history.base.wait_present=frame_wait;panel.history.base.present_status=raw_present_status;panel.history.base.set_brightness=expiry_brightness;
 panel.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;panel.history.extension_version=1;panel.history.seed_previous=expiry_seed;
 panel.power_tag=RISC_DISPLAY_POWER_TAG;panel.power_version=1;panel.prepare=panel_prepare;panel.resume=panel_resume;
 power=(x4_power_deep_v1){{1,sizeof(power),NULL,read_key,NULL},X4_POWER_DEEP_TAG,1,expiry_deep};
 expiry_service.base=(alarm_service_descriptor_v2){.base=alarm_api,.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1};
 expiry_service.base.base.api_version=2;expiry_service.base.base.struct_size=sizeof(expiry_service);
 expiry_service.base.base.status=expiry_status;expiry_service.base.base.refresh=expiry_refresh;
 expiry_service.base.base.step=expiry_step;expiry_service.base.base.prepare_sleep=expiry_prepare;
 expiry_service.points=(points_service_projection_suffix){POINTS_SERVICE_PROJECTION_TAG,1,expiry_projection};
 initial=(portable_desk_record){.config={.face=PORTABLE_DESK_POINTS,.time_format=1,.rtc_stores_utc=1},
  .displayed_minute=1791331140,.refresh_modulo=1,.has_image=true};strcpy(initial.config.time_zone,"UTC");
 if(getenv("EXPIRY_FLIP"))initial.config.flip_ui=(uint8_t)atoi(getenv("EXPIRY_FLIP"));
 initial.config.points.status=PORTABLE_DESK_POINTS_READY;
 initial.config.points.view=(points_catalog_projection){.struct_size=sizeof(cached),.catalog_revision=7,
  .snapshot=1,.seconds=rtc_now()-120,.valid_until=rtc_now(),.count=1};
 initial.config.points.view.next[0]=(points_catalog_event){.event_id=10,.type_id=11,.revision=1,.symbol=7,.color=3,
  .deadline=rtc_now()+600,.parent_day=9000};strcpy(initial.config.points.view.next[0].label,"Previous copied point");
 if(which("valid")||which("other-face"))initial.config.points.view.valid_until+=3600;
 if(which("other-face"))initial.config.face=PORTABLE_DESK_SEGMENTS;
 if(which("rewind")){initial.config.points.view.seconds+=240;initial.config.points.view.valid_until+=3600;initial.displayed_minute+=180;}
 if(which("rewind-in-window")){initial.config.points.view.valid_until+=3600;initial.displayed_minute+=180;}
 if(which("unavailable"))initial.config.points=(portable_desk_points_snapshot){0};
 if(getenv("EXPIRY_RECORD")) {
  FILE *f=fopen(getenv("EXPIRY_RECORD"),"rb");assert(f);
  assert(fread(retained_bytes,1,sizeof(retained_bytes),f)==sizeof(retained_bytes));assert(!fclose(f));
  assert(portable_desk_decode(retained_bytes,sizeof(retained_bytes),&initial));
  epoch=which("expired")?946684800u+initial.config.points.view.valid_until:(uint32_t)initial.displayed_minute+60;
 }
 cached=initial.config.points.view;
 assert(portable_desk_encode(&initial,retained_bytes,sizeof(retained_bytes)));
 native_valid=true;in_main=true;start_ms=ms;assert(app_module_init()==0);assert(!grants&&!starts&&!kv_reads&&!native_reads);
 if(setjmp(entry)) {
  assert(entries==1&&terminal&&grants&&!subs&&!frames&&!native_live);
  assert(!promoted&&!promotions&&!kv_reads&&!home_reads&&!home_acquires&&!rtc_reads&&!battery_acquires&&!launches);
  assert(high_water<=6&&!raw_brightness_writes&&portable_desk_adapter_timer_only());
  bool unchanged=which("valid")||which("other-face");
  if(unchanged) {
   assert(!refreshes&&!steps&&!copies);
   assert(committed.config.points.view.next[0].symbol==initial.config.points.view.next[0].symbol);
   assert(committed.config.points.view.next[0].color==initial.config.points.view.next[0].color);
  }
  else {
   bool refused=which("blocked")||which("owned");
   assert(refreshes==(refused?0u:1u)&&steps<=64&&clean_frames==1&&!seeds);
   if(refused)assert(!steps&&!copies);
   else if(which("budget"))assert(steps==64&&!copies);
   else if(which("due")||which("rtc-error"))assert(steps==1&&!copies);
   else assert(steps==3&&copies==1);
   bool error=refused||which("budget")||which("due")||which("rtc-error")||which("projection-error")||which("stale-result")||which("future-result")||which("old-snapshot");
   assert(committed.config.points.status==(error?PORTABLE_DESK_POINTS_ERROR:PORTABLE_DESK_POINTS_READY));
   if(!error)assert(committed.config.points.view.catalog_revision==9&&
    committed.config.points.view.next[0].symbol==2&&committed.config.points.view.next[0].color==6&&
    committed.config.points.view.next[0].type_id==90000&&!strcmp(committed.config.points.view.next[0].label,"Refreshed full catalog label"));
  }
  assert(committed.config.face==initial.config.face&&committed.config.flip_ui==initial.config.flip_ui);
  save_image(state_path);printf("TIMER %s: refresh=%u steps=%u copies=%u clean=%u grants=%u; dark, no foreground policy PASS\n",test,refreshes,steps,copies,clean_frames,high_water);return 0;
 }
 app_main();assert(portable_app_sleep_retained()&&terminal&&barriers==1&&!entries&&!presents);
#ifndef TEST_POINTS_EXPIRY_BASELINE
 portable_points_catalog_view ignored={.struct_size=sizeof(ignored)};
 assert(portable_desk_points_catalog_refresh(&ignored)==-2);
#endif
 unsigned held=grants;app_module_fini();assert(grants==held&&!promotions&&!kv_reads&&!battery_acquires&&!launches);
 printf("TIMER %s: silent terminal fence and no later provider/cleanup calls PASS\n",test);return 0;
}
