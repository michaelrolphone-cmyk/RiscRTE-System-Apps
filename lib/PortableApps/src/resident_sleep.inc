/* The host owns the temporary image; the product helper owns every sleep and
 * cleanup operation. It never calls restore after uncertain native custody. */
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
 if(!snapshot->copy_completed(display->context,surface_format,alarm_pixels,
      quick_frame_bytes(),(uint32_t)quick_row_bytes()))return PORTABLE_IDLE_UI_REFUSED;
#ifdef PORTABLE_PAPER_CROSSFADE
 portable_paper_transition_cancel();
#endif
 alarm_pixels_valid=true;alarm_modal=true;
 paper_previous_valid=previous_valid=false;
 resident_sleep_present=true;
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
 resident_sleep_present=false;
 if(!settled) {
  if(!native_custody_retained)portable_adapter_retain_silent();
  return PORTABLE_IDLE_UI_RETAINED;
 }
 return PORTABLE_IDLE_UI_READY;
}
static portable_idle_ui_result resident_sleep_restore(void *context) {
 (void)context;
 if(native_custody_retained || failed)return PORTABLE_IDLE_UI_RETAINED;
 if(!alarm_modal || !alarm_pixels_valid)return PORTABLE_IDLE_UI_RETAINED;
 resident_sleep_present=true;
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
