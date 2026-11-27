/* Actual shared scene provider; transports are deterministic host doubles. */
#define main unused_host_main
#include "../scene/host_test.c"
#undef main
#include "RiscSceneLifecycleV1.h"
static void run_case(unsigned features,bool repeat,bool transition){
 startup();const risc_scene_lifecycle_api_v1 *life=risc_scene_lifecycle_get_v1(api);assert(life);
 settle();assert(life->configure(NULL,session,features)==0);settle();
 /* New revision is already installed, with no older input queued. */
 doc.revision++;assert(api->update(NULL,session,&doc)==0);settle();
 nav_pressed=RISC_NAV_HOME; /* Current-revision key awaiting provider poll. */
 if(repeat)assert(life->configure(NULL,session,transition?0:features)==0);
 risc_scene_event_v1 e;int rc=tick(&e);
 bool lost=repeat&&(!(features&RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY)||transition);
 assert(rc==(lost?RISC_SCENE_IDLE:RISC_SCENE_OK));
 if(!lost)assert(e.document_revision==doc.revision&&e.kind==RISC_SCENE_SUSPEND_EVENT);
 printf("actual provider: features=%u repeat=%u transition=%u current_revision=%u key=%s\n",features,repeat,transition,doc.revision,lost?"consumed":"delivered");
 printf("event rc=%d struct_size=%u kind=%u revision=%u node=%u action=%u value=%d sequence=%llu\n",rc,e.struct_size,e.kind,e.document_revision,e.node,e.action,e.value,(unsigned long long)e.sequence);
 finish();
}
int main(int argc,char **argv){
 assert(argc==2);
 if(!strcmp(argv[1],"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};}
 else if(!strcmp(argv[1],"gray")){w=480;h=800;format=3;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,0,0,0};}
 run_case(1,false,false);run_case(1,true,false);run_case(3,true,false);run_case(3,true,true);return 0;}
