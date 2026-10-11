/* Real Clock + renderer + input loop against asynchronous display custody. */
#define SPARSE_FIXTURE_MAIN sparse_fixture_main
#include "sparse_clock_startup_test.c"
#include "RiscDisplayOutputSnapshotV1.h"
static risc_display_output_api_v1_snapshot boot_panel;
static uint8_t submitted[48000];
static unsigned animation_starts,animation_ends,animation_frames,animation_began,animation_ended;
static unsigned touch_samples,last_touch,max_touch_gap,provider_samples,boot_frames;
static bool animation_live,skip_sent,exit_sent,skip_input;
static const char *directory;
static bool boot_health(risc_runtime_health_v1 *out){io();assert(ms<15000);out->uptime_ms=ms;return true;}
static bool boot_log(const char *line){
 if(strstr(line,"stage=boot-animation-start")){animation_starts++;animation_live=true;animation_began=ms;}
 if(strstr(line,"stage=boot-animation-end")){animation_ends++;animation_live=false;animation_ended=ms;}
 return report(line);
}
static bool boot_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;safe();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(touch_samples&&ms-last_touch>max_touch_gap)max_touch_gap=ms-last_touch;
 last_touch=ms;touch_samples++;
 if(skip_input&&animation_live&&ms-animation_began>500&&ms-animation_began<650){
  out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=400};skip_sent=true;
 }
 return true;
}
static uint64_t boot_subscribe(void *c){
 (void)c;subs++;raw_head=raw_count=0;assert(boot_touch(NULL,&raw_state));return 1;
}
static bool boot_poll_touch(void *c,size_t n){
 (void)c;assert(n==1);polls++;risc_touch_snapshot_v1 next;assert(boot_touch(NULL,&next));
 if(raw_state.contact_count&&!next.contact_count)raw_emit(RISC_TOUCH_EVENT_UP,raw_state.contacts[0]);
 if(!raw_state.contact_count&&next.contact_count)raw_emit(RISC_TOUCH_EVENT_DOWN,next.contacts[0]);
 next.sequence=raw_state.sequence;next.timestamp_ms=ms;raw_state=next;return true;
}
static bool boot_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;safe();*out=(risc_input_navigation_frame_v1){0};
 if(!animation_live&&ms>=4000&&!raw_present_pending&&!exit_sent){out->pressed=out->released=RISC_NAV_CONFIRM;exit_sent=true;}
 return true;
}
static bool boot_info(void *c,risc_display_info_v1 *out){raw_display_info(c,out);out->flags&=~RISC_DISPLAY_INFO_CLEAN_PRESENT;return true;}
static bool boot_copy(void*c,uint32_t format,void*out,size_t size,uint32_t stride){
 (void)c;safe();assert(!frames&&!raw_present_pending&&format==RISC_DISPLAY_FORMAT_MONO1&&stride==100&&size==48000);
 if(!presents)return false;
 memcpy(out,physical,48000);return true;
}
static bool boot_acquire(void*c,uint32_t format,risc_display_surface_v1*out){assert(!raw_present_pending);return frame_acquire(c,format,out);}
static bool boot_submit(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1*damage,size_t count,const risc_display_present_options_v1*options,risc_display_present_token_v1*token){
 assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
 boot_frames++;if(animation_live)animation_frames++;
 char path[768];snprintf(path,sizeof(path),"%s/frame-%02u.bin",directory,boot_frames);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(pixels,1,48000,f)==48000);assert(!fclose(f));
 memcpy(submitted,pixels,sizeof(submitted));return frame_submit(c,frame,damage,count,options,token);
}
static bool boot_status(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){
 provider_samples++;assert(!frames&&raw_present_pending&&!memcmp(pixels,submitted,sizeof(pixels)));
 return raw_present_status(c,token,out);
}
static bool boot_obtain(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 if(!obtain(name,version,instance,grant))return false;
 if(!strcmp(name,"display.output")){
  boot_panel.metrics.power=panel;
  risc_display_output_api_v1 *base=&boot_panel.metrics.power.history.base;
  base->get_info=boot_info;base->acquire=boot_acquire;base->submit=boot_submit;base->present_status=boot_status;base->struct_size=sizeof(boot_panel);
  boot_panel.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;boot_panel.snapshot_version=1;boot_panel.copy_completed=boot_copy;
  grant->api=&boot_panel;
 }
 if(!strcmp(name,"input.touch.raw")){static risc_touch_power_api_v1 input;input=touch_power;input.base.subscribe=boot_subscribe;input.base.poll=boot_poll_touch;input.base.snapshot=snapshot;grant->api=&input;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 input;input=navigation;input.poll=boot_nav;grant->api=&input;}
 return true;
}
int main(int argc,char **argv){
 assert(argc==5);directory=argv[3];skip_input=atoi(argv[4])!=0;
 runtime.acquire=boot_obtain;runtime.health=boot_health;runtime.diagnostic=boot_log;
 char*args[]={argv[0],argv[1],argv[2]};int rc=sparse_fixture_main(3,args);assert(!rc);
 bool cold=!strcmp(argv[1],"cold")||!strcmp(argv[1],"reset");
 assert(animation_starts==(unsigned)cold&&animation_ends==(unsigned)cold);
 if(cold){assert(animation_frames>=3&&animation_ended>animation_began);
  if(skip_input)assert(skip_sent&&animation_ended-animation_began<1000);
  else assert(animation_ended-animation_began>=1800&&animation_ended-animation_began<2600);
 }
 assert(touch_samples>10&&provider_samples>10&&max_touch_gap<=24&&exit_sent);
 char path[768];snprintf(path,sizeof(path),"%s/home.bin",directory);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(physical,1,48000,f)==48000);assert(!fclose(f));
 printf("Boot animation cold=%u skip=%u frames=%u duration=%u touch_samples=%u max_touch_gap=%u provider_samples=%u PASS\n",cold,skip_input,animation_frames,animation_ended-animation_began,touch_samples,max_touch_gap,provider_samples);
 return 0;
}
