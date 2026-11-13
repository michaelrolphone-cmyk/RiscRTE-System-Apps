#ifdef PORTABLE_BLE_BROADCAST
  if(!portable_broadcast_stop())return -1;
#endif
/* Included at the legacy init point or explicit native Settings app_main start. */
  if (!rt->acquire("display.output", 1, 0, &dg))
    return -1;
  display = dg.api;
  if (!display || display->api_version != 1 ||
      display->struct_size < sizeof(*display) || !display->get_info ||
      !display->acquire || !display->release || !display->submit ||
      !display->present_status)
    return -1;
  if (!display->get_info(display->context, &info) || info.width < 160 ||
      info.height < 160 || info.width > 1024 || info.height > 1024 ||
      !(info.supported_formats & (RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)|
                                 RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1))))
    return -1;
#ifdef PORTABLE_WATCH_COLOR_ONLY
  if(!(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)))return -1;
#ifndef PORTABLE_WATCH_NATIVE_HEIGHT
  if(info.width!=240 || info.height!=240)return -1;
#endif
#endif
#ifdef PORTABLE_USB_TRANSFER_APP
  /* A blocking panel wait would starve the owner-task USB service. */
  if(!(info.flags&RISC_DISPLAY_INFO_ASYNC_PRESENT))return -1;
#endif
#ifdef PORTABLE_SAFE_VIEWPORT
  if(!portable_safe_viewport_open(&content_view,info.width,info.height,info.safe_insets.left,info.safe_insets.top,info.safe_insets.right,info.safe_insets.bottom))return -1;
#ifdef PORTABLE_SETTINGS_ROUND_GEOMETRY
  /* Safe insets remain authoritative for all non-round/non-opt-in clients. */
  if((info.flags&RISC_DISPLAY_INFO_ROUND) && (info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)))
    content_view=(portable_safe_viewport){0,0,info.width,info.height};
#endif
#ifdef PORTABLE_TIMECARD_NATIVE_UI
  /* Native Timecard derives its controls from this safe content rectangle.
   * Its controller admits 160 pixels; legacy 240-row consumers keep their
   * existing minimum below. */
  if(content_view.width<160 || content_view.height<160)return -1;
#else
  if(content_view.width<240 || content_view.height<240)return -1;
#endif
#endif
  surface_format=(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565))?
    RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1;
  paper_rotated=PORTABLE_DISPLAY_ROTATION==90 && surface_format==RISC_DISPLAY_FORMAT_MONO1 &&
    (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) && info.width>info.height;
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && !pp_enabled())return -1;
#ifdef PORTABLE_PAPER_PREFERENCES
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && pp_enabled()) {
    paper_preferences_grant=(risc_runtime_capability_v1){.struct_size=sizeof(paper_preferences_grant)};unsigned value=0;
    int preference_status=PORTABLE_READER_UNAVAILABLE;
    if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_READER_STORE_INSTANCE,&paper_preferences_grant)) {
      preference_status=portable_reader_preference_load(paper_preferences_grant.api,false,&value);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
      if(native_custody_retained)return -1;
#endif
      if(!rt->release(&paper_preferences_grant)) {
        /* Preserve the actual grant and every resource on uncertain cleanup. */
        paper_preferences_retained=failed=true;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
        portable_adapter_retain();
#endif
#ifdef PORTABLE_ALARM_CLIENT
        native_sleep_retained=true;
#endif
        return -1;
      }
    }
    paper_preferences_pending=preference_status==PORTABLE_READER_UNAVAILABLE;
    paper_preferences_retry_at=millis_now()+250u;
    if(!paper_preferences_pending)paper_flip_ui=value!=0;
  }
#endif
#if defined(PORTABLE_RF_NATIVE_CANVAS)
  /* RF paints native pixels on every admitted color aspect ratio. Its editor
   * layout has its own bounded viewport and does not require 240 logical rows. */
  if(!pp_enabled() && surface_format!=RISC_DISPLAY_FORMAT_RGB565)return -1;
