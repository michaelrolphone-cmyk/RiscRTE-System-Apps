#define main historical_fixture_main
#include HOST_FIXTURE
#undef main
#include "RiscSceneCheckpointV1.h"
extern const risc_driver_v2*paper_profile_get(uint32_t);
int main(int argc,char**argv){
 assert(argc==2);const char*mode=argv[1];w=800;h=480;format=1;
 const risc_driver_v2*profile_driver=paper_profile_get(2);assert(profile_driver&&profile_driver->start(NULL,0));prof=*(const risc_scene_profile_v1*)profile_driver->capability;
 assert(prof.display_rotation==270&&prof.touch_rotation==0&&prof.preferred_format==1);
 driver=t5_driver_get(2);assert(driver);api=driver->capability;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 if(!strncmp(mode,"missing-",8)){
  char*end=NULL;unsigned long omit=strtoul(mode+8,&end,10);assert(* (mode+8)&&end&&!*end&&omit<4);
  for(unsigned i=(unsigned)omit;i<3;i++)deps[i]=deps[i+1];
  unsigned before=calls;
  assert(!driver->start(deps,3)&&calls==before);printf("Actual paper required dependency%lu absent: clean refusal\n",omit);return 0;
 }
 assert(driver->start(deps,4));make_document();assert(!api->open(NULL,&doc,NULL,&session));
 const risc_scene_checkpoint_api_v1*checkpoint=risc_scene_checkpoint_get_v1(api);assert(checkpoint);const risc_scene_page_refresh_api_v1*refresh=&checkpoint->presentation;const risc_scene_page_api_v1*page=&refresh->page;
 risc_scene_page_geometry_v1 geometry={.struct_size=sizeof(geometry)};assert(!page->geometry(NULL,&geometry));assert(geometry.width>0&&geometry.width<=480&&geometry.height>0&&geometry.height<=800);assert(geometry.stride==(geometry.width+7)/8&&geometry.stride<=60);assert((size_t)geometry.stride*geometry.height<=48000);
 if(!strcmp(mode,"touch")){
  settle();unsigned top=prof.font_scale*7+prof.padding*2+8;touch_tap(240,top+prof.row_height+10);risc_scene_event_v1 event;assert(tick(&event)==RISC_SCENE_IDLE);settle();
  risc_scene_navigation_v1 state={.struct_size=sizeof(state)};uint32_t flags;assert(!api->snapshot(NULL,session,&state,&flags)&&state.depth==2);finish();puts("Actual270 paper profile touch navigation without navigation provider passed");return 0;
 }
 size_t bytes=(size_t)geometry.stride*geometry.height;uint8_t*bitmap=malloc(bytes);assert(bitmap);memset(bitmap,255,bytes);memset(bitmap+bytes/2,0,bytes/8);
 risc_scene_page_document_v1 document={.struct_size=sizeof(document),.revision=2,.previous_action=71,.menu_action=72,.next_action=73,.back_action=72};strcpy(document.title,"PAPER CONTRACT");
 expected_intent=RISC_DISPLAY_PRESENT_CLEAN;assert(!refresh->present_page_with_refresh(NULL,session,&document,bitmap,bytes,RISC_SCENE_PAGE_REFRESH_CLEAN));settle();
 unsigned initial=submits;size_t frame_bytes=(size_t)w/8*h;uint8_t*completed=malloc(frame_bytes);assert(completed);memcpy(completed,pixels+16,frame_bytes);risc_scene_event_v1 event;
 if(!strcmp(mode,"static")){
  for(unsigned cycle=0;cycle<3;cycle++){assert(!checkpoint->pause(NULL,session)&&!subscribed&&!held&&!presenting);assert(!checkpoint->resume(NULL,session,0));for(unsigned n=0;n<1000;n++)assert(tick(&event)==RISC_SCENE_IDLE);assert(submits==initial&&!memcmp(completed,pixels+16,frame_bytes));}
 }else if(!strcmp(mode,"overlay")){
  assert(!checkpoint->pause(NULL,session));memset(pixels+16,0x55,frame_bytes);assert(!checkpoint->resume(NULL,session,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_CLEAN));settle();assert(submits==initial+1&&!memcmp(completed,pixels+16,frame_bytes));
 }else if(!strcmp(mode,"pending")){
  document.revision++;assert(!refresh->present_page_with_refresh(NULL,session,&document,bitmap,bytes,RISC_SCENE_PAGE_REFRESH_CLEAN));allow_complete=false;begin_frame();assert(checkpoint->pause(NULL,session)==RISC_SCENE_AGAIN&&!driver->quiesce());assert(checkpoint->resume(NULL,session,0)==RISC_SCENE_BUSY);allow_complete=true;assert(!checkpoint->pause(NULL,session));assert(!checkpoint->resume(NULL,session,0));settle();assert(submits==initial+1);
 }else assert(0);
 finish();free(bitmap);free(completed);assert(!held&&!presenting&&!subscribed);printf("Actual270 paper %s without optional navigation passed\n",mode);
}
