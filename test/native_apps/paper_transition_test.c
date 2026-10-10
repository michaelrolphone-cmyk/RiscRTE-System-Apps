/* Real paper controller/adapter over strict completed-image and slow async
 * provider doubles. Timings below describe the fixture, never hardware. */
#define TEST_CUSTOM_CATALOG
#define main toolbar_fixture_main
#include "portable_native_toolbar_test.c"
#undef main
#include "RiscInputNavigationV1.h"
extern void app_main(void);

#ifndef TEST_SCROLL_CATALOG
const t5_app_manifest_t portable_catalog[3]={
 {.display_name="FILES",.file_name="files.elf",.icon="solid:f07c",.compatible=true},
 {.display_name="POINTS",.file_name="points.elf",.icon="solid:f017",.compatible=true},
 {.display_name="SETTINGS",.file_name="settings.elf",.icon="solid:f013",.compatible=true}};
const unsigned portable_catalog_count=3;
#endif
static const char *test_case,*capture_directory;
static bool is_case(const char *name){return !strcmp(test_case,name);}
static bool quick_case(void){return !strncmp(test_case,"quick",5)||!strncmp(test_case,"brightness",10);}
static bool brightness_case(void){return !strncmp(test_case,"brightness",10);}
static uint8_t completed_image[48000],queued_image[48000],padded_pixels[104*480];
static unsigned pending,submitted,complete_count,copies,launch_count,launch_tick,input_reads,bright_value=40;
static unsigned frame_coverage[64],frame_tick[64];
static bool alarm_due,alarm_done,stop_seen,fail_allocation;
#ifdef PORTABLE_STAGE_LOGS
static unsigned log_statements,log_submitted,log_completed,log_opened,log_closed,log_touches;
static bool test_diagnostic(const char *line){
 assert(!strstr(line,"RTE_PERF")&&!strstr(line,"phase="));
 if(!strncmp(line,"APP t_ms=",9)){
  unsigned long long stamp;assert(sscanf(line,"APP t_ms=%llu",&stamp)==1&&stamp==ticks&&strlen(line)<224);
  log_statements++;log_submitted+=strstr(line,"stage=display-submit-end")!=NULL;log_completed+=strstr(line,"stage=display-complete")!=NULL;
  log_opened+=strstr(line,"name=quick-controls-open")!=NULL;log_closed+=strstr(line,"name=quick-controls-close")!=NULL;log_touches+=strstr(line,"stage=touch-picked-up")!=NULL;
 }
 return fx_diagnostic(line);
}
#endif
static unsigned frame_latency=100,screen_stride=100;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size){assert(!retained);if(fail_allocation){fail_allocation=false;return NULL;}return __real_malloc(size);}
static uint8_t *frame_pixels(void){return screen_stride==100?pixels:padded_pixels;}
static void copy_frame(uint8_t *out){for(unsigned y=0;y<480;y++)memcpy(out+y*100,frame_pixels()+y*screen_stride,100);}
static void immutable_frame(void){if(!pending)return;uint8_t check[48000];copy_frame(check);assert(!memcmp(check,queued_image,sizeof(check)));}
static void save_image(const char *name,const uint8_t *data){
 if(!capture_directory)return;
 char path[768];snprintf(path,sizeof(path),"%s/%s.pbm",capture_directory,name);FILE *file=fopen(path,"wb");assert(file);
 fprintf(file,"P4\n800 480\n");assert(fwrite(data,1,48000,file)==48000);assert(!fclose(file));
}
static bool test_info(void *c,risc_display_info_v1 *out){
 fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_BRIGHTNESS;
 out->damage_x_alignment=out->damage_width_alignment=8;
 out->nominal_refresh_millihz=10000;out->typical_present_latency_us=100000;return true;
}
static bool test_frame(void *c,uint32_t format,risc_display_surface_v1 *out){
 assert(!pending);fx_frame(c,format,out);memset(frame_pixels(),0xcd,screen_stride*480);
 out->pixels=frame_pixels();out->stride_bytes=screen_stride;out->size_bytes=screen_stride*480;return true;
}
static void test_frame_release(void *c,risc_display_frame_v1 frame){assert(!pending);fx_frame_release(c,frame);}
static bool test_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(!pending&&presents<64);
#ifdef TEST_SCROLL_CATALOG
 assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
#else
 assert(options->intent==RISC_DISPLAY_PRESENT_QUALITY);
