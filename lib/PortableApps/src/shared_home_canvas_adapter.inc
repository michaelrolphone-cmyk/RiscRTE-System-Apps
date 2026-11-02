/* Opt-in common Home logical surface. Physical output is retained only for
 * brightness/tone; its geometry, pixel format and rotation are never read. */
#include "PortableHomeCanvas.h"
#include "PortableHomeCanvasTouch.h"
#include "RiscCanvasInputV1.h"
static home_canvas_surface shared_canvas;
static uint32_t shared_canvas_orientation=RISC_CANVAS_ORIENTATION_DEFAULT;
static home_canvas_touch shared_canvas_touch;
static risc_runtime_capability_v1 shared_canvas_grant,shared_canvas_input_grant;
static const risc_canvas_input_v1 *shared_canvas_input;
static bool shared_canvas_input_active,shared_canvas_input_prepared;
static bool shared_canvas_owned,shared_canvas_input_owned,shared_outputs_owned;
static const risc_display_output_api_v1 *shared_outputs;
static void shared_canvas_retained(void *unused){(void)unused;portable_adapter_retain_silent();}
static inline bool shared_canvas_open(void) {
 if(!rt->acquire("display.output",1,0,&dg))return false;
 shared_outputs_owned=true;
 shared_outputs=dg.api;
 if(!shared_outputs||shared_outputs->api_version!=1||shared_outputs->struct_size<sizeof(*shared_outputs))return false;
 shared_canvas_grant=(risc_runtime_capability_v1){.struct_size=sizeof(shared_canvas_grant)};
 if(!rt->acquire(RISC_CANVAS_RASTER_CAPABILITY,1,0,&shared_canvas_grant))return false;
 shared_canvas_owned=true;
 const risc_canvas_api_v1 *api=shared_canvas_grant.api;
 api=api?risc_canvas_get_v1(api,api->struct_size):NULL;
 if(!api)return false;
 if(!home_canvas_open_content_owned(&shared_canvas,api,RISC_DISPLAY_FORMAT_MONO1,true,
     shared_outputs->set_brightness,shared_outputs->context,shared_canvas_retained,NULL))return false;
 display=&shared_canvas.display;surface_format=RISC_DISPLAY_FORMAT_MONO1;paper_rotated=false;
 home_canvas_touch_init(&shared_canvas_touch,&shared_canvas);
 return display->get_info(display->context,&info);
}
static inline bool shared_canvas_close(void) {
 if(shared_canvas.retained||native_custody_retained)return false;
 if(shared_canvas_input_active){
  if(!home_canvas_result(&shared_canvas,shared_canvas_input->deactivate(shared_canvas_input->context,shared_canvas.session))){portable_adapter_retain_silent();return false;}
  shared_canvas_input_active=false;
 }
 if(shared_canvas_input_owned){
  if(!rt->release(&shared_canvas_input_grant)){portable_adapter_retain_silent();return false;}
  shared_canvas_input_owned=false;
 }
 shared_canvas_input=NULL;
 if(shared_canvas.session&&!home_canvas_close(&shared_canvas)){portable_adapter_retain_silent();return false;}
 if(shared_canvas_owned){
  if(!rt->release(&shared_canvas_grant)){portable_adapter_retain_silent();return false;}
  shared_canvas_owned=false;
 }
 if(shared_outputs_owned){
  if(!rt->release(&dg)){portable_adapter_retain_silent();return false;}
  shared_outputs_owned=false;dg=(risc_runtime_capability_v1){.struct_size=sizeof(dg)};
 }
 shared_outputs=NULL;return true;
}
static inline bool shared_canvas_touch_open(portable_touch*t,const risc_runtime_api_v1 *runtime) {
 if(shared_canvas.retained)return false;
 if(!shared_canvas_input_owned){
  shared_canvas_input_grant=(risc_runtime_capability_v1){.struct_size=sizeof(shared_canvas_input_grant)};
  if(!runtime->acquire(RISC_CANVAS_INPUT_CAPABILITY,1,0,&shared_canvas_input_grant)){
   if(shared_canvas_input_grant.struct_size!=sizeof(shared_canvas_input_grant)||shared_canvas_input_grant.api||
      shared_canvas_input_grant.slot||shared_canvas_input_grant.generation)portable_adapter_retain_silent();
   return false;
  }
  shared_canvas_input_owned=true;
  const risc_canvas_input_v1 *api=shared_canvas_input_grant.api;
  shared_canvas_input=api?risc_canvas_input_get_v1(api,api->struct_size):NULL;
  if(!shared_canvas_input)return false;
 }
 if(!shared_canvas_input_active){
  if(!home_canvas_result(&shared_canvas,shared_canvas_input->activate(shared_canvas_input->context,shared_canvas.session)))return false;
  shared_canvas_input_active=true;
 }
 memset(t,0,sizeof(*t));t->api=&shared_canvas_touch.api;
 t->subscription=t->api->subscribe(t->api->context);
 return t->subscription&&portable_touch_sync(t,true);
}
static inline bool shared_canvas_touch_close(portable_touch*t,const risc_runtime_api_v1 *runtime) {
 return portable_touch_close(t,runtime); /* local facade owns no fabricated grant */
}
#define portable_touch_open shared_canvas_touch_open
#define portable_touch_close shared_canvas_touch_close
static inline bool shared_canvas_navigation_poll(void *unused,risc_input_navigation_frame_v1*out) {
 (void)unused;if(shared_canvas.retained)return false;
 bool controls=false;*out=home_canvas_touch_navigation(&shared_canvas_touch,&controls);
 shared_canvas_controls|=controls;return true;
}
static inline bool shared_canvas_navigation_reset(void *unused) {
 (void)unused;if(shared_canvas.retained)return false;
 home_canvas_touch_discard_navigation(&shared_canvas_touch);shared_canvas_controls=false;return true;
}
static inline bool shared_canvas_navigation_foreground(void *unused,const risc_input_foreground_v1 *claims,size_t count) {
 (void)unused;(void)claims;(void)count;return !shared_canvas.retained;
}
static inline const risc_input_navigation_api_v1 *shared_canvas_navigation_open(void) {
 static const risc_input_navigation_api_v1 api={.api_version=1,.struct_size=sizeof(api),.context=NULL,
  .poll=shared_canvas_navigation_poll,.foreground=shared_canvas_navigation_foreground,.reset=shared_canvas_navigation_reset};
 return &api;
}
/* Called only at settled lease boundaries. Resize copied images, never a
 * provider-owned frame; all optional history is invalidated on geometry change. */
