/* Optional bounded colour-display mailbox. Rendering owns one private canvas;
 * only a sealed complete image may be copied into a short provider lease.
 * There is at most one submitted token and one replaceable latest image. */
#ifdef PORTABLE_ASYNC_RGB565
static uint16_t *canvas_pixels;
static bool canvas_async,canvas_sealed,canvas_uncertain,canvas_retained;
static risc_display_present_token_v1 canvas_token;
static uint32_t canvas_submitted_at,canvas_pumped_at;
static bool canvas_pumping;
#define CANVAS_PUMP_INTERVAL_MS 4u
#define CANVAS_DRAWING_FRAME UINT64_MAX
static bool canvas_retain(void) {
 failed=true;canvas_uncertain=true;canvas_sealed=false;
 if(canvas_retained)return false;
 if(rt->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE && rt->retain_invocation && rt->retain_invocation()) {
  canvas_retained=true;
#ifdef PORTABLE_ALARM_CLIENT
  native_sleep_retained=true;
#endif
  return false;
 }
 /* Async mode requires this suffix. If a broken owner contract refuses it,
  * never call an ordinary yield: that could poll the uncertain provider. */
 for(;;){}
}
static bool canvas_surface_valid(const risc_display_surface_v1 *s) {
 return s->frame && s->pixels && s->width==info.width && s->height==info.height &&
  s->pixel_format==RISC_DISPLAY_FORMAT_RGB565 && s->stride_bytes>=info.width*2u &&
  s->stride_bytes<=UINT32_MAX/info.height && s->size_bytes>=s->stride_bytes*info.height;
}
static bool canvas_token_status(void) {
 if(!canvas_token)return true;
 risc_display_present_status_v1 status={0};
 if(!display->present_status(display->context,canvas_token,&status))failed=true;
 else if(status.state==RISC_DISPLAY_PRESENT_COMPLETE) {
  canvas_token=0;previous_valid=!failed && previous_pixels;
 } else if(status.state!=RISC_DISPLAY_PRESENT_QUEUED && status.state!=RISC_DISPLAY_PRESENT_ACTIVE)failed=true;
 if(failed && canvas_token)return canvas_retain();
 if(canvas_token && (uint32_t)(millis_now()-canvas_submitted_at)>=10000u)failed=true;
 if(failed && canvas_token)return canvas_retain();
 return !failed;
}
static bool canvas_progress(void) {
 if(!canvas_async)return !failed;
 if(canvas_retained)return false;
 if(canvas_uncertain)return canvas_retain();
 if(!canvas_token_status())return false;
 /* The provider is cooperative: status reads do not transmit rows. Bound
  * raster ownership of the owner task, while never copying an unsealed image.
  * At most one pump is admitted per interval. The guard also fences unexpected
  * callback reentry; Runtime normally rejects recursive owner-task yields. */
 if(canvas_token && surface.frame==CANVAS_DRAWING_FRAME && !canvas_pumping &&
    (uint32_t)(millis_now()-canvas_pumped_at)>=CANVAS_PUMP_INTERVAL_MS) {
  canvas_pumping=true;
  rt->yield_ms(1);
  canvas_pumping=false;
  /* A yield may have failed the provider. Validate before any normal callback,
   * touching provider storage, or asking Runtime for a fresh timestamp. */
  if(!canvas_token_status())return false;
  canvas_pumped_at=millis_now();
 }
 if(failed){canvas_sealed=false;if(canvas_token)return canvas_retain();return false;}
 /* No progress callback can copy a partly painted canvas. */
 if(!canvas_token && canvas_sealed && !surface.frame) {
  risc_display_rect_v1 damage={0};size_t count=0;
  if(previous_pixels && previous_valid) {
   unsigned first=info.height,last=0;
   for(unsigned y=0;y<info.height;y++)if(memcmp(previous_pixels+(size_t)y*info.width,
       canvas_pixels+(size_t)y*info.width,info.width*2u)) {
    if(first==info.height)first=y;
    last=y+1;
   }
   if(first==info.height){canvas_sealed=false;goto settled;}
   damage=(risc_display_rect_v1){0,(int32_t)first,info.width,last-first};count=1;
  }
  risc_display_surface_v1 lease={0};
  if(!display->acquire(display->context,RISC_DISPLAY_FORMAT_RGB565,&lease)) {failed=true;return false;}
  if(!canvas_surface_valid(&lease)) {
   if(lease.frame)display->release(display->context,lease.frame);
   failed=true;return false;
  }
  for(unsigned y=0;y<info.height;y++)memcpy((uint8_t*)lease.pixels+(size_t)y*lease.stride_bytes,
    canvas_pixels+(size_t)y*info.width,info.width*2u);
  const risc_display_present_options_v1 options={RISC_DISPLAY_PRESENT_DEFAULT,RISC_DISPLAY_QUEUE_FIFO,0};
  if(!display->submit(display->context,lease.frame,count?&damage:NULL,count,&options,&canvas_token)) {
   display->release(display->context,lease.frame);failed=true;return false;
  }
  /* Successful submission transfers the real lease to the provider. */
  canvas_sealed=false;canvas_submitted_at=canvas_pumped_at=millis_now();previous_valid=false;
  if(!canvas_token)return canvas_retain();
  if(previous_pixels)memcpy(previous_pixels,canvas_pixels,(size_t)info.width*info.height*2u);
 }
settled:
#ifdef PORTABLE_ALARM_CLIENT
 display_settled=!canvas_token && !canvas_sealed;
#endif
 return !failed;
}
static bool canvas_begin(void) {
 if(!canvas_async)return false;
 /* clear() deliberately replaces an unsubmitted older image; no FIFO grows. */
 canvas_sealed=false;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
 if(handoff_pending) {
  risc_display_surface_v1 lease={0};
  if(canvas_token || !display->acquire(display->context,RISC_DISPLAY_FORMAT_RGB565,&lease)) {failed=true;return true;}
  if(!canvas_surface_valid(&lease))failed=true;
  else for(unsigned y=0;y<info.height;y++)memcpy(canvas_pixels+(size_t)y*info.width,
    (uint8_t*)lease.pixels+(size_t)y*lease.stride_bytes,info.width*2u);
  if(lease.frame)display->release(display->context,lease.frame);
  if(failed)return true;
 }
#endif
 surface=(risc_display_surface_v1){.frame=CANVAS_DRAWING_FRAME,.pixels=canvas_pixels,
  .width=info.width,.height=info.height,.stride_bytes=info.width*2u,
  .size_bytes=info.width*info.height*2u,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 return true;
}
static void canvas_present(void) {
#ifdef PORTABLE_ALARM_CLIENT
 /* This is the newest complete logical image, independent of panel latency.
  * Alarm and Controls underlays never point into a provider lease. */
 if(!alarm_modal
#ifdef PORTABLE_QUICK_ACTIONS
    && !quick_modal
#endif
 ) {
  memcpy(alarm_pixels,canvas_pixels,(size_t)info.width*info.height*2u);
  alarm_pixels_valid=true;
 }
#endif
 surface.frame=0;canvas_sealed=true;(void)canvas_progress();
}
static bool canvas_drain(void) {
 if(!canvas_async)return !failed;
 if(surface.frame==CANVAS_DRAWING_FRAME)surface.frame=0;
 for(unsigned n=0;n<10000u;n++) {
  (void)canvas_progress();
  if(!canvas_token && !canvas_sealed)return !failed && !canvas_uncertain;
  if(failed)return false;
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_capture_checkpoint())return false;
#endif
  rt->yield_ms(1);
 }
 return canvas_retain();
}
#ifdef PORTABLE_APP_SLEEP_LOCAL
/* Once idle is due, finish this bounded ownership transition instead of
 * chasing a continually replaced animation. Fresh input cancels sleep. */
static int canvas_sleep_barrier(void) {
 for(unsigned n=0;n<10000u;n++) {
  if(!canvas_progress())return -1;
  if(risc_runtime_exit_requested_v1(rt))return 0;
  input_service();
  if(failed)return -1;
  if((uint32_t)(millis_now()-last_activity)<portable_idle_ms() || navigation_pending ||
     (input_pending && (input_sample.down || input_sample.released)))return 0;
  if(!canvas_token && !canvas_sealed)return 1;
  rt->yield_ms(1);
 }
 (void)canvas_retain();return -1;
}
#endif
static bool canvas_exit_barrier(void) {
 if(canvas_drain())return true;
 if(canvas_token || canvas_uncertain)return canvas_retain();
 return false;
}
#else
static inline bool canvas_exit_barrier(void) {return !failed;}
static bool canvas_progress(void) {return !failed;}
#endif
#ifdef PORTABLE_ALARM_CLIENT
static bool foreground_frame_ready(void) {
 if(failed || surface.frame)return false;
#ifdef PORTABLE_ASYNC_RGB565
 if(canvas_async)return true;
#endif
 return display_settled;
}
#endif
