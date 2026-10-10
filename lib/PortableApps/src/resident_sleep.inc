/* The host owns the temporary image; the product helper owns every sleep and
 * cleanup operation. It never calls restore after uncertain native custody. */
#ifdef PORTABLE_DISPLAY_SETTLED
#include "RiscDisplayOutputSettledV1.h"
static portable_idle_ui_result resident_sleep_restore(void *context);
static portable_idle_ui_result resident_sleep_wait(void) {
 const risc_display_output_api_v1_settled *settled=risc_display_output_settled(display);
 if(!settled)return PORTABLE_IDLE_UI_READY; /* Legacy provider keeps its contract. */
 if(!resident_sleep_token){portable_adapter_retain_silent();return PORTABLE_IDLE_UI_RETAINED;}
 const risc_display_present_token_v1 token=resident_sleep_token;
 if(!settled->request_settle(display->context,token)) {
  portable_adapter_retain_silent();return PORTABLE_IDLE_UI_RETAINED;
 }
 if(!resident_touch_open() || !resident_focus(true))return PORTABLE_IDLE_UI_RETAINED;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 desk_touch_neutral=true;
#endif
 uint32_t began=millis_now(),last=began;
 const uint32_t activity_before=last_activity;
 bool cancelled=false,complete=false;
 for(unsigned polls=0;polls<10000 && !native_custody_retained && !failed;++polls) {
  int32_t status=settled->settled_status(display->context,token);
  if(native_custody_retained)return PORTABLE_IDLE_UI_RETAINED;
  if(status!=RISC_DISPLAY_SETTLED_PENDING && status!=RISC_DISPLAY_SETTLED_COMPLETE) {
   portable_adapter_retain_silent();return PORTABLE_IDLE_UI_RETAINED;
  }
  uint32_t now=millis_now();
  if(native_custody_retained || failed)return PORTABLE_IDLE_UI_RETAINED;
  if(now<last || now-began>=10000u)break;
  last=now;
  /* The final sample closes the sub-16 ms cancellation gap before sleep. */
  if(status==RISC_DISPLAY_SETTLED_COMPLETE || now-input_sampled_at>=16u)input_service();
  if(native_custody_retained || failed)return PORTABLE_IDLE_UI_RETAINED;
  cancelled=last_activity!=activity_before || home_pending || crown_pending || navigation_pending ||
      (input_pending && input_sample.valid && (input_sample.down || input_sample.began || input_sample.released));
  if(cancelled || status==RISC_DISPLAY_SETTLED_COMPLETE){complete=true;break;}
  rt->yield_ms(1);
 }
 if(!complete){portable_adapter_retain_silent();return PORTABLE_IDLE_UI_RETAINED;}
 bool home=home_pending||crown_pending;
 if(home && resident.request_foreground_exit(resident.invocation)!=RISC_RESIDENT_OK) {
  portable_adapter_retain_silent();return PORTABLE_IDLE_UI_RETAINED;
 }
 if(!resident_focus(false) || !resident_touch_close())return PORTABLE_IDLE_UI_RETAINED;
 if(cancelled) {
  if(resident_sleep_restore(NULL)!=PORTABLE_IDLE_UI_READY)return PORTABLE_IDLE_UI_RETAINED;
  return PORTABLE_IDLE_UI_REFUSED;
 }
 return PORTABLE_IDLE_UI_READY;
}
#endif
static portable_idle_ui_result resident_sleep_begin(void *context) {
 (void)context;
 if(native_custody_retained)return PORTABLE_IDLE_UI_RETAINED;
 if(failed || !resident_child_active || !resident_dispatching || surface.frame ||
    paper_token || !display_settled || alarm_modal || quick_modal || !pp_enabled())
  return PORTABLE_IDLE_UI_REFUSED;
 if(!resident_refresh_preferences())return PORTABLE_IDLE_UI_RETAINED;
 const risc_display_output_api_v1_snapshot *snapshot=risc_display_output_snapshot(display);
 if(!snapshot || !alarm_pixels || surface_format!=RISC_DISPLAY_FORMAT_MONO1)
  return PORTABLE_IDLE_UI_REFUSED;
#ifdef PORTABLE_DISPLAY_SETTLED
 if(display->struct_size>=sizeof(risc_display_output_api_v1_settled) && !risc_display_output_settled(display))
  return PORTABLE_IDLE_UI_REFUSED;
#endif
 if(!snapshot->copy_completed(display->context,surface_format,alarm_pixels,
      quick_frame_bytes(),(uint32_t)quick_row_bytes()))return PORTABLE_IDLE_UI_REFUSED;
#ifdef PORTABLE_PAPER_CROSSFADE
 portable_paper_transition_cancel();
#endif
 alarm_pixels_valid=true;alarm_modal=true;
 paper_previous_valid=previous_valid=false;
 resident_sleep_present=true;
#ifdef PORTABLE_DISPLAY_SETTLED
 resident_sleep_token=0;
#endif
 #ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_immediate())return PORTABLE_IDLE_UI_RETAINED;
#endif
 pp_begin();
 if(failed || native_custody_retained || !surface.frame)return PORTABLE_IDLE_UI_RETAINED;
 for(unsigned y=0;y<info.height;++y)
  memcpy((uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,
         (const uint8_t *)alarm_pixels+(size_t)y*quick_row_bytes(),quick_row_bytes());
 const char *label="Sleeping";
 const int box_width=240,box_height=96,left=(width()-box_width)/2,top=(height()-box_height)/2;
 fill(left,top,box_width,box_height,0);
 fill(left+2,top+2,box_width-4,box_height-4,0xffff);
 pp_text((width()-pp_measure(label,true))/2,(height()-38)/2,box_width-24,label,1,true,true);
 present(true);
 bool settled=!failed && !native_custody_retained && !surface.frame && !paper_token && display_settled;
 if(!settled) {
  if(!native_custody_retained)portable_adapter_retain_silent();
  return PORTABLE_IDLE_UI_RETAINED;
 }
#ifdef PORTABLE_DISPLAY_SETTLED
 portable_idle_ui_result result=resident_sleep_wait();
 if(result!=PORTABLE_IDLE_UI_READY)return result;
#endif
 resident_sleep_present=false;
 return PORTABLE_IDLE_UI_READY;
}
static portable_idle_ui_result resident_sleep_restore(void *context) {
 (void)context;
 if(native_custody_retained || failed)return PORTABLE_IDLE_UI_RETAINED;
 if(!alarm_modal || !alarm_pixels_valid)return PORTABLE_IDLE_UI_RETAINED;
 resident_sleep_present=true;
 #ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_immediate())return PORTABLE_IDLE_UI_RETAINED;
#endif
 pp_begin();
 if(failed || native_custody_retained || !surface.frame)return PORTABLE_IDLE_UI_RETAINED;
 for(unsigned y=0;y<info.height;++y)
  memcpy((uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,
         (const uint8_t *)alarm_pixels+(size_t)y*quick_row_bytes(),quick_row_bytes());
 present(true);
 resident_sleep_present=false;
 if(failed || native_custody_retained || surface.frame || paper_token || !display_settled) {
  if(!native_custody_retained)portable_adapter_retain_silent();
  return PORTABLE_IDLE_UI_RETAINED;
 }
 alarm_modal=false;paper_previous_valid=previous_valid=false;
 return PORTABLE_IDLE_UI_READY;
}
static const portable_idle_sleep_ui resident_sleep_ui={NULL,resident_sleep_begin,resident_sleep_restore};
