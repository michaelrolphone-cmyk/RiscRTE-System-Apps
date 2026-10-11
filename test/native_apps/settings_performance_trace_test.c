/* Production native Settings entry, controllers, raster and raw touch path. */
#define main settings_fixture_main
#include "portable_native_time_settings_test.c"
#undef main
static unsigned trace_count,trace_begins,trace_next,trace_draws,trace_submits,trace_completed,trace_ends;
static uint32_t trace_id;
static uint32_t settings_trace(uint32_t id,uint32_t phase,uint32_t value) {
 assert(!retained);if(id && id!=trace_id)return 0;
 ++trace_count;
 if(phase==RISC_PERF_INTERACTION_BEGIN){assert(!id);trace_id=++trace_begins;}
 if(phase==RISC_PERF_DISPATCHED && value==PORTABLE_PERF_TIMEZONE_NEXT)++trace_next;
 if(phase==RISC_PERF_FIRST_DRAW)++trace_draws;
 if(phase==RISC_PERF_PRESENT_SUBMIT)++trace_submits;
 if(phase==RISC_PERF_COMPLETE)++trace_completed;
 uint32_t result=trace_id;if(phase==RISC_PERF_INTERACTION_END){++trace_ends;trace_id=0;}
 return result;
}
int main(int argc,char **argv) {
 runtime.trace=settings_trace;
 int result=settings_fixture_main(argc,argv);assert(!result);
 if(timezone_page_case()) {
  assert(trace_begins>=3 && trace_next>=3 && trace_draws && trace_submits && trace_completed);
  assert(timezone_rasters==2 && trace_count<300);
 }
 fprintf(stderr,"Settings trace %s: %u records, %u touch begins, %u Next dispatches, %u completions\n",test_name,trace_count,trace_begins,trace_next,trace_completed);
 return 0;
}
