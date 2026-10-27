/* Independent checkpoint contract tests over the separately compiled shipping
 * text host. Existing transport fixture is reused; checkpoint tests are new. */
#define main legacy_host_fixture_main
#include "host_test.c"
#undef main
#include "RiscSceneCheckpointV1.h"
static risc_scene_checkpoint_api_v1 review_scene;
static const risc_text_entry_api_v1_checkpoint *checkpoint;
static bool review_paused,review_activity,review_input_busy;
static unsigned review_configures,review_pauses,review_resumes,review_pause_wait;
static int review_configure_result,review_pause_result,review_resume_result;
static int32_t review_configure(void*c,uint64_t s,uint32_t f){(void)c;boundary("configure");++calls;++review_configures;assert(s==12&&scene_active&&!(f&~RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY));return review_configure_result;}
static int32_t review_scene_next(void*c,uint64_t s,risc_scene_event_v1*out){assert(!review_paused);int rc=scene_next(c,s,out);if(!rc)review_activity=true;return rc;}
static int32_t review_scene_snapshot(void*c,uint64_t s,risc_scene_navigation_v1*out,uint32_t*f){int rc=scene_snapshot(c,s,out,f);if(review_activity)*f|=RISC_SCENE_ACTIVITY;if(review_input_busy)*f|=RISC_SCENE_INPUT_BUSY;review_activity=false;return rc;}
static int32_t review_pause(void*c,uint64_t s){(void)c;boundary("pause");++calls;++review_pauses;assert(s==12&&scene_active);if(review_pause_result)return review_pause_result;if(review_pause_wait){--review_pause_wait;return RISC_SCENE_AGAIN;}review_paused=true;return 0;}
static int32_t review_resume(void*c,uint64_t s,uint32_t f){(void)c;boundary("resume");++calls;++review_resumes;assert(s==12&&scene_active&&review_paused&&!(f&~(RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_FOCUS_CHANGED)));if(review_resume_result)return review_resume_result;review_paused=false;return 0;}
static int32_t unused_component_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;(void)d;(void)n;(void)s;assert(0);return -1;}
static int32_t unused_component_update(void*c,uint64_t s,const risc_components_document_v1*d){(void)c;(void)s;(void)d;assert(0);return -1;}
static int32_t unused_geometry(void*c,risc_scene_page_geometry_v1*g){(void)c;(void)g;assert(0);return -1;}
static int32_t unused_page(void*c,uint64_t s,const risc_scene_page_document_v1*d,const uint8_t*p,size_t n){(void)c;(void)s;(void)d;(void)p;(void)n;assert(0);return -1;}
static int32_t unused_refresh(void*c,uint64_t s,const risc_scene_page_document_v1*d,const uint8_t*p,size_t n,uint32_t f){(void)c;(void)s;(void)d;(void)p;(void)n;(void)f;assert(0);return -1;}
static void review_start(bool usb,bool supported){
 review_scene.presentation.page.components.lifecycle.base=scenes;
 review_scene.presentation.page.components.lifecycle.base.struct_size=sizeof(review_scene);
 review_scene.presentation.page.components.lifecycle.base.next=review_scene_next;
 review_scene.presentation.page.components.lifecycle.base.snapshot=review_scene_snapshot;
 review_scene.presentation.page.components.lifecycle.configure=review_configure;
 review_scene.presentation.page.components.tag=RISC_COMPONENTS_TAG;review_scene.presentation.page.components.version=1;
 review_scene.presentation.page.components.open=unused_component_open;review_scene.presentation.page.components.update=unused_component_update;
 review_scene.presentation.page.page_tag=RISC_SCENE_PAGE_TAG;review_scene.presentation.page.page_version=1;review_scene.presentation.page.geometry=unused_geometry;review_scene.presentation.page.present_page=unused_page;
 review_scene.presentation.refresh_tag=RISC_SCENE_PAGE_REFRESH_TAG;review_scene.presentation.refresh_version=1;review_scene.presentation.present_page_with_refresh=unused_refresh;
 review_scene.checkpoint_tag=RISC_SCENE_CHECKPOINT_TAG;review_scene.checkpoint_version=1;review_scene.pause=review_pause;review_scene.resume=review_resume;
 driver=t5_driver_get(2);assert(driver);api=driver->capability;checkpoint=risc_text_entry_checkpoint(api);assert(checkpoint);
 risc_provider_dependency_v1 deps[]={{RISC_SCENE_CAPABILITY,1,supported?(const void*)&review_scene:&scenes},{"usb.hid.keyboard",1,&keyboard}};
 assert(driver->start(deps,usb?2:1));
 risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=25,.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON,.label="NAME",.text="draft"};
 assert(!api->open(NULL,&q,&session));memset(&q,0xa5,sizeof(q));
}
static uint32_t snapshot_flags(void){risc_text_entry_checkpoint_state_v1 s={.struct_size=sizeof(s)};assert(!checkpoint->checkpoint_snapshot(NULL,session,&s));assert(s.struct_size==sizeof(s));return s.flags;}
static void consume_activity(void){(void)tick();(void)snapshot_flags();flush();}
static void enable(void){assert(!checkpoint->checkpoint_enable(NULL,session));assert(review_configures==1);assert(!checkpoint->checkpoint_enable(NULL,session));assert(review_configures==1);}
static void finish_review(void){loss_site=NULL;pending=false;finish();assert(!scene_active&&!kb_active);}
static void retained_fence(void){unsigned before=calls;risc_text_entry_checkpoint_state_v1 s={.struct_size=sizeof(s)};risc_text_entry_state_v1 t={.struct_size=sizeof(t)};assert(checkpoint->checkpoint_enable(NULL,session)==RISC_TEXT_ENTRY_RETAINED);assert(checkpoint->checkpoint_snapshot(NULL,session,&s)==RISC_TEXT_ENTRY_RETAINED);assert(checkpoint->pause(NULL,session)==RISC_TEXT_ENTRY_RETAINED);assert(checkpoint->resume(NULL,session,0)==RISC_TEXT_ENTRY_RETAINED);assert(api->poll(NULL,session,&t)==RISC_TEXT_ENTRY_RETAINED);assert(api->close(NULL,session)==RISC_TEXT_ENTRY_RETAINED);assert(!driver->quiesce());driver->stop();assert(calls==before);}
int main(int argc,char**argv){
 assert(argc==2);const char *mode=argv[1];bool usb=strstr(mode,"usb")||strstr(mode,"caps")||strstr(mode,"subscription")||strstr(mode,"snapshot");attached=usb;review_start(usb,strcmp(mode,"unsupported")!=0);
 if(!strcmp(mode,"abi")){
  risc_text_entry_api_v1_checkpoint copy=*checkpoint;assert(risc_text_entry_home_reason(api)&&risc_text_entry_masked(api)&&risc_text_entry_extended(api)&&risc_text_entry_long(api));
  for(size_t n=sizeof(risc_text_entry_api_v1);n<sizeof(copy);n++){copy.base.base.base.base.base.struct_size=(uint32_t)n;assert(!risc_text_entry_checkpoint((const risc_text_entry_api_v1*)&copy));}
  copy=*checkpoint;copy.checkpoint_tag++;assert(!risc_text_entry_checkpoint((const risc_text_entry_api_v1*)&copy));copy=*checkpoint;copy.checkpoint_version++;assert(!risc_text_entry_checkpoint((const risc_text_entry_api_v1*)&copy));copy=*checkpoint;copy.pause=NULL;assert(!risc_text_entry_checkpoint((const risc_text_entry_api_v1*)&copy));finish_review();
 }else if(!strcmp(mode,"unsupported")){assert(checkpoint->checkpoint_enable(NULL,session)==RISC_TEXT_ENTRY_UNAVAILABLE);assert(!strcmp(tick().text,"draft"));finish_review();
 }else if(!strcmp(mode,"unsupported-continuity")){review_configure_result=RISC_SCENE_INVALID;assert(checkpoint->checkpoint_enable(NULL,session)==RISC_TEXT_ENTRY_UNAVAILABLE);assert(!strcmp(tick().text,"draft"));finish_review();
 }else if(!strcmp(mode,"opt-in")){risc_text_entry_checkpoint_state_v1 s={.struct_size=sizeof(s)};assert(checkpoint->checkpoint_snapshot(NULL,session,&s)==RISC_TEXT_ENTRY_UNAVAILABLE);assert(checkpoint->pause(NULL,session)==RISC_TEXT_ENTRY_UNAVAILABLE);assert(checkpoint->resume(NULL,session,0)==RISC_TEXT_ENTRY_UNAVAILABLE);assert(!review_configures&&!review_pauses&&!review_resumes);finish_review();
 }else if(!strncmp(mode,"loss-",5)||!strncmp(mode,"fault-",6)){
  bool loss=!strncmp(mode,"loss-",5);const char *site=mode+(loss?5:6);int rc;
  if(!strcmp(site,"enable-configure")){if(loss)loss_site="configure";else review_configure_result=RISC_SCENE_BUSY;rc=checkpoint->checkpoint_enable(NULL,session);}
  else if(!strcmp(site,"enable-usb-snapshot")){loss_site=loss?"snapshot":NULL;bad_snapshot=!loss;rc=checkpoint->checkpoint_enable(NULL,session);}
  else{
   enable();consume_activity();
   if(!strcmp(site,"checkpoint-snapshot")){loss_site=loss?"scene-snapshot":NULL;if(!loss)assert(0);risc_text_entry_checkpoint_state_v1 s={.struct_size=sizeof(s)};rc=checkpoint->checkpoint_snapshot(NULL,session,&s);}
   else if(!strcmp(site,"close-subscription")){loss_site=loss?"unsubscribe":NULL;bad_unsubscribe=!loss;assert(!checkpoint->pause(NULL,session));rc=api->close(NULL,session);}
   else if(!strcmp(site,"pause-scene")){loss_site=loss?"pause":NULL;review_pause_result=loss?0:RISC_SCENE_INVALID;rc=checkpoint->pause(NULL,session);}
   else{assert(!checkpoint->pause(NULL,session));assert(review_paused);
    if(!strcmp(site,"resume-usb-snapshot")){loss_site=loss?"snapshot":NULL;bad_snapshot=!loss;}
    else if(!strcmp(site,"resume-scene")){loss_site=loss?"resume":NULL;review_resume_result=loss?0:RISC_SCENE_INVALID;}
    else assert(0);
    rc=checkpoint->resume(NULL,session,RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED);
   }
  }
  assert(rc==RISC_TEXT_ENTRY_RETAINED);retained_fence();
 }else{
  enable();
  if(!strcmp(mode,"flags")){pending=true;assert(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING);pending=false;review_input_busy=true;assert(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY);review_input_busy=false;review_activity=true;assert(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY);assert(!(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY));finish_review();}
  else if(!strcmp(mode,"usb-flags")){key(4,0);assert(!strcmp(tick().text,"drafta"));assert(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY);assert(!(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY));flush();held=true;tick();assert(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY);assert(checkpoint->pause(NULL,session)==RISC_TEXT_ENTRY_BUSY);held=false;consume_activity();assert(!checkpoint->pause(NULL,session));finish_review();}
  else if(!strcmp(mode,"usb-activity-race")){consume_activity();key(0x39,0);assert(checkpoint->pause(NULL,session)==RISC_TEXT_ENTRY_BUSY);assert(!review_paused&&kb_active);assert(snapshot_flags()&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY);flush();assert(!checkpoint->pause(NULL,session));finish_review();}
  else if(!strcmp(mode,"usb-queue")){for(unsigned i=0;i<34;i++)key(4,0);assert(checkpoint->pause(NULL,session)==RISC_TEXT_ENTRY_BUSY);assert(!review_paused);consume_activity();assert(!strcmp(tick().text,"draft"));assert(!checkpoint->pause(NULL,session));finish_review();}
  else if(!strcmp(mode,"outcome-race")||!strcmp(mode,"home-race")||!strcmp(mode,"key-race")){
   if(!strcmp(mode,"home-race"))event=(risc_scene_event_v1){.struct_size=sizeof(event),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=doc.revision};else scene_key(!strcmp(mode,"outcome-race")?RISC_SCENE_KEY_DONE:'Z');
   assert(checkpoint->pause(NULL,session)==RISC_TEXT_ENTRY_BUSY);assert(!review_paused);risc_text_entry_state_v1 s=tick();
   if(!strcmp(mode,"key-race"))assert(s.state==RISC_TEXT_ENTRY_EDITING&&!strcmp(s.text,"draftZ"));else assert(s.state==(!strcmp(mode,"home-race")?RISC_TEXT_ENTRY_CANCELLED:RISC_TEXT_ENTRY_ACCEPTED));
   if(!strcmp(mode,"home-race"))assert(s.flags&RISC_TEXT_ENTRY_HOME_CANCEL);
   finish_review();
  }else if(!strcmp(mode,"layer")||!strcmp(mode,"caps")){
   if(!strcmp(mode,"layer")){scene_key(RISC_SCENE_KEY_LAYER);tick();scene_key(RISC_SCENE_KEY_LAYER);tick();assert(doc.nodes[0].value==2);}else{key(0x39,0);tick();flush();}
   consume_activity();uint32_t revision=doc.revision;assert(!checkpoint->pause(NULL,session));assert(snapshot_flags()==RISC_TEXT_ENTRY_CHECKPOINT_PAUSED);assert(!checkpoint->pause(NULL,session));assert(review_pauses==1);
   risc_text_entry_state_v1 blocked={.struct_size=sizeof(blocked)};unsigned before=calls;assert(api->poll(NULL,session,&blocked)==RISC_TEXT_ENTRY_BUSY&&calls==before);
   if(!strcmp(mode,"caps"))held=true;
   assert(!checkpoint->resume(NULL,session,RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED));assert(!review_paused&&review_resumes==1&&doc.revision==revision);
   if(!strcmp(mode,"layer")){assert(doc.nodes[0].value==2);scene_key('1');assert(!strcmp(tick().text,"draft1"));}
   else{key(4,0);assert(!strcmp(tick().text,"draft"));flush();held=false;tick();key(5,0);assert(!strcmp(tick().text,"draftB"));}
   finish_review();
  }else if(!strncmp(mode,"usb-full-",9)){
   consume_activity();const char *kind=mode+9;
   if(!strcmp(kind,"pause-loss")||!strcmp(kind,"pause-fault")){
    loss_site=!strcmp(kind,"pause-loss")?"unsubscribe":NULL;bad_unsubscribe=!strcmp(kind,"pause-fault");assert(checkpoint->pause_for_focus_change(NULL,session)==RISC_TEXT_ENTRY_RETAINED);retained_fence();return 0;
   }
   review_pause_wait=!strcmp(kind,"again")?2:0;
   int rc=checkpoint->pause_for_focus_change(NULL,session);
   while(rc==RISC_TEXT_ENTRY_AGAIN)rc=checkpoint->pause_for_focus_change(NULL,session);
   assert(!rc&&review_paused&&!kb_active&&unsubs==1);
   if(!strcmp(kind,"close")){finish_review();assert(unsubs==1);}
   else if(!strcmp(kind,"resume-loss")||!strcmp(kind,"snapshot-loss")||!strcmp(kind,"snapshot-fault")){
    loss_site=!strcmp(kind,"resume-loss")?"subscribe":!strcmp(kind,"snapshot-loss")?"snapshot":NULL;bad_snapshot=!strcmp(kind,"snapshot-fault");assert(checkpoint->resume(NULL,session,0)==RISC_TEXT_ENTRY_RETAINED);retained_fence();return 0;
   }else{
    held=true;key(4,0);assert(!checkpoint->resume(NULL,session,0)&&kb_active);assert(!strcmp(tick().text,"draft"));flush();held=false;tick();key(5,0);assert(!strcmp(tick().text,"draftb"));finish_review();assert(unsubs==2);
   }
  }else if(!strncmp(mode,"usb-continuity-",15)){
   consume_activity();unsigned before_unsubs=unsubs;assert(!checkpoint->pause(NULL,session));assert(kb_active&&unsubs==before_unsubs);
   const char *kind=mode+15;
   if(!strcmp(kind,"repeated")){
    for(unsigned i=0;i<8;i++){key(4,0);keys[key_count++]=(risc_usb_keyboard_event_v1){++sequence,7,4,4,0,0};}
    assert(!checkpoint->resume(NULL,session,0));assert(!strcmp(tick().text,"draftaaaaaaaa"));assert(unsubs==before_unsubs);
   }else if(!strcmp(kind,"held")){
    held=true;key(4,0);assert(!checkpoint->resume(NULL,session,0));assert(!strcmp(tick().text,"drafta"));flush();assert(!strcmp(tick().text,"drafta"));held=false;keys[key_count++]=(risc_usb_keyboard_event_v1){++sequence,7,4,4,0,0};tick();flush();key(4,0);assert(!strcmp(tick().text,"draftaa"));
   }else if(!strcmp(kind,"cancel")||!strcmp(kind,"done")){
    key(4,0);key(!strcmp(kind,"cancel")?0x29:0x28,0);key(5,0);assert(!checkpoint->resume(NULL,session,0));risc_text_entry_state_v1 result=tick();assert(!strcmp(result.text,"drafta")&&result.state==(!strcmp(kind,"cancel")?RISC_TEXT_ENTRY_CANCELLED:RISC_TEXT_ENTRY_ACCEPTED));
   }else if(!strcmp(kind,"sleep")){
    held=true;key(4,0);assert(!checkpoint->resume(NULL,session,RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED));assert(!strcmp(tick().text,"draft"));flush();held=false;tick();key(5,0);assert(!strcmp(tick().text,"draftb"));
   }else if(!strcmp(kind,"overflow")){
    for(unsigned i=0;i<34;i++){key(4,0);}assert(!checkpoint->resume(NULL,session,0));assert(!strcmp(tick().text,"draft"));assert(!strcmp(tick().text,"draft"));flush();key(5,0);assert(!strcmp(tick().text,"draftb"));
   }else if(!strcmp(kind,"close")){key(4,0);assert(!api->close(NULL,session));assert(!kb_active&&unsubs==before_unsubs+1);assert(driver->quiesce());driver->stop();printf("review checkpoint %s PASS\n",mode);return 0;}
   else assert(0);
   finish_review();
  }else if(!strcmp(mode,"pause-again")||!strcmp(mode,"close-pausing")||!strcmp(mode,"close-paused")){
   consume_activity();review_pause_wait=strcmp(mode,"close-paused")?2:0;int rc=checkpoint->pause(NULL,session);
   if(review_pause_wait){assert(rc==RISC_TEXT_ENTRY_AGAIN);risc_text_entry_checkpoint_state_v1 s={.struct_size=sizeof(s)};assert(checkpoint->checkpoint_snapshot(NULL,session,&s)==RISC_TEXT_ENTRY_BUSY);}
   if(!strcmp(mode,"pause-again")){assert(checkpoint->pause(NULL,session)==RISC_TEXT_ENTRY_AGAIN);assert(!checkpoint->pause(NULL,session));assert(!checkpoint->resume(NULL,session,0));}
   finish_review();
  }else if(!strcmp(mode,"invalid")){risc_text_entry_checkpoint_state_v1 s={0};unsigned before=calls;assert(checkpoint->checkpoint_snapshot(NULL,session,NULL)==RISC_TEXT_ENTRY_INVALID);assert(checkpoint->checkpoint_snapshot(NULL,session,&s)==RISC_TEXT_ENTRY_INVALID);assert(checkpoint->resume(NULL,session,4)==RISC_TEXT_ENTRY_INVALID);assert(checkpoint->checkpoint_enable(NULL,session+1)==RISC_TEXT_ENTRY_STALE);assert(checkpoint->resume(NULL,session,0)==RISC_TEXT_ENTRY_BUSY);assert(calls==before);finish_review();}
  else assert(0);
 }
 printf("review checkpoint %s PASS (callbacks=%u configures=%u pauses=%u resumes=%u)\n",mode,calls,review_configures,review_pauses,review_resumes);return 0;
}
