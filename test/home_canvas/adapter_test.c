#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_provider.h"
#include "PortableTouch.h"
#include "PortableHomeCanvas.h"
#include "PortableNavigation.h"
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_QUICK_ACTIONS
static const risc_runtime_api_v1 *rt;
static risc_runtime_capability_v1 dg;
static const risc_display_output_api_v1 *display;
static risc_display_info_v1 info;
static risc_display_surface_v1 surface;
static uint32_t surface_format;
static bool paper_rotated,native_custody_retained,shared_canvas_controls,paper_orientation_dirty;
static uint64_t paper_token;
static uint16_t *alarm_pixels,*previous_pixels,*quick_background;
static uint8_t *paper_previous;
static bool alarm_pixels_valid,previous_valid,paper_previous_valid;
static unsigned brightness_calls,grant_releases;
static void portable_adapter_retain_silent(void){native_custody_retained=true;}
#include "../../lib/PortableApps/src/shared_home_canvas_adapter.inc"
static canvas_provider provider;
static unsigned physical_w=13,physical_h=21,physical_format;
static unsigned char pixels[1024];static uint64_t lease,token;
static bool phys_info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=physical_w,.height=physical_h,.supported_formats=RISC_DISPLAY_FORMAT_BIT(physical_format)};return true;}
static bool phys_acquire(void*c,uint32_t format,risc_display_surface_v1*out){(void)c;assert(!lease&&format==physical_format);unsigned stride=format==RISC_DISPLAY_FORMAT_MONO1?(physical_w+7)/8:physical_w*2;lease=7;*out=(risc_display_surface_v1){lease,pixels,physical_w,physical_h,stride,stride*physical_h,format};return true;}
static void phys_release(void*c,uint64_t id){(void)c;assert(id==lease);lease=0;}
static bool phys_submit(void*c,uint64_t id,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*out){(void)c;(void)d;(void)n;(void)o;assert(id==lease);lease=0;*out=++token;return true;}
static bool phys_status(void*c,uint64_t id,risc_display_present_status_v1*out){(void)c;assert(id==token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool phys_brightness(void*c,uint16_t v,uint16_t m){(void)c;assert(v==43&&m==100);brightness_calls++;return true;}
static const risc_display_output_api_v1 physical={1,sizeof(physical),NULL,phys_info,phys_acquire,phys_release,phys_submit,phys_status,NULL,phys_brightness};
static bool acquire_cap(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*out){assert(v==1&&!i);out->slot=1;out->generation=1;if(!strcmp(n,"display.output"))out->api=&physical;else{assert(!strcmp(n,RISC_CANVAS_CAPABILITY));out->api=&provider.api;}return true;}
static bool release_cap(risc_runtime_capability_v1*g){assert(g->api);grant_releases++;memset(g,0,sizeof(*g));return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire_cap,.release=release_cap};
static void run(unsigned format,unsigned rotation){
 physical_format=format;lease=token=0;native_custody_retained=false;brightness_calls=grant_releases=0;surface=(risc_display_surface_v1){0};info=(risc_display_info_v1){0};dg=(risc_runtime_capability_v1){.struct_size=sizeof(dg)};rt=&runtime;
 canvas_provider_init(&provider,&physical,NULL,rotation);
 assert(shared_canvas_open()&&surface_format==RISC_DISPLAY_FORMAT_MONO1&&!paper_rotated);
 assert(info.width==(rotation&1?physical_h:physical_w)&&info.height==(rotation&1?physical_w:physical_h));
 assert(display->set_brightness(display->context,43,100)&&brightness_calls==1);
 portable_touch touch;assert(shared_canvas_touch_open(&touch,rt));
 const risc_input_navigation_api_v1*n=shared_canvas_navigation_open();assert(n->foreground(n->context,NULL,0)&&n->reset(n->context));
 shared_canvas_touch.navigation_events[0]=(risc_canvas_event_v1){.kind=RISC_CANVAS_NAVIGATION,.navigation={.buttons=RISC_NAV_DOWN,.pressed=RISC_NAV_DOWN}};
 shared_canvas_touch.navigation_events[1]=(risc_canvas_event_v1){.kind=RISC_CANVAS_NAVIGATION,.navigation={.released=RISC_NAV_DOWN}};
 shared_canvas_touch.navigation_events[2]=(risc_canvas_event_v1){.kind=RISC_CANVAS_CONTROLS_REQUEST};
 shared_canvas_touch.navigation_head=0;shared_canvas_touch.navigation_count=3;
 risc_input_navigation_frame_v1 nav;assert(n->poll(NULL,&nav)&&nav.pressed==RISC_NAV_DOWN&&!nav.released);
 assert(n->poll(NULL,&nav)&&!nav.pressed&&nav.released==RISC_NAV_DOWN);
 assert(n->poll(NULL,&nav)&&shared_canvas_controls);assert(n->reset(NULL)&&!shared_canvas_controls);
 assert(display->acquire(display->context,surface_format,&surface));
 memset(surface.pixels,255,surface.size_bytes);uint64_t receipt;
 assert(display->submit(display->context,surface.frame,NULL,0,NULL,&receipt));surface=(risc_display_surface_v1){0};
 risc_display_present_status_v1 status;assert(display->present_status(display->context,receipt,&status)&&status.state==RISC_DISPLAY_PRESENT_COMPLETE);
 assert(shared_canvas_touch_close(&touch,rt)&&!grant_releases);
 alarm_pixels=malloc(64);quick_background=malloc(64);previous_pixels=malloc(64);paper_previous=malloc(64);
 assert(home_canvas_orientation(&shared_canvas,RISC_CANVAS_ORIENTATION_LANDSCAPE,true));
 assert(shared_canvas_geometry_sync()&&info.width>=info.height&&!paper_rotated);
 assert(home_canvas_quiesce(&shared_canvas)&&home_canvas_resume(&shared_canvas));
 assert(shared_canvas_close()&&grant_releases==1);assert(runtime.release(&dg)&&grant_releases==2);
 free(alarm_pixels);alarm_pixels=NULL;free(quick_background);quick_background=NULL;free(previous_pixels);previous_pixels=NULL;free(paper_previous);paper_previous=NULL;
}
int main(void){for(unsigned f=0;f<2;f++)for(unsigned r=0;r<4;r++)run(f?RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1,r);puts("Overview Canvas adapter:8 physicalformat/rotation combinations, logicalMONO, orderednavigation, brightness, checkedresize, orientation, sleep and grantclosure PASS");}
