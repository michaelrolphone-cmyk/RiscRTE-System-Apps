/* Actual native Home/Points controller and sparse adapter. The old Springboard
 * image is an explicitly completed provider fixture, never acquired contents. */
#define SPARSE_FIXTURE_MAIN sparse_fixture_main
#include "sparse_clock_startup_test.c"
#include "RiscDisplayOutputSnapshotV1.h"
static risc_display_output_api_v1_snapshot transition_panel;
static unsigned snapshot_copies,transition_frames;
static bool inherited_image,without_snapshot;
static const char *transition_directory;
static bool transition_metrics(void *c,risc_display_present_metrics_v1 *out){(void)c;(void)out;return false;}
static bool transition_copy(void *c,uint32_t format,void *out,size_t size,uint32_t stride){
 (void)c;safe();snapshot_copies++;assert(!frames&&!raw_present_pending&&format==RISC_DISPLAY_FORMAT_MONO1&&stride==100&&size==48000);
 if(!inherited_image&&!presents)return false;
 memcpy(out,physical,48000);return true;
}
static bool transition_info(void *c,risc_display_info_v1 *out){raw_display_info(c,out);out->flags&=~RISC_DISPLAY_INFO_CLEAN_PRESENT;return true;}
static bool transition_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(options->intent==RISC_DISPLAY_PRESENT_QUALITY);
 char path[768];snprintf(path,sizeof(path),"%s/frame-%02u.pbm",transition_directory,++transition_frames);FILE *file=fopen(path,"wb");assert(file);
 fprintf(file,"P4\n800 480\n");assert(fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));assert(!fclose(file));
 return frame_submit(c,frame,damage,count,options,token);
}
static bool transition_obtain(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 if(!obtain(name,version,instance,grant))return false;
 if(!strcmp(name,"display.output")){
  transition_panel.metrics.power=panel;
  transition_panel.metrics.power.history.base.get_info=transition_info;
  transition_panel.metrics.power.history.base.submit=transition_submit;
  transition_panel.metrics.power.history.base.struct_size=without_snapshot?sizeof(panel):sizeof(transition_panel);
  transition_panel.metrics.metrics_tag=RISC_DISPLAY_METRICS_TAG;transition_panel.metrics.metrics_version=1;transition_panel.metrics.snapshot=transition_metrics;
  transition_panel.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;transition_panel.snapshot_version=1;transition_panel.copy_completed=transition_copy;
  grant->api=&transition_panel;
 }
 return true;
}
int main(int argc,char **argv){
 assert(argc==6);transition_directory=argv[5];inherited_image=!strcmp(argv[1],"home-ready");without_snapshot=!strcmp(argv[4],"absent");
 FILE *file=fopen(argv[3],"rb");assert(file);assert(fread(physical,1,sizeof(physical),file)==sizeof(physical));assert(!fclose(file));
 runtime.acquire=transition_obtain;
 char *args[]={argv[0],argv[1],argv[2]};int result=sparse_fixture_main(3,args);assert(!result);
 if(!strcmp(argv[1],"terminal"))assert(!snapshot_copies&&!promoted&&transition_frames<=1);
 else if(without_snapshot)assert(!snapshot_copies);
 else {assert(snapshot_copies==1);if(inherited_image)assert(transition_frames>=5);}
 printf("Home snapshot: copies=%u frames=%u prior=%u absent=%u\n",snapshot_copies,transition_frames,inherited_image,without_snapshot);
 return 0;
}