#endif
 if(is_case("submit-false"))return false;
 if(n){assert(n==1&&damage);for(unsigned y=0;y<480;y++)for(unsigned x=0;x<100;x++)
  if(frame_pixels()[y*screen_stride+x]!=completed_image[y*100+x])
   assert((int)x*8>=damage->x&&(int)y>=damage->y&&x*8+8<=(unsigned)damage->x+damage->width&&y<(unsigned)damage->y+damage->height);}
 if(screen_stride>100)for(unsigned y=0;y<480;y++)for(unsigned x=100;x<screen_stride;x++)assert(padded_pixels[y*screen_stride+x]==0xcd);
 frame_coverage[presents]=paper_handoff_old?(1+(ticks-paper_handoff_started)*15/400):16;
 frame_tick[presents]=ticks;
 bool ok=fx_submit(c,frame,damage,n,options,token);assert(ok);copy_frame(queued_image);
 assert(memcmp(queued_image,completed_image,sizeof(queued_image))); /* No alpha-zero or duplicate transmissions. */
 pending=*token;submitted=ticks;
 char name[64];snprintf(name,sizeof(name),"frame-%02u",presents);save_image(name,queued_image);return true;
}
static bool test_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending==token);immutable_frame();
 if(is_case("status-false")&&ticks-submitted>=40)return false;
 if(is_case("superseded")&&ticks-submitted>=40){out->state=RISC_DISPLAY_PRESENT_SUPERSEDED;return true;}
 if(is_case("timeout")||ticks-submitted<frame_latency){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 out->state=RISC_DISPLAY_PRESENT_COMPLETE;pending=0;complete_count++;memcpy(completed_image,queued_image,sizeof(completed_image));return true;
}
static bool test_snapshot(void *c,uint32_t format,void *out,size_t size,uint32_t stride){
 (void)c;io();copies++;assert(!pending&&!frames&&format==RISC_DISPLAY_FORMAT_MONO1&&stride>=100&&size>=stride*480);
 if(is_case("refused"))return false;
 for(unsigned y=0;y<480;y++)memcpy((uint8_t*)out+y*stride,completed_image+y*100,100);
 return true;
}
static bool test_metrics(void *c,risc_display_present_metrics_v1 *out){
 (void)c;io();assert(out->api_version==1&&out->struct_size==sizeof(*out));
 *out=(risc_display_present_metrics_v1){.api_version=1,.struct_size=sizeof(*out),.token=presents,
  .state=pending?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE,.bytes_sent=48000,
  .queued_ms=submitted,.transfer_start_ms=submitted,.transfer_end_ms=ticks,.busy_done_ms=ticks,
  .effective_update={0,0,800,480}};return true;
}
static bool test_seed(void *c,risc_display_frame_v1 frame){(void)c;(void)frame;return false;}
static int32_t test_power(void *c,uint32_t timeout){(void)c;(void)timeout;return RISC_DISPLAY_POWER_OK;}
static risc_display_output_api_v1_snapshot test_display;
static bool test_brightness(void *c,uint16_t value,uint16_t maximum){assert(!pending&&!frames);bright_value=value;return fx_brightness(c,value,maximum);}
static unsigned saved_brightness;
static int32_t test_put(void *c,const char *key,const void *data,uint32_t size){
 if(!strcmp(key,PQA_BRIGHTNESS_KEY)){(void)c;io();assert(!pending&&!frames&&size==1);kv_writes++;saved_brightness=*(const uint8_t*)data;return RISC_KEY_VALUE_OK;}
 return fx_kv_put(c,key,data,size);
}
static int32_t test_get(void *c,const char *key,void *data,uint32_t capacity,uint32_t *size){
 if(!strcmp(key,PQA_BRIGHTNESS_KEY)&&saved_brightness){io();assert(capacity>=1);*(uint8_t*)data=(uint8_t)saved_brightness;*size=1;return RISC_KEY_VALUE_OK;}
 return fx_kv_get(c,key,data,capacity,size);
}
static risc_key_value_v1 test_kv;
static bool test_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();input_reads++;immutable_frame();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 int x=240,y=136;bool down=false;
 if(is_case("launch")||is_case("replace")){down=ticks>=40&&ticks<60;if(is_case("replace")&&ticks>=140&&ticks<160){down=true;x=370;}}
 if(quick_case()){
  if(ticks>=80&&ticks<300){down=true;y=ticks<120?20:ticks<200?200:500;}
  unsigned scale=is_case("brightness-slow")?3:1;
  if(brightness_case()){
   if(ticks>=1000*scale&&ticks<1120*scale){down=true;y=140;x=ticks<1060*scale?100:364;}
   if(ticks>=1300*scale&&ticks<1440*scale){down=true;y=140;x=ticks<1360*scale?364:226;}
  }
  unsigned close=brightness_case()?1800*scale:900;
  if(!is_case("quick-back")&&!is_case("quick-home")&&ticks>=close&&ticks<close+100){down=true;y=ticks<close+40?660:480;}
  if(is_case("quick-repeat")){
   if(ticks>=2200&&ticks<2500){down=true;y=ticks<2300?20:ticks<2400?200:500;}
   if(ticks>=3200&&ticks<3300){down=true;y=ticks<3240?660:480;}
  }
 }
 if(is_case("alarm")&&ticks>=800&&ticks<840){down=true;y=650;}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
 return true;
}
static risc_touch_api_v1 test_touch_api;
static bool test_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};
 unsigned stop=is_case("back")||is_case("home")?140:is_case("brightness-slow")?8000:is_case("brightness")?2600:is_case("quick-repeat")?4200:quick_case()||is_case("alarm")?1800:1600;
 if((is_case("quick-back")||is_case("quick-home"))&&ticks>=900&&ticks<980)out->buttons=out->pressed=is_case("quick-home")?RISC_NAV_HOME:RISC_NAV_BACK;
 if(ticks>=stop){out->buttons=out->pressed=is_case("home")?RISC_NAV_HOME:RISC_NAV_BACK;stop_seen=true;}return true;
}
static bool test_foreground(void *c,const risc_input_foreground_v1 *claims,size_t count){(void)c;(void)claims;(void)count;io();return true;}
static bool test_reset(void *c){(void)c;io();return true;}
static const risc_input_navigation_api_v1 test_navigation={1,sizeof(test_navigation),NULL,test_nav,test_foreground,test_reset};
static int32_t test_alarm_step(void *c){assert(!frames);fx_alarm_step(c);if(is_case("alarm")&&ticks>=120&&!alarm_done)alarm_due=true;return ALARM_OK;}
static int32_t test_alarm_status(void *c,alarm_status_v1 *out){assert(!frames);fx_alarm_status(c,out);if(alarm_due){out->state=ALARM_STATE_ALERT;out->occurrence=(alarm_token_v1){1,1,1,1};}return ALARM_OK;}
static int32_t test_alarm_ack(void *c,const alarm_token_v1 *token){assert(!pending&&alarm_due);fx_alarm_ack(c,token);alarm_due=false;alarm_done=true;return ALARM_OK;}
static alarm_service_v1 test_alarm;
static bool test_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!strcmp(name,"input.navigation")){io();++acquires;++live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=&test_navigation};return true;}
 bool ok=fx_acquire(name,version,instance,out);
 if(!strcmp(name,"display.output"))out->api=&test_display;
 if(!strcmp(name,"input.touch.raw"))out->api=&test_touch_api;
 if(!strcmp(name,"storage.key-value"))out->api=&test_kv;
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY))out->api=&test_alarm;
 return ok;
}
static bool test_release(risc_runtime_capability_v1 *grant){assert(!pending);return fx_release(grant);}
static bool test_launch(const char *path){io();assert(!pending&&!frames);launch_count++;launch_tick=ticks;assert(!paper_handoff_old);assert(path&&*path);return true;}
static void test_yield(uint32_t delay){immutable_frame();assert(ticks<12000);fx_yield(delay);if(is_case("dropped")&&ticks==120)ticks+=600;}

