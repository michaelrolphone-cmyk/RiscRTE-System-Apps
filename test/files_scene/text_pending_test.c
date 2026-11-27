#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../Apps/files_scene_text_pending.h"
typedef struct fixture {
 files_text_pending pending;
 files_text_identity current;
 files_text_intent opened;
 bool live;
 unsigned retains,snapshots,drains,opens;
 int drain_rc,open_rc;
 unsigned effect;
} fixture;
static bool alive(void*c){return ((fixture*)c)->live;}
static void retain(void*c){((fixture*)c)->retains++;}
static bool snapshot(void*c,files_text_identity*out){
 fixture*f=c;f->snapshots++;*out=f->current;
 if(f->effect==1)f->live=false;
 if(f->effect==2)(void)files_text_pending_step(&f->pending);
 return true;
}
static int drain(void*c){
 fixture*f=c;f->drains++;
 if(f->effect==3)f->current.storage++;
 if(f->effect==4)f->live=false;
 if(f->effect==5)(void)files_text_pending_step(&f->pending);
 if(f->effect==6)files_text_pending_cancel(&f->pending);
 return f->drain_rc;
}
static int open_text(void*c,const files_text_intent*i){
 fixture*f=c;f->opens++;f->opened=*i;
 if(f->effect==7)f->live=false;
 if(f->effect==8)(void)files_text_pending_request(&f->pending,9,"Other","bad",128);
 return f->open_rc;
}
static void init(fixture*f){
 memset(f,0,sizeof(*f));f->live=true;f->drain_rc=f->open_rc=1;
 f->current.mode=4;f->current.choice=2;f->current.storage=1;
 f->current.grant_slot=3;f->current.grant_generation=7;
 strcpy(f->current.path,"/folder");strcpy(f->current.file_path,"/folder/old");
 strcpy(f->current.selected.name,"old");f->current.selected.size=9;
 files_text_pending_init(&f->pending,f,alive,retain,snapshot,drain,open_text);
}
static void request(fixture*f){
 unsigned opens=f->opens,drains=f->drains;
 assert(files_text_pending_request(&f->pending,6,"Rename","old",128));
 assert(f->pending.pending&&f->opens==opens&&f->drains==drains);
}
static void unchanged_and_once(void){
 fixture f;init(&f);char label[16]="Rename",draft[16]="copied";
 assert(files_text_pending_request(&f.pending,6,label,draft,128));
 strcpy(label,"changed");strcpy(draft,"mutated");
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_OPENED);
 assert(f.opens==1&&f.drains==1&&!strcmp(f.opened.label,"Rename")&&!strcmp(f.opened.draft,"copied"));
 assert(f.opened.identity.mode==4&&f.opened.identity.choice==2);
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_WAIT&&f.opens==1);
}
static void again_no_retarget(void){
 fixture f;init(&f);request(&f);f.drain_rc=0;
 for(unsigned i=0;i<8;i++)assert(files_text_pending_step(&f.pending)==FILES_TEXT_WAIT);
 assert(f.drains==8&&!f.opens);
 assert(!files_text_pending_request(&f.pending,7,"New","replacement",128));
 assert(f.pending.intent.requested_mode==6&&!strcmp(f.pending.intent.draft,"old"));
 f.drain_rc=1;assert(files_text_pending_step(&f.pending)==FILES_TEXT_OPENED&&f.opens==1);
}
static void stale_fields(void){
 for(unsigned k=0;k<13;k++){
  fixture f;init(&f);request(&f);
  switch(k){
  case 0:f.current.mode++;break;case 1:f.current.storage++;break;
  case 2:f.current.choice++;break;case 3:f.current.grant_generation++;break;
  case 4:f.current.path[1]='X';break;case 5:f.current.file_path[1]='Y';break;
  case 6:f.current.selected.name[0]='Z';break;case 7:f.current.selected.size++;break;
  case 8:f.current.mounted=true;break;case 9:f.current.new_directory=true;break;
  case 10:f.current.pairing_generation++;break;case 11:f.current.operation_generation++;break;
  case 12:f.current.wait_generation++;break;
  }
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_CANCELLED&&!f.opens&&f.drains==1);
 }
}
static void cancellation_drains(void){
 fixture f;init(&f);request(&f);files_text_pending_cancel(&f.pending);f.drain_rc=0;
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_WAIT&&f.pending.pending&&!f.opens);
 f.drain_rc=1;assert(files_text_pending_step(&f.pending)==FILES_TEXT_CANCELLED&&!f.opens);
 /* Explicit hook excludes same-field mount teardown/replacement (ABA). */
 init(&f);request(&f);f.effect=6;
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_CANCELLED&&!f.opens);
}
static void changed_during_drain(void){
 fixture f;init(&f);request(&f);f.effect=3;
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_CANCELLED&&!f.opens);
}
static void terminal_drain(void){
 const int errors[]={-1,2,-99};
 for(unsigned k=0;k<sizeof(errors)/sizeof(errors[0]);k++){
  fixture f;init(&f);request(&f);f.drain_rc=errors[k];
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_RETAINED&&f.retains==1&&!f.opens);
  unsigned calls=f.drains+f.snapshots;
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_RETAINED);
  assert(!files_text_pending_request(&f.pending,6,"Again","old",128));
  files_text_pending_cancel(&f.pending);assert(f.drains+f.snapshots==calls);
 }
}
static void identity_loss(void){
 for(unsigned k=0;k<3;k++){
  fixture f;init(&f);request(&f);f.effect=k==0?1:k==1?4:7;
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_RETAINED);
  if(k<2)assert(!f.opens);
  unsigned calls=f.drains+f.snapshots+f.opens;
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_RETAINED);
  assert(calls==f.drains+f.snapshots+f.opens);
 }
}
static void reentry(void){
 const unsigned effects[]={2,5,8};
 for(unsigned k=0;k<3;k++){
  fixture f;init(&f);request(&f);f.effect=effects[k];
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_RETAINED&&f.retains==1);
  assert(f.opens==(k==2?1u:0u));
 }
}
static void clean_open_failure(void){
 fixture f;init(&f);request(&f);f.open_rc=0;
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_UNAVAILABLE&&!f.pending.pending&&f.opens==1);
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_WAIT&&f.opens==1);
 request(&f);f.open_rc=1;assert(files_text_pending_step(&f.pending)==FILES_TEXT_OPENED&&f.opens==2);
}
static void unknown_open_failure(void){
 const int errors[]={-1,2,-99};
 for(unsigned k=0;k<3;k++){
  fixture f;init(&f);request(&f);f.open_rc=errors[k];
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_RETAINED&&f.retains==1&&f.opens==1);
  assert(files_text_pending_step(&f.pending)==FILES_TEXT_RETAINED&&f.opens==1);
 }
}
static void bounded_strings(void){
 fixture f;init(&f);char longlabel[RISC_TEXT_ENTRY_LABEL],draft[128];
 memset(longlabel,'x',sizeof(longlabel));memset(draft,'x',sizeof(draft));
 assert(!files_text_pending_request(&f.pending,6,longlabel,"old",128));
 assert(!files_text_pending_request(&f.pending,6,"Name",draft,128));
 assert(!files_text_pending_request(&f.pending,6,"Name","old",0));
 assert(!files_text_pending_request(&f.pending,6,"Name","old",129));
 draft[127]=0;assert(files_text_pending_request(&f.pending,6,"Name",draft,128));
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_OPENED&&strlen(f.opened.draft)==127);
}
static void malformed_snapshot(void){
 fixture f;init(&f);memset(f.current.path,'x',sizeof(f.current.path));
 assert(!files_text_pending_request(&f.pending,6,"Name","old",128)&&!f.drains&&!f.opens);
 init(&f);request(&f);memset(f.current.selected.name,'x',sizeof(f.current.selected.name));
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_CANCELLED&&!f.opens);
}
static void fresh_new_folder(void){
 fixture f;init(&f);f.current.new_directory=true;
 assert(files_text_pending_request(&f.pending,6,"New folder","",128));
 assert(f.current.new_directory);f.drain_rc=0;
 assert(files_text_pending_step(&f.pending)==FILES_TEXT_WAIT&&f.current.new_directory);
 f.drain_rc=1;assert(files_text_pending_step(&f.pending)==FILES_TEXT_OPENED);
 assert(f.opened.identity.new_directory&&!strcmp(f.opened.draft,""));
}
int main(void){
 unchanged_and_once();again_no_retarget();stale_fields();cancellation_drains();
 changed_during_drain();terminal_drain();identity_loss();reentry();clean_open_failure();
 unknown_open_failure();bounded_strings();malformed_snapshot();fresh_new_folder();
 puts("13 Files pending-text groups passed");return 0;
}
