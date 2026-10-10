#define SPARSE_FIXTURE_MAIN sparse_fixture_main
#include "sparse_clock_startup_test.c"
#undef SPARSE_FIXTURE_MAIN
#include "PortablePerformance.h"
static unsigned record_count;
static struct {uint32_t phase,value;} boot_records[512];
static uint32_t boot_trace(uint32_t id,uint32_t phase,uint32_t value) {
 assert(!terminal);(void)id;
 assert(record_count<512);boot_records[record_count].phase=phase;boot_records[record_count++].value=value;
 return 0;
}
static unsigned stage(uint32_t phase,uint32_t tag) {
 for(unsigned n=0;n<record_count;++n)if(boot_records[n].phase==phase && boot_records[n].value==tag)return n+1;
 return 0;
}
int main(int argc,char **argv) {
 runtime.trace=boot_trace;int result=sparse_fixture_main(argc,argv);assert(!result);
 if(which("cold") || which("gpio")) {
  const uint32_t tags[]={PORTABLE_PERF_SPAN_CLASSIFICATION,PORTABLE_PERF_SPAN_PROMOTION,
   PORTABLE_PERF_SPAN_CONFIG,PORTABLE_PERF_SPAN_LOGO,PORTABLE_PERF_SPAN_RECOVERY};
  unsigned previous=0;
  for(unsigned n=0;n<sizeof(tags)/sizeof(tags[0]);++n) {
   unsigned begin=stage(RISC_PERF_SPAN_BEGIN,tags[n]),end=stage(RISC_PERF_SPAN_END,tags[n]);
   assert(begin>previous && end>begin);previous=end;
  }
  /* RTC duration starts after the entire logo stage, not from cumulative boot. */
  assert(stage(RISC_PERF_SPAN_END,PORTABLE_PERF_SPAN_LOGO)<stage(RISC_PERF_SPAN_BEGIN,PORTABLE_PERF_SPAN_RECOVERY));
 }
 if(terminal){unsigned before=record_count;portable_perf_span(999,true);assert(record_count==before);}
 fprintf(stderr,"Sparse Clock trace %s: %u records; separate boot spans/custody pass\n",test,record_count);
 return 0;
}