#ifndef PAPER_TRANSITION_MAIN
#define PAPER_TRANSITION_MAIN main
#endif
int PAPER_TRANSITION_MAIN(int argc,char **argv){
 assert(argc==4);test_case=argv[1];capture_directory=argv[3];
 FILE *source=fopen(argv[2],"rb");assert(source);assert(fread(completed_image,1,sizeof(completed_image),source)==sizeof(completed_image));assert(!fclose(source));save_image("outgoing",completed_image);
 test_display.metrics.power.history.base=fx_display;
 test_display.metrics.power.history.base.struct_size=sizeof(test_display);
 test_display.metrics.power.history.base.get_info=test_info;test_display.metrics.power.history.base.acquire=test_frame;
 test_display.metrics.power.history.base.release=test_frame_release;test_display.metrics.power.history.base.submit=test_submit;
 test_display.metrics.power.history.base.present_status=test_status;test_display.metrics.power.history.base.set_brightness=test_brightness;
 test_display.metrics.power.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;test_display.metrics.power.history.extension_version=1;test_display.metrics.power.history.seed_previous=test_seed;
 test_display.metrics.power.power_tag=RISC_DISPLAY_POWER_TAG;test_display.metrics.power.power_version=1;test_display.metrics.power.prepare=test_power;test_display.metrics.power.resume=test_power;
 test_display.metrics.metrics_tag=RISC_DISPLAY_METRICS_TAG;test_display.metrics.metrics_version=RISC_DISPLAY_METRICS_VERSION;test_display.metrics.snapshot=test_metrics;
 test_display.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;test_display.snapshot_version=RISC_DISPLAY_SNAPSHOT_VERSION;test_display.copy_completed=test_snapshot;
 if(is_case("absent"))test_display.metrics.power.history.base.struct_size=sizeof(risc_display_output_api_v1);
 if(is_case("malformed"))test_display.snapshot_tag=0;
 if(is_case("slow")||is_case("quick-slow")||is_case("brightness-slow"))frame_latency=700;
 if(is_case("padded"))screen_stride=104;
 test_touch_api=fx_touch;test_touch_api.snapshot=test_touch;
 test_kv=fx_kv;test_kv.put=test_put;test_kv.get=test_get;
 test_alarm=fx_alarm;test_alarm.step=test_alarm_step;test_alarm.status=test_alarm_status;test_alarm.acknowledge=test_alarm_ack;
 fx_runtime.acquire=test_acquire;fx_runtime.release=test_release;fx_runtime.request_launch=test_launch;fx_runtime.yield_ms=test_yield;
 #ifdef PORTABLE_STAGE_LOGS
 fx_runtime.diagnostic=test_diagnostic;
#endif
 assert(!app_module_init());const paper_presentation *view=paper_presentation_get();assert(view);
 if(is_case("oom"))fail_allocation=true;
 if(is_case("repeat")){portable_paper_transition_begin();portable_paper_transition_begin();assert(copies==2&&paper_handoff_old);}
 app_main();
 if(retained){assert(barriers==1);unsigned before=free_calls;portable_paper_transition_begin();portable_paper_transition_cancel();assert(free_calls==before);check_retained(view);}
 else {assert(!pending);save_image("endpoint",completed_image);app_module_fini();assert(!live&&!frames&&!subscriptions&&!paper_handoff_old);}
 if(is_case("absent")||is_case("malformed")||is_case("oom"))assert(!copies&&presents==1);
 else if(is_case("refused"))assert(copies==1&&presents==1);
 else if(is_case("normal")||is_case("padded")||is_case("repeat"))assert(presents>=4&&presents<=6&&frame_coverage[0]==1&&frame_coverage[presents-1]==16);
 if(is_case("slow"))assert(presents==2&&frame_coverage[1]==16);
 if(is_case("dropped"))assert(presents<=3&&frame_coverage[presents-1]==16&&frame_tick[presents-1]>=720);
 if(is_case("back")||is_case("home"))assert(presents<=2&&stop_seen);
 if(is_case("launch")||is_case("replace"))assert(launch_count==1&&launch_tick>=200&&frame_coverage[presents-1]==16);
 if(is_case("alarm"))assert(alarm_done);
 if(brightness_case()){fprintf(stderr,"brightness fixture writes=%u value=%u calls=%u frames=%u\n",kv_writes,bright_value,brightness_calls,presents);assert(kv_writes==2&&bright_value==50&&brightness_calls>=2);}
 if(is_case("submit-false")||is_case("status-false")||is_case("superseded")||is_case("timeout"))assert(retained&&!launch_count);
 #ifdef PORTABLE_STAGE_LOGS
 assert(log_statements&&log_submitted==presents&&log_completed==complete_count&&log_touches<=input_reads);
 if(quick_case())assert(log_opened==log_closed&&log_opened==(is_case("quick-repeat")?2u:1u));
#endif
 printf("{\"case\":\"%s\",\"frames\":%u,\"completed\":%u,\"snapshot_copies\":%u,\"input_reads\":%u,\"launch_ms\":%u,\"brightness\":%u,\"writes\":%u,\"retained\":%s}\n",test_case,presents,complete_count,copies,input_reads,launch_tick,bright_value,kv_writes,retained?"true":"false");
 return 0;
}
