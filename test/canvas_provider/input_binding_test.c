#include <assert.h>
#include <stdio.h>
#include "canvas_provider.h"
#include "../../Services/canvas_input/input.c"
static unsigned subscriptions,unsubscriptions,claims,resets;
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=7,.height=11,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static unsigned char pixels[154];
static bool acquire_frame(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;*out=(risc_display_surface_v1){1,pixels,7,11,14,154,f};return true;}
static void release_frame(void*c,uint64_t f){(void)c;(void)f;}
static bool submit_frame(void*c,uint64_t f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)c;(void)f;(void)d;(void)n;(void)o;*t=1;return true;}
static bool status_frame(void*c,uint64_t t,risc_display_present_status_v1*out){(void)c;(void)t;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint64_t subscribe(void*c){(void)c;subscriptions++;return 1;}
static bool unsubscribe(void*c,uint64_t id){(void)c;assert(id==1);unsubscriptions++;return true;}
static bool poll(void*c,size_t n){(void)c;(void)n;return true;}
static int32_t next(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;(void)id;(void)out;return 0;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;*out=(risc_touch_snapshot_v1){.sequence=1,.width=7,.height=11};return true;}
static bool navpoll(void*c,risc_input_navigation_frame_v1*out){(void)c;*out=(risc_input_navigation_frame_v1){0};return true;}
static bool foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;(void)f;(void)n;claims++;return true;}
static bool reset(void*c){(void)c;resets++;return true;}
int main(void){
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,.acquire=acquire_frame,.release=release_frame,.submit=submit_frame,.present_status=status_frame};
 canvas_provider raster;canvas_provider_init(&raster,&d,NULL,0);
 risc_canvas_open_v1 request={sizeof(request),RISC_CANVAS_MONO1,RISC_CANVAS_RESPONSIVE,0,0};uint64_t session;
 assert(!raster.api.open(&raster,&request,&session));
 risc_canvas_frame_v1 frame={.struct_size=sizeof(frame)};assert(!raster.api.begin_frame(&raster,session,&frame));memset(frame.pixels,255,frame.byte_count);uint64_t receipt;assert(!raster.api.submit(&raster,session,frame.lease,&receipt));risc_canvas_receipt_v1 r={.struct_size=sizeof(r)};assert(!raster.api.poll(&raster,session,receipt,&r));assert(!raster.api.forget_receipt(&raster,session,receipt));
 assert(!subscriptions&&!claims&&!resets); /* Complete TIMER raster path. */
 risc_touch_api_v1 t={1,sizeof(t),NULL,subscribe,unsubscribe,poll,next,snapshot};
 risc_input_navigation_api_v1 n={.api_version=1,.struct_size=sizeof(n),.poll=navpoll,.foreground=foreground,.reset=reset};
 risc_provider_dependency_v1 deps[]={{.capability_id=RISC_CANVAS_CAPABILITY,.api_version=1,.api=&raster.api},{.capability_id="input.touch.raw",.api_version=1,.api=&t},{.capability_id="input.navigation",.api_version=1,.api=&n}};
 assert(start(deps,3)&&!subscriptions&&!claims);
 assert(!activate(NULL,session)&&subscriptions==1&&claims==1&&resets==1&&!quiesce());
 assert(!activate(NULL,session)&&subscriptions==1);
 assert(!deactivate(NULL,session)&&unsubscriptions==1&&claims==2&&quiesce()&&!raster.touch&&!raster.navigation);
 stop();assert(!raster.api.close(&raster,session));
 puts("Canvas split input:complete raster path without input callbacks, dependency-bound foreground activation, idempotent ownership and exact deactivation PASS");
}