#elif defined(PORTABLE_WATCH_NATIVE_HEIGHT)
  if(surface_format!=RISC_DISPLAY_FORMAT_RGB565 || !nu_layout_ready() || nu_height()<240)return -1;
#elif defined(PORTABLE_NOVA_UI)
  if(!pp_enabled() && (info.width!=240 || info.height!=240))return -1;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(native_custody_retained)return -1;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  alarm_pixels=malloc((size_t)(surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2)*info.height);
  if(!alarm_pixels || !portable_alarm_open(&alarms,rt))return -1;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if (!portable_touch_open(&touch, rt)) {portable_adapter_retain();return -1;}
#else
  if (!portable_touch_open(&touch, rt))return -1;
#endif
#ifndef PORTABLE_FORCE_FULL_FRAMES
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && (info.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE))
    paper_previous=malloc((size_t)((info.width+7)/8)*info.height); /* optional, 48 KB at 800x480 */
  if(surface_format==RISC_DISPLAY_FORMAT_RGB565 && (info.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE) && info.width<=320 && info.height<=320)
    previous_pixels=malloc((size_t)info.width*info.height*2); /* optional; full-frame fallback */
#endif
#if defined(PORTABLE_INPUT_NAVIGATION) && !defined(PORTABLE_SETTINGS_APP)
  if(!input_navigation_open())return -1;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(native_custody_retained)return -1;
#endif
/* Native-only Settings has no battery requirement. Avoid an undeclared
   * external acquisition: its strict custody wrapper cannot prove rollback
   * from a boolean failure. Legacy battery-selected profiles are unchanged. */
#ifndef PORTABLE_SETTINGS_NATIVE_ONLY
  if (rt->acquire("board.battery", 1, 0, &bg)) {
    gauge = bg.api;
    if (!gauge || gauge->api_version != 1 ||
        gauge->struct_size < sizeof(*gauge) || !gauge->read)
      return -1;
  }
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(native_custody_retained)return -1;
#endif
#ifdef PORTABLE_SETTINGS_APP
  if (!settings_open()) return -1;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(native_custody_retained)return -1;
#endif
#endif
#ifdef PORTABLE_APP_SLEEP_LOCAL
  last_activity=millis_now();
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  quick.ui.paper=pp_enabled();
#ifdef PORTABLE_QUICK_NATIVE_GEOMETRY
  if(surface_format!=RISC_DISPLAY_FORMAT_RGB565 || !pqa_set_dimensions(&quick.ui,info.width,info.height))return -1;
#ifdef PORTABLE_QUICK_SAFE_CONTENT
  if(!pqa_set_safe_content(&quick.ui,info.safe_insets.left,info.safe_insets.top,
    info.width-info.safe_insets.left-info.safe_insets.right,
    info.height-info.safe_insets.top-info.safe_insets.bottom))return -1;
#endif
#endif
  if(!pqa_session_load(&quick,rt))return -1;
  initial_brightness_pending=surface_format==RISC_DISPLAY_FORMAT_RGB565 &&
    (info.flags&RISC_DISPLAY_INFO_BRIGHTNESS)!=0;
  quick_paper_capabilities();
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(quick.ui.paper && !pqa_session_sync_tone(&quick,display)){
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
   /* A refused provider operation may already own terminal custody. */
   portable_adapter_retain_silent();
#else
   portable_adapter_retain();
#endif
   return -1;}
#endif
#ifdef PORTABLE_QUICK_RADIOS
#ifdef PORTABLE_DESK_CLOCK
  desk_radios_loaded=false;
#else
  if(!pqa_radios_load(&quick_radios,&quick.ui,rt))return -1;
#endif
#endif
#endif
#if defined(PORTABLE_CONTEXTS_CLIENT) && defined(PORTABLE_RESIDENT_SHELL_CLIENT)
  if(!contexts_controls_load())return -1;
#endif
#ifdef PORTABLE_FOREGROUND_IDLE
  foreground_idle_configured=foreground_idle_retry_pending=false;
  foreground_idle_retry_delay=0;
  if(!foreground_idle_configure_once(millis_now()))return -1;
#endif
  return failed?-1:0;
