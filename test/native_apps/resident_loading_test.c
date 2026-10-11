/* Reuse deterministic peripheral definitions only. This new fixture exercises
 * the real host controller/adapter. Runtime loading itself is modeled here;
 * the separate Runtime callback suite covers real chain/admission ownership. */
#define main base_fixture_main
#define risc_runtime_get_api base_fixture_runtime
#include "resident_shell_test.c"
#undef risc_runtime_get_api
#undef main
#include "PortableResidentShell.h"
static const char *loading_case;
static bool in_loading,child_pending;
static unsigned loading_frames,child_loads,loading_completions,pending,submitted_at,normal_count;
static unsigned callback_calls,input_during_loading,returned,failed_frames,loading_nav;
static int result_seen;
static uint8_t loading_image[48000];
bool loading_mode(const char *name){return !strcmp(loading_case,name);}
void loading_result(int result){result_seen=result;++returned;}
static bool loading_info(void *c,risc_display_info_v1 *out){
 bool result=display_info(c,out);
 if(!loading_mode("sync"))out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;
 return result;
}
static bool loading_acquire(void *c,uint32_t format,risc_display_surface_v1 *out){
 assert(!pending);
 if(in_loading && loading_mode("acquire")){live();return false;}
 return acquire_frame(c,format,out);
}
static bool loading_submit(void *c,risc_display_frame_v1 value,const risc_display_rect_v1 *rect,size_t count,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *out){
 live();(void)c;(void)rect;assert(value==1&&frame==role&&!pending);
 if(in_loading) {
  assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY && count==0);
  ++loading_frames;
  if(loading_mode("submit"))return false;
 }else if(role==1){
  if(options->intent==RISC_DISPLAY_PRESENT_CLEAN)++failed_frames;
  else ++normal_count;
 }
 frame=0;*out=++token;pending=token;submitted_at=ms;
 return true;
}
static bool loading_status(void *c,risc_display_present_token_v1 value,risc_display_present_status_v1 *out){
 live();(void)c;assert(value==pending&&value==token);
 if(in_loading&&loading_mode("status"))return false;
 if((in_loading&&loading_mode("timeout"))||ms-submitted_at<37){
  if(child_pending)assert(!memcmp(completed,loading_image,sizeof(completed)));
  out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;
 }
 memcpy(completed,pixels,sizeof(completed));pending=0;out->state=RISC_DISPLAY_PRESENT_COMPLETE;
 if(in_loading){
  ++loading_completions;memcpy(loading_image,completed,sizeof(completed));
  char path[1024];snprintf(path,sizeof(path),"%s/loading-%s-%u.pbm",fixture_dir,loading_case,loading_completions);
  FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P4\n480 800\n");
  assert(fwrite(completed,1,sizeof(completed),file)==sizeof(completed));assert(!fclose(file));
  /* The old Home rectangle must not leak through a still-running crossfade. */
  for(unsigned y=0;y<250;++y)for(unsigned x=0;x<60;++x)assert(!completed[y*60+x]);
  for(unsigned y=600;y<800;++y)for(unsigned x=0;x<60;++x)assert(!completed[y*60+x]);
  /* Icon, catalog title, loading text and indeterminate dots all contain ink. */
  const unsigned bounds[][2]={{270,366},{394,454},{474,504},{508,517}};
  for(unsigned region=0;region<4;++region){unsigned ink=0;
   for(unsigned y=bounds[region][0];y<bounds[region][1];++y)
    for(unsigned x=3;x<57;++x)ink+=completed[y*60+x]!=0;
   assert(ink>0);
  }
 }
 return true;
}
static bool loading_navigation(void *c,risc_input_navigation_frame_v1 *out){
 if(in_loading){++input_during_loading;assert(false);}
 bool result=nav_poll(c,out);
 if(failed_frames && !pending && ++loading_nav==2)out->pressed=RISC_NAV_BACK;
 return result;
}
static bool loading_snapshot(void *c,uint32_t format,void *out,size_t size,uint32_t stride){
 live();(void)c;assert(!frame&&!pending&&format==RISC_DISPLAY_FORMAT_MONO1&&size==sizeof(completed)&&stride==60);
 memcpy(out,completed,size);return true;
}
static bool loading_wait(void *c,risc_display_present_token_v1 value,uint32_t budget,risc_display_present_status_v1 *out){
 assert(budget>=37);wait_ms(37);return loading_status(c,value,out);
}
static risc_display_output_api_v1_snapshot loading_display;
static risc_input_navigation_api_v1 loading_navigation_api;
static bool loading_obtain(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!acquire(name,version,instance,out))return false;
 if(!strcmp(name,"display.output"))out->api=&loading_display;
 if(!strcmp(name,"input.navigation"))out->api=&loading_navigation_api;
 return true;
}
static int32_t loading_run(uint64_t invocation,const char *path,risc_resident_result_v1 *out){
 live();assert(invocation==1&&host_registered&&!focus&&!subs&&!frame&&!pending);
 if(!path){if(loading_mode("no-pending"))return out->status=RISC_RESIDENT_NO_PENDING;path="client.elf";}
 if(loading_mode("refused"))return out->status=RISC_RESIDENT_DENIED;
 if(loading_mode("busy"))return out->status=RISC_RESIDENT_BUSY;
 if(loading_mode("fenced")){terminal=true;return out->status=RISC_RESIDENT_RETAINED;}
 unsigned runs=loading_mode("chain")?2:1;
 for(unsigned i=0;i<runs;++i){
  in_loading=true;++callback_calls;
  assert(callbacks.loading);
  int status=callbacks.loading(callbacks.context,i?"second.elf":path);
  in_loading=false;
  if(status!=RISC_RESIDENT_OK)return out->status=status;
  assert(!frame&&!pending&&loading_completions==callback_calls);
  if(loading_mode("legacy"))return out->status=RISC_RESIDENT_HANDOFF;
  ++child_loads;
  /* Synthetic slow loading/init leaves the completed host image untouched. */
  for(unsigned j=0;j<20;++j){wait_ms(50);assert(!memcmp(completed,loading_image,sizeof(completed)));}
  if(loading_mode("retained")){terminal=true;return out->status=RISC_RESIDENT_RETAINED;}
  if(loading_mode("load-failed")){
   out->invocation=42;out->failure=(risc_resident_failure_v1){.struct_size=sizeof(out->failure),
    .kind=RISC_RESIDENT_FAILURE_LOAD,.status=RISC_RESIDENT_FAILED,.invocation=42};
   strcpy(out->failure.application,path);strcpy(out->failure.detail,"Synthetic load refusal");
   return out->status=RISC_RESIDENT_FAILED;
  }
  role=2;risc_display_surface_v1 child={0};assert(loading_acquire(NULL,RISC_DISPLAY_FORMAT_MONO1,&child));
  memset(pixels,0x55,sizeof(pixels));risc_display_present_token_v1 token_value;
  const risc_display_present_options_v1 options={RISC_DISPLAY_PRESENT_LOW_LATENCY,RISC_DISPLAY_QUEUE_FIFO,0};
  child_pending=true;assert(loading_submit(NULL,1,NULL,0,&options,&token_value));
  assert(!memcmp(completed,loading_image,sizeof(completed)));
  risc_display_present_status_v1 status_value={0};
  do{assert(loading_status(NULL,token_value,&status_value));if(pending)wait_ms(1);}while(pending);
  child_pending=false;assert(memcmp(completed,loading_image,sizeof(completed))!=0);role=1;
 }
 return out->status=RISC_RESIDENT_OK;
}
static bool loading_resident(risc_resident_client_v1 *out){
 if(!resident_get(out))return false;
 out->run_foreground=loading_run;return true;
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){
 static risc_runtime_api_v1 api;api=runtime;api.acquire=loading_obtain;api.resident_shell=loading_resident;
 return version==1?&api:NULL;
}
int main(int argc,char **argv){
 assert(argc==3);fixture_dir=argv[1];loading_case=argv[2];mode=100;
 loading_display=display;loading_display.copy_completed=loading_snapshot;
 risc_display_output_api_v1 *selected=&loading_display.metrics.power.history.base;
 selected->get_info=loading_info;selected->acquire=loading_acquire;
 selected->submit=loading_submit;selected->present_status=loading_status;selected->wait_present=loading_wait;
 loading_navigation_api=nav;loading_navigation_api.poll=loading_navigation;
 load_app("host.elf",1);
 bool unsafe=loading_mode("fenced")||loading_mode("retained")||loading_mode("acquire")||loading_mode("submit")||loading_mode("status")||loading_mode("timeout");
 assert(returned&&terminal==unsafe&&!input_during_loading&&!after_terminal);
 if(unsafe)assert(result_seen==-1&&!finis);
 else {
  assert(!frame&&!pending&&!grants&&!subs&&!focus&&finis==1);
  if(loading_mode("refused")||loading_mode("busy")||loading_mode("writable-refused")||loading_mode("load-failed"))assert(result_seen==0&&normal_count==2);
  else if(loading_mode("legacy"))assert(result_seen==PORTABLE_RESIDENT_HANDOFF&&normal_count==1&&!child_loads);
  else if(loading_mode("no-pending"))assert(result_seen==PORTABLE_RESIDENT_NO_PENDING&&!loading_frames&&!child_loads);
  else assert(result_seen==1&&child_loads==(loading_mode("chain")?2u:loading_mode("repeat")?3u:1u));
  assert(loading_frames==loading_completions);
  if(loading_mode("load-failed"))assert(failed_frames==1);
 }
 printf("Host loading %s: callbacks=%u settled=%u loads=%u retained=%u PASS\n",loading_case,callback_calls,loading_completions,child_loads,terminal);
 return 0;
}
