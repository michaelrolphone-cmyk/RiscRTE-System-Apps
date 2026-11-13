#define main original_scene_test_main
#define risc_runtime_get_api original_runtime_get_api
#include "../scene/host_test.c"
#undef risc_runtime_get_api
#undef main
#include "RiscSceneCanvasV1.h"
static bool replace_runtime,reenter;
static unsigned reentered;
static const risc_canvas_api_v1*canvas;
static void cooperate(uint32_t n){
 ms+=n;
 if(reenter){uint64_t other=0;unsigned before=calls;assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_BUSY);risc_canvas_open_v1 request={sizeof(request),RISC_CANVAS_MONO1,RISC_CANVAS_FIT,8,8};assert(canvas->open(NULL,&request,&other)==RISC_CANVAS_BUSY);assert(calls==before);reentered++;}
}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){static const risc_runtime_api_v1 first={.api_version=1,.struct_size=sizeof(first),.yield_ms=cooperate},second={.api_version=1,.struct_size=sizeof(second),.yield_ms=cooperate};return version==1&&native_alive?(replace_runtime?&second:&first):NULL;}
static uint64_t open_canvas(void){risc_canvas_open_v1 request={sizeof(request),RISC_CANVAS_MONO1,RISC_CANVAS_FIT,8,8};uint64_t s=0;assert(!canvas->open(NULL,&request,&s)&&s);return s;}
static void no_io_retained(uint64_t s){unsigned before=calls;uint64_t other=0;risc_canvas_geometry_v1 geometry={.struct_size=sizeof(geometry)};assert(canvas->geometry(NULL,s,&geometry)==RISC_CANVAS_RETAINED);assert(canvas->close(NULL,s)==RISC_CANVAS_RETAINED);assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_RETAINED);assert(!driver->quiesce());driver->stop();assert(calls==before);}
int main(int argc,char**argv){
 assert(argc==3);const char*mode=argv[1];if(!strcmp(argv[2],"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,90,0,0};}else if(!strcmp(argv[2],"large")){w=h=410;}
 startup();canvas=risc_scene_canvas_get_v1(api);assert(canvas&&api->api_version==1);
 const risc_scene_checkpoint_api_v1*checkpoint=risc_scene_checkpoint_get_v1(api);assert(checkpoint&&risc_scene_components_get_v1(api)&&risc_scene_reading_links_get_v1(api));
 if(!strcmp(mode,"prefix")){
#ifdef EXPECT_COMPACT
 assert(!risc_scene_telemetry_get_v1(api));
#else
 assert(risc_scene_telemetry_get_v1(api));
#endif
 risc_scene_canvas_api_v1 copy=*(const risc_scene_canvas_api_v1*)api;((risc_scene_api_v1*)&copy)->struct_size=sizeof(risc_scene_telemetry_api_v1);assert(!risc_scene_canvas_get_v1((void*)&copy));finish();
 }else if(!strcmp(mode,"scene-busy")){risc_canvas_open_v1 req={sizeof(req),1,1,8,8};uint64_t s=0;unsigned before=calls;assert(canvas->open(NULL,&req,&s)==RISC_CANVAS_BUSY&&calls==before);assert(!checkpoint->pause(NULL,session));before=calls;assert(canvas->open(NULL,&req,&s)==RISC_CANVAS_BUSY&&calls==before);finish();
 }else if(!strcmp(mode,"scene-fault")){bad_unsubscribe=true;assert(api->close(NULL,session)==RISC_SCENE_RETAINED);no_io_retained(1);
 }else{
 assert(!api->close(NULL,session));
 if(!strcmp(mode,"open-native")){loss_callback="get_info";risc_canvas_open_v1 req={sizeof(req),RISC_CANVAS_MONO1,RISC_CANVAS_FIT,8,8};uint64_t token=0;assert(canvas->open(NULL,&req,&token)==RISC_CANVAS_RETAINED);no_io_retained(token);return 0;}
 uint64_t s=open_canvas(),other=0;
 if(!strcmp(mode,"switch")){unsigned before=calls;assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_BUSY&&calls==before);assert(!canvas->quiesce(NULL,s));before=calls;assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_BUSY&&calls==before);assert(!canvas->resume(NULL,s));assert(!canvas->close(NULL,s)&&driver->quiesce());assert(!api->open(NULL,&doc,NULL,&session));finish();}
 else if(!strcmp(mode,"lease")){risc_canvas_frame_v1 frame={.struct_size=sizeof(frame)};assert(!canvas->begin_frame(NULL,s,&frame));assert(canvas->close(NULL,s)==RISC_CANVAS_BUSY&&!driver->quiesce());assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_BUSY);assert(!canvas->cancel_frame(NULL,s,frame.lease));assert(!canvas->close(NULL,s)&&driver->quiesce());driver->stop();}
 else if(!strcmp(mode,"receipt")||!strcmp(mode,"reentrant")){reenter=!strcmp(mode,"reentrant");risc_canvas_frame_v1 frame={.struct_size=sizeof(frame)};assert(!canvas->begin_frame(NULL,s,&frame));memset(frame.pixels,255,frame.byte_count);allow_complete=false;expected_intent=RISC_DISPLAY_PRESENT_DEFAULT;expected_queue=RISC_DISPLAY_QUEUE_FIFO;uint64_t receipt;assert(!canvas->submit(NULL,s,frame.lease,&receipt));assert(canvas->close(NULL,s)==RISC_CANVAS_AGAIN&&!driver->quiesce());assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_BUSY);allow_complete=true;assert(!canvas->close(NULL,s)&&driver->quiesce());if(reenter)assert(reentered);driver->stop();}
 else if(!strcmp(mode,"canvas-fault")){bad_unsubscribe=true;assert(canvas->close(NULL,s)==RISC_CANVAS_RETAINED);no_io_retained(s);}
 else if(!strcmp(mode,"submit-native")){risc_canvas_frame_v1 frame={.struct_size=sizeof(frame)};assert(!canvas->begin_frame(NULL,s,&frame));loss_callback="submit";expected_intent=RISC_DISPLAY_PRESENT_DEFAULT;expected_queue=RISC_DISPLAY_QUEUE_FIFO;uint64_t receipt=0;assert(canvas->submit(NULL,s,frame.lease,&receipt)==RISC_CANVAS_RETAINED);no_io_retained(s);}
 else if(!strcmp(mode,"stale")){risc_canvas_geometry_v1 g={.struct_size=sizeof(g)};unsigned before=calls;assert(canvas->geometry(NULL,s+1,&g)==RISC_CANVAS_STALE&&calls==before);assert(!canvas->close(NULL,s));driver->stop();}
 else if(!strcmp(mode,"replacement")){replace_runtime=true;no_io_retained(s);}
 else if(!strcmp(mode,"null")){native_alive=false;no_io_retained(s);}
 else assert(0);
 }
 printf("scene canvas %s %s PASS\n",mode,argv[2]);return 0;
}
