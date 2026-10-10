#include "bootstrap/Runtime.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscUsbHidV1.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <memory>
#include <string>
static std::string mode,activation;
static unsigned entries,loads,unloads,calls,starts,stops,frames,nav;
static bool native_lost;
static void provider_io(const char*name){
 assert(!native_lost&&"provider I/O after native retention");++calls;
 if(mode!=std::string("native-")+name&&mode!=std::string("native-")+name+"-fail")return;
 const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r&&r->retain_invocation&&r->retain_invocation());
 assert(!risc_runtime_get_api(1));native_lost=true;
}
static bool held,touch_sub,keyboard_sub,attached,pending,bad_unsubscribe;
static uint8_t pixels[800*480*2];
static uint64_t ms;
static bool paper,contact_held;
static risc_touch_event_v1 touches[32];static unsigned touch_at,touch_count;static uint64_t touch_sequence;
static unsigned panel_width(){return paper?800u:240u;}
static unsigned panel_height(){return paper?480u:240u;}
static void tap(unsigned row,unsigned col,bool down=true,bool up=true){
 static const unsigned xs[4][10]={{52,94,135,177,218,260,302,343,385,427},{73,114,156,198,239,281,322,364,406},{63,114,156,198,239,281,322,364,416},{52,198,343,406}};
 unsigned x=xs[row][col]/(paper?1:2),y=paper?412+row*92:97+row*30;
 if(down)touches[touch_count++]={++touch_sequence,ms,RISC_TOUCH_EVENT_DOWN,0,(uint16_t)x,(uint16_t)y};
 if(up)touches[touch_count++]={++touch_sequence,ms+10,RISC_TOUCH_EVENT_UP,0,(uint16_t)x,(uint16_t)y};
 contact_held=!up;
}
static uint64_t sequence,last_token;
static const void*app_pointer;
static risc_usb_keyboard_event_v1 queue[8];static unsigned queued,at;
extern "C" bool text_runtime_paper(){return paper;}
extern "C" bool text_runtime_arm_retention(){return activation=="demand-retained-armed";}
extern "C" unsigned text_runtime_entry(){return ++entries;}
extern "C" const char*text_runtime_mode(){return mode.c_str();}
// Demand activation may unload/reload the host between apps; tokens have no
// identity outside their live session/grant. Eager and explicitly armed
// demand-retained pins preserve host generations.
extern "C" void text_runtime_check_token(uint64_t token){assert(token);if(activation=="eager"||activation=="demand-retained-armed")assert(token!=last_token);last_token=token;}
extern "C" void text_runtime_app_event(unsigned n,const void*p){if(n==1){++loads;app_pointer=p;}else{assert(n==2);++unloads;}}
static void push(unsigned kind,unsigned usage){queue[queued++]={++sequence,7,(uint8_t)kind,(uint8_t)usage,0,0};}
extern "C" void text_runtime_control(unsigned command){
 queued=at=0;touch_at=touch_count=0;
 if(command==1){attached=true;push(1,0);push(3,4);}
 else if(command==2)push(3,4);
 else if(command==3){attached=false;push(2,0);push(3,5);}
 else if(command==4){pending=true; /* Force a pending host frame after edit. */
   nav=RISC_NAV_LEFT;
 }
 else if(command==5)pending=false;
 else if(command==6)nav=RISC_NAV_BACK;
 else if(command==9)bad_unsubscribe=true;
 else if(command==10){pending=true;tap(0,0);}
 else if(command==11){for(unsigned col:{0u,0u,1u,2u,1u,0u})tap(0,col);}
 else if(command==12)tap(0,0,true,false);
 else if(command==13)tap(0,0,false,true);
 else if(command==14){pending=true;tap(2,0);}
 else if(command==15)tap(0,0);
 else if(command==16)tap(3,3);
 else if(command==19){assert(paper);touches[touch_count++]={++touch_sequence,ms,RISC_TOUCH_EVENT_DOWN,0,408,192};touches[touch_count++]={++touch_sequence,ms+10,RISC_TOUCH_EVENT_UP,0,408,192};}
 else if(command>=100&&command<110)tap(0,command-100);
 else assert(0);
}
static bool info(void*,risc_display_info_v1*out){provider_io("info");*out={};out->api_version=1;out->struct_size=sizeof(*out);out->width=panel_width();out->height=panel_height();out->supported_formats=RISC_DISPLAY_FORMAT_BIT(5);out->preferred_format=5;return true;}
static bool acquire(void*,uint32_t f,risc_display_surface_v1*out){provider_io("acquire");assert(!held&&f==5);held=true;*out={frames+1,pixels,panel_width(),panel_height(),panel_width()*2,sizeof(pixels),5};return true;}
static void release(void*,uint64_t f){provider_io("release");assert(held&&f);held=false;}
static bool submit(void*,uint64_t f,const risc_display_rect_v1*,size_t n,const risc_display_present_options_v1*,uint64_t*out){provider_io("submit");assert(held&&f&&!n);*out=++frames;held=false;return true;}
static bool status(void*,uint64_t f,risc_display_present_status_v1*out){provider_io("status");assert(!held&&f);out->state=pending?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),nullptr,info,acquire,release,submit,status,nullptr,nullptr};
static uint64_t subscribe(void*){provider_io("subscribe");assert(!touch_sub);touch_sub=true;return 1;}
static bool unsubscribe(void*,uint64_t s){provider_io("unsubscribe");assert(s==1&&touch_sub);if(bad_unsubscribe)return false;touch_sub=false;return true;}
static bool touch_poll(void*,size_t){provider_io("touch_poll");ms+=10;return true;}
static int32_t next(void*,uint64_t s,risc_touch_event_v1*out){provider_io("next");assert(s==1&&touch_sub);if(touch_at==touch_count)return 0;*out=touches[touch_at++];return 1;}
static bool snapshot(void*,risc_touch_snapshot_v1*out){provider_io("snapshot");*out={};out->width=paper?480:240;out->height=paper?800:240;out->sequence=touch_sequence;out->contact_count=contact_held?1:0;return mode!="native-snapshot-fail";}
static const risc_touch_api_v1 touch={1,sizeof(touch),nullptr,subscribe,unsubscribe,touch_poll,next,snapshot};
static bool navigation_poll(void*,risc_input_navigation_frame_v1*out){provider_io("navigation_poll");*out={};out->pressed=nav;nav=0;return true;}
static bool foreground(void*,const risc_input_foreground_v1*,size_t){provider_io("foreground");return true;}
static bool reset(void*){provider_io("reset");nav=0;return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),nullptr,navigation_poll,foreground,reset};
static uint64_t now(void*){return ms;}
static void sleep(void*,uint32_t){assert(0);}
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),nullptr,now,sleep};
static uint64_t keyboard_subscribe(void*,uint64_t){provider_io("keyboard_subscribe");assert(!keyboard_sub);keyboard_sub=true;return 2;}
static bool keyboard_unsubscribe(void*,uint64_t s){provider_io("keyboard_unsubscribe");assert(s==2&&keyboard_sub);keyboard_sub=false;return true;}
static bool keyboard_poll(void*,size_t){provider_io("keyboard_poll");return true;}
static int32_t keyboard_next(void*,uint64_t s,risc_usb_keyboard_event_v1*out){provider_io("keyboard_next");assert(s==2&&keyboard_sub);if(at==queued)return 0;*out=queue[at++];return 1;}
static bool keyboard_snapshot(void*,risc_usb_keyboard_state_v1*out,size_t*count){provider_io("keyboard_snapshot");*count=attached?1:0;if(attached){*out={};out->device=7;out->connected=1;}return true;}
static const risc_usb_keyboard_api_v1 keyboard={1,sizeof(keyboard),nullptr,keyboard_subscribe,keyboard_unsubscribe,keyboard_poll,keyboard_next,keyboard_snapshot};
extern "C" const void*text_runtime_provider(const char*name){if(!strcmp(name,"display.output"))return &display;if(!strcmp(name,"input.touch.raw"))return &touch;if(!strcmp(name,"input.navigation"))return &navigation;if(!strcmp(name,"usb.hid.keyboard"))return &keyboard;assert(0);return nullptr;}
extern "C" bool text_runtime_provider_event(const char*name,unsigned event){if(event==1)++starts;else if(event==2)++stops;else if(event==3){if(!strcmp(name,"display.output"))return !held;if(!strcmp(name,"input.touch.raw"))return !touch_sub;if(!strcmp(name,"usb.hid.keyboard"))return !keyboard_sub;}return true;}
int main(int argc,char**argv){
 assert(argc==4);paper=std::getenv("TEXT_TEST_PAPER")!=nullptr;mode=argv[2];activation=argv[3];bool retained=mode=="retained"||mode=="unclosed"||mode.rfind("native-",0)==0;
 RiscBoot::Port port{[](){return true;},[](risc_runtime_health_v1*){return true;},[](uint32_t n){ms+=n;},[](const char*){return true;}};
 port.retainedDelay=[](uint32_t){};
 port.bindPlatforms=[](RiscBoot::Runtime&r){return r.registerPlatform("platform.clock",1,RiscBoot::Runtime::Scope::Global,0,&clock_api);};
 auto runtime=std::make_unique<RiscBoot::Runtime>(port);assert(runtime->prepare(argv[1]));bool ran=runtime->run();
 if(ran==retained){fprintf(stderr,"unexpected Runtime result: %s\n",runtime->error());assert(0);}
 assert(runtime->retained()==retained);if(mode.rfind("native-",0)==0)assert(native_lost);Dl_info mapping{};
 if(retained){assert(loads==1);if(mode=="unclosed"&&(activation=="eager"||activation=="demand-retained-armed")){assert(unloads==1&&!dladdr(app_pointer,&mapping));assert(strstr(runtime->error(),"quiescence"));}else assert(!unloads&&dladdr(app_pointer,&mapping));unsigned before=calls;assert(!runtime->run()&&calls==before);printf("text real Runtime %s/%s PASS (provider graph retained, app unloads=%u)\n",activation.c_str(),mode.c_str(),unloads);fflush(stdout);std::_Exit(0);}
 assert(loads==3&&unloads==3&&entries==3&&!held&&!touch_sub&&!keyboard_sub);assert(!dladdr(app_pointer,&mapping));runtime.reset();assert(starts==stops);
 printf("text real Runtime %s/%s PASS (3 actual app unloads, %u frames, %u provider calls)\n",activation.c_str(),mode.c_str(),frames,calls);
}
