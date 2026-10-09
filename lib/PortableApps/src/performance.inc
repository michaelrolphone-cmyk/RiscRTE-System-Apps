/* Compiled only by the explicit diagnostic build. Records are bounded Runtime
 * memory writes; this code never calls diagnostic(), health() or yield_ms(). */
#ifdef PORTABLE_PERFORMANCE_TRACE
static const risc_runtime_api_v1 *perf_runtime;
static uint32_t perf_id,perf_frame_id,perf_draw_id,perf_detail,perf_navigation_down;
static uint32_t perf_counts[PORTABLE_PERF_COUNTER_COUNT];
static bool perf_drew,perf_end_frame,perf_frame_end,perf_drawing,perf_handoff;
static uint32_t perf_emit(uint32_t id,uint32_t phase,uint32_t value) {
 return failed?0:risc_perf_trace_v1(perf_runtime,id,phase,value);
}
void portable_perf_event(uint32_t phase,uint32_t value) {(void)perf_emit(0,phase,value);}
void portable_perf_span(uint32_t tag,bool begin) {
 portable_perf_event(begin?RISC_PERF_SPAN_BEGIN:RISC_PERF_SPAN_END,tag);
}
void portable_perf_count(unsigned counter) {
 if(!failed && counter>0 && counter<PORTABLE_PERF_COUNTER_COUNT && perf_counts[counter]<0xffffffu)
  ++perf_counts[counter];
}
static void perf_snapshot(uint32_t id) {
 if(id!=perf_id)return;
 for(unsigned i=1;i<PORTABLE_PERF_COUNTER_COUNT;++i)
  if(perf_counts[i]) (void)perf_emit(id,RISC_PERF_COUNTER,(i<<24)|perf_counts[i]);
}
static void perf_initialize(void) {
 perf_runtime=rt;memset(perf_counts,0,sizeof(perf_counts));
 /* Counter tag 0, value 1 is the app trace schema, not an operation count.
  * ID zero intentionally inherits an outgoing app's pending interaction. */
 perf_id=perf_emit(0,RISC_PERF_COUNTER,1);perf_frame_id=perf_draw_id=0;
 perf_detail=perf_navigation_down=0;perf_drew=perf_drawing=perf_handoff=false;perf_frame_end=false;
 perf_end_frame=perf_id!=0;
}
static void perf_input_begin(uint32_t kind) {
 perf_snapshot(perf_id);
 perf_id=perf_emit(0,RISC_PERF_INTERACTION_BEGIN,kind);
 perf_drew=perf_handoff=perf_end_frame=false;perf_detail=PORTABLE_PERF_TOUCH_SAMPLE;
 /* This timestamp is the first accepted software sample. Physical contact
  * time is unavailable in this API and must not be inferred from it. */
 (void)perf_emit(perf_id,RISC_PERF_COUNTER,PORTABLE_PERF_TOUCH_SAMPLE);
}
static inline void perf_navigation(uint32_t buttons,uint32_t pressed) {
 uint32_t fresh=pressed&~perf_navigation_down;perf_navigation_down=buttons;
 if(fresh)perf_input_begin(3u);
}
void portable_perf_action(uint32_t detail,bool handoff) {
 perf_detail=detail;perf_handoff=handoff;perf_end_frame=!handoff;
 (void)perf_emit(0,RISC_PERF_RECOGNIZED,detail);
 (void)perf_emit(0,RISC_PERF_DISPATCHED,detail);
}
static void perf_draw_begin(void) {
 perf_draw_id=perf_id;perf_drawing=true;
 portable_perf_count(PORTABLE_PERF_RASTER_PASSES);
 if(!perf_drew){(void)perf_emit(perf_id,RISC_PERF_FIRST_DRAW,perf_detail);perf_drew=true;}
 (void)perf_emit(perf_draw_id,RISC_PERF_SPAN_BEGIN,PORTABLE_PERF_SPAN_RASTER);
}
static void perf_draw_end(void) {
 if(perf_drawing && perf_draw_id==perf_id)
  (void)perf_emit(perf_draw_id,RISC_PERF_SPAN_END,PORTABLE_PERF_SPAN_RASTER);
 perf_drawing=false;
}
static void perf_submit(void) {
 perf_frame_id=perf_draw_id;perf_frame_end=perf_end_frame && !perf_handoff;
 if(perf_frame_id==perf_id)(void)perf_emit(perf_frame_id,RISC_PERF_PRESENT_SUBMIT,perf_detail);
}
static void perf_complete(bool submitted) {
 /* An older frame can finish after a new touch began. Never attach it to the
  * new interaction by passing inherited zero. Explicit stale IDs are rejected. */
 if(perf_frame_id!=perf_id)return;
 perf_snapshot(perf_frame_id);
 (void)perf_emit(perf_frame_id,RISC_PERF_COMPLETE,submitted?1u:0u);
 if(perf_frame_end && perf_frame_id){
  (void)perf_emit(perf_frame_id,RISC_PERF_INTERACTION_END,perf_detail);
  perf_id=0;perf_end_frame=perf_frame_end=false;
 }
}
#ifdef PORTABLE_PERFORMANCE_DISPLAY_METRICS
static void perf_metric(unsigned tag,uint64_t value) {
 (void)perf_emit(perf_frame_id,RISC_PERF_COUNTER,(tag<<24)|(value>0xffffffu?0xffffffu:(uint32_t)value));
}
static void perf_metrics(risc_display_present_token_v1 token) {
 if(failed || perf_frame_id!=perf_id)return;
 const risc_display_output_api_v1_metrics *metrics=risc_display_output_metrics(display);
 if(!metrics)return;
 risc_display_present_metrics_v1 value={.api_version=1,.struct_size=sizeof(value)};
 if(!metrics->snapshot(display->context,&value) || value.api_version!=1 ||
    value.struct_size<sizeof(value) || value.token!=token)return;
 perf_metric(16,value.bytes_sent);perf_metric(17,value.gpio_write_calls);
 perf_metric(20,(uint64_t)value.effective_update.width*value.effective_update.height);
 perf_metric(21,value.mode);perf_metric(25,value.valid_times);
 const unsigned transfer=RISC_DISPLAY_METRICS_TRANSFER_START|RISC_DISPLAY_METRICS_TRANSFER_END;
 const unsigned total=RISC_DISPLAY_METRICS_QUEUED|RISC_DISPLAY_METRICS_BUSY_DONE;
 const unsigned busy=RISC_DISPLAY_METRICS_BUSY_ASSERT|RISC_DISPLAY_METRICS_BUSY_DONE;
 const unsigned assert_time=RISC_DISPLAY_METRICS_REFRESH|RISC_DISPLAY_METRICS_BUSY_ASSERT;
 const unsigned queued=RISC_DISPLAY_METRICS_QUEUED|RISC_DISPLAY_METRICS_TRANSFER_START;
 if((value.valid_times&transfer)==transfer && value.transfer_end_ms>=value.transfer_start_ms)
  perf_metric(18,value.transfer_end_ms-value.transfer_start_ms);
 if((value.valid_times&total)==total && value.busy_done_ms>=value.queued_ms)
  perf_metric(19,value.busy_done_ms-value.queued_ms);
 if((value.valid_times&busy)==busy && value.busy_done_ms>=value.busy_assert_ms)
  perf_metric(22,value.busy_done_ms-value.busy_assert_ms);
 if((value.valid_times&assert_time)==assert_time && value.busy_assert_ms>=value.refresh_ms)
  perf_metric(23,value.busy_assert_ms-value.refresh_ms);
 if((value.valid_times&queued)==queued && value.transfer_start_ms>=value.queued_ms)
  perf_metric(24,value.transfer_start_ms-value.queued_ms);
}
#else
#define perf_metrics(token) ((void)0)
#endif
static void perf_unchanged(void) {
 perf_frame_id=perf_draw_id;perf_frame_end=perf_end_frame && !perf_handoff;perf_complete(false);
}
#else
#define perf_initialize() ((void)0)
#define perf_input_begin(kind) ((void)(kind))
#define perf_navigation(buttons,pressed) ((void)0)
#define perf_draw_begin() ((void)0)
#define perf_draw_end() ((void)0)
#define perf_submit() ((void)0)
#define perf_complete(submitted) ((void)0)
#define perf_metrics(token) ((void)0)
#define perf_unchanged() ((void)0)
#endif
