/* Actual shared adapter; isolated provider media and exact pending frame identity. */
#define TEST_QUICK_ALLOC_REFUSAL
#define TEST_SCENE_QUICK
#define TEST_SCENE_ROUND
#define PORTABLE_QUICK_NATIVE_GEOMETRY
#define PORTABLE_QUICK_PENDING_FRAME
#define PORTABLE_QUICK_SLICED_NATIVE
#define PORTABLE_FRAME_RECEIPTS
#ifndef PORTABLE_RASTER_SNAPSHOT
#define PORTABLE_RASTER_SNAPSHOT
#endif
#define PORTABLE_RASTER_WORK_BUDGET_MS 8
#include "quick_sliced_media.h"
static unsigned mode,waits;
static uint16_t expected[TEST_SIZE*TEST_SIZE];
static bool mutated;
static void drive(uint32_t n){
 yield(n);++waits;
 if(quick_pending_receipt){
  if(mode==2)navigation_pending|=T5_APP_BUTTON_BACK;
  if(mode==3)down=true;
  if(mode==7)bad_poll=true;
  if(mode==9&&!mutated){quick.ui.brightness=17;quick.ui.error_flags|=PQA_ERROR_SAVE;mutated=true;}
  if((mode==4||mode==5)&&raster_sealed){raster_discard();if(mode==5){presentation_completed_id=quick_pending_receipt+1;display_settled=true;}}
 }
 if(waits>3)completed=submitted;
}
int main(int argc,char**argv){
 mode=argc>1?(unsigned)atoi(argv[1]):0;assert(mode<13);
 assert(app_module_init()==0);auto_complete=true;
 assert(portable_paper_frame_ready());clear();present(false);assert(portable_paper_frame_drain());
 assert(frame_requested_id&&frame_completed_id==frame_requested_id);
 uint64_t app_id=frame_completed_id;
 if(mode==11||mode==12){
  quick.ui.position_q8=quick.ui.target_q8=pqa_open_q8(&quick.ui);quick.ui.gesture=PQA_IDLE;quick.ui.neutral_gate=false;
  quick_drive_close=true;quick_wifi_route=mode==12;bool consumed=false;
  assert(quick_foreground(&consumed));assert(consumed&&!failed&&!retained&&!quick_modal);
  assert(frame_requested_id==app_id&&frame_completed_id==app_id);
  assert(portable_paper_frame_drain());assert(frame_completed_id==app_id);
  if(mode==12)assert(quick_wifi_phase);else assert(!memcmp(pixels,alarm_pixels,sizeof(pixels)));
  app_module_fini();assert(!retained&&!frames&&!grants);puts("PASS full modal close/setup with preserved app map");return 0;
 }
 if(mode==10){
  quick.ui.position_q8=quick.ui.target_q8=pqa_open_q8(&quick.ui);quick.ui.gesture=PQA_IDLE;quick.ui.neutral_gate=false;
  refuse_underlay=true;bool consumed=false;assert(quick_foreground(&consumed));refuse_underlay=false;
  assert(!quick_modal&&!quick_background&&!retained&&!failed);
  assert(frame_requested_id==app_id&&frame_completed_id==app_id);assert(!pqa_visible(&quick.ui));
  assert(portable_paper_frame_ready());clear();present(false);assert(portable_paper_frame_drain());
  app_module_fini();assert(!retained&&!frames&&!grants);puts("PASS underlay allocation refusal preserves usable foreground");return 0;
 }
 quick_background=malloc(quick_frame_bytes());assert(quick_background);memcpy(quick_background,alarm_pixels,quick_frame_bytes());
 quick_modal=true;quick.ui.position_q8=quick.ui.target_q8=pqa_open_q8(&quick.ui);quick.ui.gesture=PQA_IDLE;quick.ui.neutral_gate=false;
 quick_atomic_running=true;auto_complete=mode==0;quick_fail_submit=mode==6;
 if(mode==8)presentation_requested_id=UINT64_MAX;
 memcpy(expected,quick_background,sizeof(expected));
 pqa_state preview=quick.ui;preview.save_pending=true;preview.save_cancelled=false;
 risc_display_surface_v1 expected_surface={.pixels=expected,.width=TEST_SIZE,.height=TEST_SIZE,.stride_bytes=TEST_SIZE*2,.size_bytes=sizeof(expected),.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 assert(pqa_render(&expected_surface,&preview,"--:--",false,0));
 risc_runtime_api_v1 wrapper=api;wrapper.yield_ms=drive;rt=&wrapper;
 int result=quick_pending_frame();
 if(mode==4||mode==6||mode==8){assert(result==PENDING_FAULT);if(!retained)portable_adapter_retain_silent();unsigned before=calls;assert(quick_pending_frame()==PENDING_FAULT);app_module_fini();assert(calls==before);puts("PASS sliced pending terminal fence");return 0;}
 fprintf(stderr,"case%u result%d retained%d failed%d app%llu req%llu complete%llu pres%llu completed%llu expected%llu waits%u\n",mode,result,retained,failed,(unsigned long long)app_id,(unsigned long long)frame_requested_id,(unsigned long long)frame_completed_id,(unsigned long long)presentation_requested_id,(unsigned long long)presentation_completed_id,(unsigned long long)quick_pending_receipt,waits);assert(!retained&&frame_requested_id==app_id&&frame_completed_id==app_id);
 if(mode==2||mode==3||mode==4||mode==5||mode==7)assert(result==PENDING_CANCELLED);
 else {assert(result==PENDING_READY&&quick_pending_receipt&&presentation_completed_id==quick_pending_receipt);assert(!paper_token&&!raster_sealed);assert(!memcmp(pixels,expected,sizeof(expected)));if(mode==9)assert(mutated);}
 bad_poll=false;down=false;completed=submitted;auto_complete=true;assert(portable_paper_frame_drain());
 quick_pending_done();quick_atomic_running=false;quick_modal=false;app_module_fini();assert(!retained&&!frames&&!grants);
 printf("PASS native466 sliced pending case%u result%d app-receipt-unchanged\n",mode,result);
}
