#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneCheckpointV1.h"
int main(int argc,char **argv){
 assert(argc==2);const char *mode=argv[1];
 w=480;h=800;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,0,0,0};
 startup();const risc_scene_checkpoint_api_v1 *checkpoint=risc_scene_checkpoint_get_v1(api);assert(checkpoint);
 const risc_scene_page_refresh_api_v1 *refresh=&checkpoint->presentation;const risc_scene_page_api_v1 *page=&refresh->page;
 risc_scene_page_refresh_api_v1 old=*refresh;old.page.components.lifecycle.base.struct_size=sizeof(old);
 assert(!risc_scene_checkpoint_get_v1(&old.page.components.lifecycle.base));
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};assert(!page->geometry(NULL,&g));
 size_t bytes=(size_t)g.stride*g.height;uint8_t *bitmap=malloc(bytes);memset(bitmap,0xff,bytes);memset(bitmap+bytes/2,0,bytes/8);
 risc_scene_page_document_v1 p={.struct_size=sizeof(p),.revision=2,.previous_action=71,.menu_action=72,.next_action=73,.back_action=72};strcpy(p.title,"STATIC BOOK");
 expected_intent=RISC_DISPLAY_PRESENT_CLEAN;assert(!refresh->present_page_with_refresh(NULL,session,&p,bitmap,bytes,RISC_SCENE_PAGE_REFRESH_CLEAN));settle();
 const unsigned initial=submits;size_t frame_bytes=w/8*h;uint8_t *completed=malloc(frame_bytes);memcpy(completed,pixels+16,frame_bytes);
 risc_scene_event_v1 e;
 if(!strcmp(mode,"static")){
  for(unsigned cycle=0;cycle<4;cycle++){
   assert(!checkpoint->pause(NULL,session)&&!checkpoint->pause(NULL,session));assert(!held&&!presenting&&!subscribed);
   unsigned before=calls;assert(tick(&e)==RISC_SCENE_BUSY);p.revision++;
   assert(page->present_page(NULL,session,&p,bitmap,bytes)==RISC_SCENE_BUSY&&calls==before);
   assert(checkpoint->resume(NULL,session,RISC_SCENE_RESUME_CLEAN)==RISC_SCENE_INVALID&&calls==before);
   assert(!checkpoint->resume(NULL,session,0));
   for(unsigned n=0;n<1000;n++)assert(tick(&e)==RISC_SCENE_IDLE);
   assert(submits==initial&&!memcmp(completed,pixels+16,frame_bytes));
  }
  nav_pressed=RISC_NAV_RIGHT;assert(tick(&e)==RISC_SCENE_OK&&e.action==73&&e.document_revision==2);
 }else if(!strcmp(mode,"external")){
  assert(!checkpoint->pause(NULL,session));memset(pixels+16,0x55,frame_bytes);
  assert(!checkpoint->resume(NULL,session,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_CLEAN));settle();
  for(unsigned n=0;n<1000;n++)assert(tick(&e)==RISC_SCENE_IDLE);
  assert(submits==initial+1&&!memcmp(completed,pixels+16,frame_bytes));
 }else if(!strcmp(mode,"raster")){
  p.revision++;assert(!refresh->present_page_with_refresh(NULL,session,&p,bitmap,bytes,RISC_SCENE_PAGE_REFRESH_CLEAN));
  assert(tick(&e)==RISC_SCENE_IDLE&&held);assert(!checkpoint->pause(NULL,session));assert(!held&&!subscribed&&releases==1);
  assert(!checkpoint->resume(NULL,session,0));settle();assert(submits==initial+1);
 }else if(!strcmp(mode,"pending")){
  p.revision++;assert(!refresh->present_page_with_refresh(NULL,session,&p,bitmap,bytes,RISC_SCENE_PAGE_REFRESH_CLEAN));allow_complete=false;begin_frame();
  assert(checkpoint->pause(NULL,session)==RISC_SCENE_AGAIN);assert(checkpoint->resume(NULL,session,0)==RISC_SCENE_BUSY);
  assert(tick(&e)==RISC_SCENE_BUSY);allow_complete=true;assert(!checkpoint->pause(NULL,session));assert(!presenting&&!subscribed);
  assert(!checkpoint->resume(NULL,session,0));settle();assert(submits==initial+1);
 }else if(!strcmp(mode,"paused-close")){
  assert(!checkpoint->pause(NULL,session));finish();free(bitmap);free(completed);puts("Paused scene close PASS");return 0;
 }else{
  bool pause_fault=!strcmp(mode,"unsubscribe-fault");
  if(pause_fault){bad_unsubscribe=true;assert(checkpoint->pause(NULL,session)==RISC_SCENE_RETAINED);}
  else{assert(!checkpoint->pause(NULL,session));loss_callback=mode;assert(checkpoint->resume(NULL,session,0)==RISC_SCENE_RETAINED);}
  unsigned before=calls;assert(checkpoint->pause(NULL,session)==RISC_SCENE_RETAINED);assert(checkpoint->resume(NULL,session,0)==RISC_SCENE_RETAINED);assert(api->close(NULL,session)==RISC_SCENE_RETAINED);assert(!driver->quiesce());driver->stop();assert(calls==before);
  free(bitmap);free(completed);printf("Scene checkpoint %s retention fence PASS\n",mode);return 0;
 }
 finish();free(bitmap);free(completed);printf("Scene checkpoint %s PASS\n",mode);return 0;
}
