/* Actual .57 selected sparse Home, including its complete production flags. */
static unsigned expected_launches;
#define SPARSE_EXPECT_LAUNCHES expected_launches
#define TEST_SHARED_QUICK_REFERENCE
#define app_main gt_sparse_app_entry
#define SPARSE_FIXTURE_MAIN gt_sparse_setup_main
#include "sparse_clock_startup_test.c"
#undef app_main
void app_main(void);
static unsigned retry_at;
static bool clock_gt_refusal_script(risc_touch_snapshot_v1 *out){
 *out=(risc_touch_snapshot_v1){.width=480,.height=800};
 unsigned at=retry_at&&script_ms>=retry_at?script_ms-retry_at+50:script_ms;
 if((!launches||(retry_at&&script_ms>=retry_at))&&at>=50&&at<125){
  out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=at>=75?285:240,.y=360};
 }
 return true;
}
#define CLOCK_GT_REFUSAL_RETRY
#define CLOCK_GT_INCLUDE_FIXTURE
#define CLOCK_GT_EXTERNAL_RUNTIME
#include "paper_clock_gt911_report_test.c"
#include "RiscResidentShellV1.h"
void clock_gt_finish(void);
bool clock_gt_frame_pending(void);
static unsigned selected_case;
static unsigned retained_at;
static uint8_t submitted_pixels[sizeof(pixels)];
static bool selected_retain(void){retained_at=ms;printf("retain ms=%u\n",ms);return retain_invocation();}
static bool reference_sparse_ready(void){return true;}
static bool reference_sparse_touch(void*c,risc_touch_snapshot_v1*out){(void)c;return gt_script(out);}
static bool empty_navigation(void*c,risc_input_navigation_frame_v1*out){(void)c;*out=(risc_input_navigation_frame_v1){0};return true;}
static bool selected_health(risc_runtime_health_v1*out){
 out->uptime_ms=ms;
 if(!display_busy&&script_ms>=1000&&!clock_gt_frame_pending())clock_gt_finish();
 assert(script_ms<15000);return true;
}
static void selected_yield(uint32_t count){assert(!terminal);ms+=count;script_ms+=count;}
static bool selected_diagnostic(const char *message){printf("diagnostic ms=%u %s\n",ms,message);return diagnostic(message);}
static bool selected_info(void*c,risc_display_info_v1*out){assert(raw_display_info(c,out));out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return true;}
static bool selected_submit(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1 *rect,size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 assert(!display_pending);bool ok=frame_submit(c,frame,rect,count,options,token);
 if(ok){memcpy(submitted_pixels,pixels,sizeof(pixels));display_submitted_at=ms;display_pending=true;printf("display_submit ms=%u token=%llu\n",ms,(unsigned long long)*token);}return ok;
}
#include "RiscAppDataV1.h"
static int32_t empty_stat(void*c,const char*n,uint32_t*size,uint64_t*revision){(void)c;(void)n;safe();*size=0;*revision=0;return RISC_APP_DATA_NOT_FOUND;}
static int32_t empty_read(void*c,const char*n,uint64_t expected,void*out,uint32_t cap,uint32_t*size,uint64_t*revision){(void)expected;(void)out;(void)cap;return empty_stat(c,n,size,revision);}
static int32_t empty_replace(void*c,const char*n,uint64_t expected,const void*out,uint32_t size){(void)c;(void)n;(void)expected;(void)out;(void)size;assert(!"No crash report write expected");return RISC_APP_DATA_INVALID;}
static const risc_app_data_v1 empty_data={1,sizeof(empty_data),NULL,empty_stat,empty_read,empty_replace};
static bool selected_status(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){
 if(display_pending)assert(!memcmp(submitted_pixels,pixels,sizeof(pixels)));
 return gt_present_status(c,token,out);
}
static bool selected_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1*out){
 if(!strcmp(name,"contexts.service"))return false; /* Explicit absent optional service. */
 if(!strcmp(name,"storage.app-data")&&id==62){safe();assert(version==1);out->api=&empty_data;out->slot=++grants;out->generation=1;return true;}
 bool result=obtain(name,version,id,out);
 if(result&&!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 nav;nav=navigation;nav.poll=empty_navigation;out->api=&nav;}
 if(result&&!strcmp(name,"input.touch.raw")){touch_power.base=gt_touch;touch_power.base.struct_size=sizeof(touch_power);out->api=&touch_power;}
 return result;
}
static int32_t selected_register(uint64_t id,const risc_resident_callbacks_v1 *callbacks){assert(id==1&&callbacks&&callbacks->dispatch);return RISC_RESIDENT_OK;}
static int32_t selected_run(uint64_t id,const char *path,risc_resident_result_v1 *result){
 assert(id==1&&result);if(!path)return RISC_RESIDENT_NO_PENDING;
 assert(!display_pending&&!frames);assert(ms-display_submitted_at>=display_delay);
 printf("clock_launch ms=%u target=%s\n",ms,path);
 bool accepted=launch_app(path);if(!accepted)retry_at=script_ms+150;
 return accepted?RISC_RESIDENT_HANDOFF:RISC_RESIDENT_DENIED;
}
static int32_t selected_exit(uint64_t id){assert(id==1);return RISC_RESIDENT_OK;}
static bool selected_resident(risc_resident_client_v1 *out){*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=1,.role=RISC_RESIDENT_ROLE_HOST,.register_shell=selected_register,.run_foreground=selected_run,.request_foreground_exit=selected_exit};return true;}
void gt_sparse_app_entry(void){
 scenario=selected_case;script_ms=0;
 panel.history.base.get_info=selected_info;panel.history.base.submit=selected_submit;panel.history.base.present_status=selected_status;
 runtime.health=selected_health;runtime.yield_ms=selected_yield;runtime.diagnostic=selected_diagnostic;
 runtime.acquire=selected_acquire;runtime.resident_shell=selected_resident;runtime.retain_invocation=selected_retain;
 app_main();
 if(display_busy){
  assert(!launches&&barriers==1&&terminal&&grants);assert(retained_at-display_submitted_at>=10000);
  assert(gt_first_dispatch_ms&&gt_first_dispatch_ms<retained_at);
  if(selected_case==15){
   assert(clock_input_delivered[RISC_TOUCH_EVENT_DOWN]==2&&clock_input_delivered[RISC_TOUCH_EVENT_UP]==2);
   assert(gt_last_dispatch_ms<display_submitted_at+200);
  }
 }
 else {assert(launches==expected_launches);if(launches)assert(!strcmp(launched,selected_case==16?"points_in_time.elf":"springboard.elf"));}
 if(selected_case==16)assert(clock_input_delivered[RISC_TOUCH_EVENT_DOWN]==1&&clock_input_delivered[RISC_TOUCH_EVENT_UP]==1);
 if(selected_case==18&&(display_delay||display_busy))assert(injected_multi);
}
int main(int argc,char **argv){
 assert(argc==4||argc==5);setbuf(stdout,NULL);selected_case=(unsigned)atoi(argv[1]);repeated_report=argc==5;
 if(!strcmp(argv[2],"slow"))display_delay=200;else if(!strcmp(argv[2],"busy"))display_busy=true;else assert(!strcmp(argv[2],"fast"));
 expected_launches=display_busy?0u:selected_case==9?2u:selected_case==14||selected_case==16||selected_case==18?1u:0u;
 gt911=clock_gt911_start();ms=1000;runtime.resident_shell=selected_resident;runtime.retain_invocation=selected_retain;
 char *base_argv[]={argv[0],"foreground",argv[3],NULL};int result=gt_sparse_setup_main(3,base_argv);
 if(!display_busy)clock_gt911_stop();
 printf("Selected Home actual GT911 scenario=%u display=%s launches=%u retained=%u PASS\n",selected_case,argv[2],launches,barriers);return result;
}
