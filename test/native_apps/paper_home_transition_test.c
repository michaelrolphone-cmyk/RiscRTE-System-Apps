/* Actual native Home/Points controller and sparse adapter. The old Springboard
 * image is an explicitly completed provider fixture, never acquired contents. */
#define SPARSE_FIXTURE_MAIN sparse_fixture_main
#include "sparse_clock_startup_test.c"
#include "RiscDisplayOutputSnapshotV1.h"
static risc_display_output_api_v1_snapshot transition_panel;
static const char *transition_directory;
static unsigned snapshot_copies,transition_frames;
static bool quick_scenario,quick_swipe,quick_interrupted,quick_slow,quick_input_started,quick_exit_sent;
static bool native_home_seen,controls_open,controls_closed;
static unsigned native_home_at,controls_opened_at,controls_closed_at,restored_frame;
static unsigned stage_statements,stage_submits,stage_completes,stage_opens,stage_closes;
static uint8_t native_home[48000],submitted_image[48000];
static risc_display_rect_v1 last_damage;
static size_t last_damage_count;
static FILE *stage_capture,*frame_records;
static void transition_save(const char *name,const uint8_t *image){
 char path[768];snprintf(path,sizeof(path),"%s/%s.pbm",transition_directory,name);FILE *file=fopen(path,"wb");assert(file);
 fprintf(file,"P4\n800 480\n");assert(fwrite(image,1,48000,file)==48000);assert(!fclose(file));
}
static bool inherited_image,without_snapshot;
static bool transition_metrics(void *c,risc_display_present_metrics_v1 *out){
 (void)c;safe();assert(out->api_version==1&&out->struct_size==sizeof(*out));
 *out=(risc_display_present_metrics_v1){.api_version=1,.struct_size=sizeof(*out),.token=presents,
  .state=raw_present_pending?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE,
  .bytes_sent=48000,.mode=last_damage_count?RISC_DISPLAY_METRICS_PARTIAL:RISC_DISPLAY_METRICS_FULL,
  .queued_ms=raw_submitted_at,.transfer_start_ms=raw_submitted_at,.transfer_end_ms=ms,.busy_done_ms=ms,
  .damage_count=(uint32_t)last_damage_count,.effective_update=last_damage_count?last_damage:(risc_display_rect_v1){0,0,800,480}};
 return true;
}
static bool transition_report(const char *line){
 if(!strncmp(line,"APP t_ms=",9)){
  unsigned long long stamp;assert(sscanf(line,"APP t_ms=%llu",&stamp)==1&&stamp==ms&&strlen(line)<224);
  assert(!strstr(line,"RTE_PERF")&&!strstr(line,"phase="));stage_statements++;
  stage_submits+=strstr(line,"stage=display-submit-end")!=NULL;stage_completes+=strstr(line,"stage=display-complete")!=NULL;
  if(strstr(line,"name=quick-controls-open")){stage_opens++;controls_open=true;controls_opened_at=ms;}
  if(strstr(line,"name=quick-controls-close")){stage_closes++;controls_open=false;controls_closed=true;controls_closed_at=ms;}
  if(stage_capture)fprintf(stage_capture,"%s\n",line);
 }
 return report(line);
}
static bool transition_health(risc_runtime_health_v1 *out){
 if(!quick_scenario)return healthy(out);
 io();assert(ms<15000);out->uptime_ms=ms;return true;
}
static bool transition_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;safe();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(!quick_interrupted&&!native_home_seen)return true;
 uint32_t origin=quick_interrupted?0:native_home_at;
 uint32_t start=origin+(quick_interrupted?80u:160u),close=origin+(quick_slow?3600u:2400u);
 int x=240,y=40;bool down=false;
 if(ms>=start&&ms<start+(quick_swipe?300u:120u)){
  down=true;quick_input_started=true;
  if(quick_swipe)y=ms<start+60?20:ms<start+160?200:500;
 }
 if(ms>=close&&ms<close+180){down=true;y=ms<close+60?675:480;}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
 return true;
}
static bool transition_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;safe();*out=(risc_input_navigation_frame_v1){0};
 if(controls_closed&&!raw_present_pending&&!memcmp(physical,native_home,sizeof(physical))&&!quick_exit_sent){
  transition_save("restored",physical);restored_frame=transition_frames;quick_exit_sent=true;
  out->pressed=out->released=RISC_NAV_CONFIRM;
 }
 return true;
}
static bool transition_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)c;safe();assert(token==presents&&!frames&&raw_present_pending&&!memcmp(pixels,submitted_image,sizeof(pixels)));
 unsigned latency=quick_slow?350:140;
 out->state=ms-raw_submitted_at<latency?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE){
  memcpy(physical,pixels,sizeof(pixels));home_completed_frame();raw_present_pending=false;
  if(quick_scenario&&!native_home_seen&&!memcmp(physical,native_home,sizeof(physical))){native_home_seen=true;native_home_at=ms;}
 }
 return true;
}
static bool transition_acquire_frame(void *c,uint32_t format,risc_display_surface_v1 *out){assert(!raw_present_pending);return frame_acquire(c,format,out);}
static bool transition_copy(void *c,uint32_t format,void *out,size_t size,uint32_t stride){
 (void)c;safe();snapshot_copies++;assert(!frames&&!raw_present_pending&&format==RISC_DISPLAY_FORMAT_MONO1&&stride==100&&size==48000);
 if(!inherited_image&&!presents)return false;
 memcpy(out,physical,48000);return true;
}
static bool transition_info(void *c,risc_display_info_v1 *out){raw_display_info(c,out);out->flags&=~RISC_DISPLAY_INFO_CLEAN_PRESENT;if(quick_scenario)out->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;return true;}
static bool transition_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(options->intent==RISC_DISPLAY_PRESENT_QUALITY);
 char name[64];snprintf(name,sizeof(name),"frame-%02u",++transition_frames);transition_save(name,pixels);
 if(frame_records)fprintf(frame_records,"%u,%u,%s,%zu,%d,%d,%u,%u\n",transition_frames,ms,controls_open?"controls":controls_closed?"after-close":quick_input_started?"feedback":"home",count,count?damage->x:0,count?damage->y:0,count?damage->width:800,count?damage->height:480);
 last_damage_count=count;if(count){assert(count==1);last_damage=*damage;}memcpy(submitted_image,pixels,sizeof(pixels));
 return frame_submit(c,frame,damage,count,options,token);
}
static bool transition_obtain(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 if(!obtain(name,version,instance,grant))return false;
 if(!strcmp(name,"display.output")){
  transition_panel.metrics.power=panel;
  transition_panel.metrics.power.history.base.get_info=transition_info;
  transition_panel.metrics.power.history.base.acquire=transition_acquire_frame;
  if(raw_async)transition_panel.metrics.power.history.base.present_status=transition_status;
  transition_panel.metrics.power.history.base.submit=transition_submit;
  transition_panel.metrics.power.history.base.struct_size=without_snapshot?sizeof(panel):sizeof(transition_panel);
  transition_panel.metrics.metrics_tag=RISC_DISPLAY_METRICS_TAG;transition_panel.metrics.metrics_version=1;transition_panel.metrics.snapshot=transition_metrics;
  transition_panel.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;transition_panel.snapshot_version=1;transition_panel.copy_completed=transition_copy;
  grant->api=&transition_panel;
 }
 if(quick_scenario&&!strcmp(name,"input.touch.raw")){static risc_touch_power_api_v1 input;input=touch_power;input.base.snapshot=transition_touch;grant->api=&input;}
 if(quick_scenario&&!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 nav;nav=navigation;nav.poll=transition_nav;grant->api=&nav;}
 return true;
}
int main(int argc,char **argv){
 assert(argc==6);transition_directory=argv[5];quick_scenario=!strncmp(argv[1],"home-quick-",11);
 quick_swipe=strstr(argv[1],"swipe")||strstr(argv[1],"interrupt");quick_interrupted=strstr(argv[1],"interrupt")!=NULL;quick_slow=strstr(argv[1],"slow")!=NULL;
 inherited_image=!strcmp(argv[1],"home-ready")||quick_scenario;without_snapshot=!strcmp(argv[4],"absent");
 char output[768];snprintf(output,sizeof(output),"%s/stage.log",transition_directory);stage_capture=fopen(output,"w");assert(stage_capture);
 snprintf(output,sizeof(output),"%s/frames.csv",transition_directory);frame_records=fopen(output,"w");assert(frame_records);fprintf(frame_records,"frame,submitted_ms,phase,damage_count,x,y,width,height\n");
 if(quick_scenario){FILE *home=fopen(getenv("NATIVE_HOME_BASELINE"),"rb");assert(home);assert(fread(native_home,1,sizeof(native_home),home)==sizeof(native_home));assert(!fclose(home));}
 FILE *file=fopen(argv[3],"rb");assert(file);assert(fread(physical,1,sizeof(physical),file)==sizeof(physical));assert(!fclose(file));
 runtime.acquire=transition_obtain;runtime.diagnostic=transition_report;runtime.health=transition_health;
 char *args[]={argv[0],argv[1],argv[2]};int result=sparse_fixture_main(3,args);assert(!result);
 if(!strcmp(argv[1],"terminal"))assert(!snapshot_copies&&!promoted&&transition_frames<=1);
 else if(without_snapshot)assert(!snapshot_copies);
 else {assert(snapshot_copies==1);if(inherited_image)assert(transition_frames>=5);}
 if(quick_scenario){assert(stage_opens==1&&stage_closes==1&&quick_exit_sent&&restored_frame&&native_home_seen);assert(controls_closed_at-controls_opened_at>=1000);}
#ifdef PORTABLE_STAGE_LOGS
 assert(stage_submits==presents&&stage_completes==presents);
 if(presents)assert(stage_statements);
#endif
 assert(!fclose(stage_capture)&&!fclose(frame_records));
 if(quick_scenario)printf("Home controls: opened_ms=%u closed_ms=%u restored_frame=%u frames=%u logs=%u exact_native_Home=1 writes=0\n",controls_opened_at,controls_closed_at,restored_frame,transition_frames,stage_statements);
 printf("Home snapshot: copies=%u frames=%u prior=%u absent=%u\n",snapshot_copies,transition_frames,inherited_image,without_snapshot);
 return 0;
}
