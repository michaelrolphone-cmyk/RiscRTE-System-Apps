#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "RiscDisplayOutputV1.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
static uint64_t milliseconds,present_at;
static unsigned width=240,height=240,format=5,frames,starts,stops;
static bool held,pending,complete;
static unsigned char pixels[466*466*2];
static bool info(void*,risc_display_info_v1*out){*out={};out->api_version=1;out->struct_size=sizeof(*out);out->width=width;out->height=height;out->supported_formats=RISC_DISPLAY_FORMAT_BIT(format);out->preferred_format=format;return true;}
static bool acquire(void*,uint32_t f,risc_display_surface_v1*out){assert(!held&&!pending&&f==format);held=true;unsigned stride=format==5?width*2:(width+7)/8;*out={frames+1,pixels,width,height,stride,stride*height,format};return true;}
static void release(void*,uint64_t f){assert(held&&f);held=false;}
static bool submit(void*,uint64_t f,const risc_display_rect_v1*,size_t count,const risc_display_present_options_v1*,uint64_t*out){assert(held&&!pending&&f&&!count);held=false;pending=true;present_at=milliseconds;*out=++frames;return true;}
static bool status(void*,uint64_t token,risc_display_present_status_v1*out){assert(token==frames&&!held);if(milliseconds-present_at>=5)pending=false;out->state=pending?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),nullptr,info,acquire,release,submit,status,nullptr,nullptr};
extern "C" const void*canvas_runtime_display(){return &display;}
extern "C" bool canvas_runtime_event(unsigned event){if(event==1)starts++;else if(event==2)stops++;else if(event==3)return !held&&!pending;return true;}
extern "C" void canvas_runtime_complete(){complete=true;}
int main(int argc,char**argv){assert(argc==2);
 RiscBoot::Port port{[](){return true;},[](risc_runtime_health_v1*h){h->uptime_ms=(uint32_t)milliseconds;return true;},[](uint32_t n){milliseconds+=n;},[](const char*s){fprintf(stderr,"diagnostic: %s\n",s);return true;}};
 RiscTest::FileSources sources({argv[1]}); auto runtime=std::make_unique<RiscBoot::Runtime>(port); assert(sources.bind(*runtime));bool prepared=runtime->prepare(argv[1]);if(!prepared)fprintf(stderr,"prepare: %s\n",runtime->error());assert(prepared);
 bool ok=runtime->run();if(!ok)fprintf(stderr,"run: %s\n",runtime->error());assert(ok&&complete&&frames==3&&!held&&!pending);runtime.reset();assert(starts==stops);puts("Actual Runtime/ProviderGraph canvas grant, three presentations, quiesce/resume and clean return passed");
}