static inline bool shared_canvas_geometry_sync(void) {
 if(shared_canvas.retained||surface.frame||paper_token)return false;
 risc_display_info_v1 next={0};
 if(!display->get_info(display->context,&next))return false;
 if(!next.width||!next.height||next.width>UINT16_MAX||next.height>UINT16_MAX)return false;
 if(next.width==info.width&&next.height==info.height){info=next;return true;}
 size_t row=((size_t)next.width+7)/8;
 if(next.height>SIZE_MAX/row)return false;
#ifdef PORTABLE_ALARM_CLIENT
 void *new_alarm=alarm_pixels?malloc(row*next.height):NULL;
 if(alarm_pixels&&!new_alarm)return false;
 free(alarm_pixels);alarm_pixels=new_alarm;alarm_pixels_valid=false;
#endif
 free(previous_pixels);previous_pixels=NULL;previous_valid=false;
 free(paper_previous);paper_previous=NULL;paper_previous_valid=false;
#ifndef PORTABLE_FORCE_FULL_FRAMES
 if(next.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE)paper_previous=malloc(row*next.height);
#endif
#ifdef PORTABLE_QUICK_ACTIONS
 free(quick_background);quick_background=NULL;
#endif
 info=next;paper_rotated=false;paper_orientation_dirty=true;
 return true;
}

/* Generic sleep source calls these only after alarm reconciliation, and resumes
 * all physical resources before the alarm ticket. TIMER has no input grant. */
bool portable_home_canvas_sleep_available(void) {
 return !shared_canvas.retained&&!native_custody_retained&&
  (!shared_canvas_input_active||risc_canvas_input_sleep_supported(shared_canvas_input));
}
int portable_home_canvas_prepare_sleep(void) {
 if(shared_canvas.retained||native_custody_retained)return -2;
 if(!home_canvas_quiesce(&shared_canvas))return shared_canvas.retained?-2:0;
 if(!shared_canvas_input_active)return 1;
 if(!risc_canvas_input_sleep_supported(shared_canvas_input))return 0;
 shared_canvas_input_prepared=true;
 int32_t rc=shared_canvas_input->prepare_sleep(shared_canvas_input->context,shared_canvas.session,1000);
 if(rc==RISC_CANVAS_UNSUPPORTED){shared_canvas_input_prepared=false;return 0;}
 if(!home_canvas_result(&shared_canvas,rc))return shared_canvas.retained?-2:0;
 return 1;
}
int portable_home_canvas_resume_sleep(void) {
 if(shared_canvas.retained||native_custody_retained)return -2;
 if(!shared_canvas_input_prepared)return 1;
 int32_t rc=shared_canvas_input->resume_sleep(shared_canvas_input->context,shared_canvas.session,1000);
 if(!home_canvas_result(&shared_canvas,rc)){portable_adapter_retain_silent();return -2;}
 shared_canvas_input_prepared=false;return 1;
}
