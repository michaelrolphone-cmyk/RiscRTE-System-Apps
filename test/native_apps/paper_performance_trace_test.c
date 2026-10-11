/* Real Springboard controller + adapter, deterministic async provider. */
#define PAPER_FIXTURE_MAIN paper_fixture_main
#include "paper_present_input_test.c"
#undef PAPER_FIXTURE_MAIN
struct perf_record {uint32_t id,phase,value,ms;};
static struct perf_record records[2048];
static unsigned record_count,rejected;
static uint32_t current_id,next_id;
static bool trace_disabled;
static uint32_t trace_record(uint32_t id,uint32_t phase,uint32_t value) {
 assert(!retained);
 if(trace_disabled)return 0;
 if(phase==RISC_PERF_INTERACTION_BEGIN){assert(!id);current_id=++next_id;}
 else if(id && id!=current_id){++rejected;return 0;}
 assert(record_count<sizeof(records)/sizeof(records[0]));
 records[record_count++]=(struct perf_record){current_id,phase,value,ticks};
 uint32_t result=current_id;
 if(phase==RISC_PERF_INTERACTION_END)current_id=0;
 return result;
}
static const struct perf_record *find_record(uint32_t phase,uint32_t value,uint32_t id) {
 for(unsigned i=0;i<record_count;++i)
  if(records[i].phase==phase && records[i].value==value && records[i].id==id)return &records[i];
 return NULL;
}
int main(int argc,char **argv) {
 assert(argc==3);const char *configuration=argv[2];
 fx_runtime.trace=trace_record;
 if(!strcmp(configuration,"disabled"))trace_disabled=true;
 if(!strcmp(configuration,"old"))fx_runtime.struct_size=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE;
 int result=paper_fixture_main(2,argv);assert(!result);
 if(trace_disabled || !strcmp(configuration,"old")){assert(!record_count);return 0;}
 assert(record_count && !rejected);
 if(!strcmp(argv[1],"launch-feedback") || !strcmp(argv[1],"launch-same")) {
  assert(current_id==1);
  const struct perf_record *begin=find_record(RISC_PERF_INTERACTION_BEGIN,RISC_PERF_KIND_TOUCH_LAUNCH,1);
  const struct perf_record *dispatch=find_record(RISC_PERF_DISPATCHED,PORTABLE_PERF_LAUNCH,1);
  const struct perf_record *draw=find_record(RISC_PERF_FIRST_DRAW,PORTABLE_PERF_LAUNCH,1);
  const struct perf_record *submit=find_record(RISC_PERF_PRESENT_SUBMIT,PORTABLE_PERF_LAUNCH,1);
  const struct perf_record *complete=find_record(RISC_PERF_COMPLETE,1,1);
  assert(begin&&dispatch&&draw&&submit&&complete);
  assert(begin->ms==40 && dispatch->ms==60 && draw->ms==240 && submit->ms==240 && complete->ms==480);
  /* Initial frame completion at240 must never impersonate this new touch. */
  for(unsigned i=0;i<record_count;i++)if(records[i].phase==RISC_PERF_COMPLETE&&records[i].id==1)assert(records[i].ms>=480);
  assert(!find_record(RISC_PERF_INTERACTION_END,PORTABLE_PERF_LAUNCH,1));
  /* A fresh incoming invocation inherits the same Runtime interaction. Its
   * first completed drawing closes correlation, without beginning a new ID. */
  perf_initialize();assert(perf_id==1);perf_draw_begin();perf_draw_end();perf_submit();perf_complete(true);
  assert(!current_id && find_record(RISC_PERF_INTERACTION_END,0,1));
 }
 if(!strcmp(argv[1],"launch-replace"))assert(next_id==2&&current_id==2);
 if(retained){unsigned before=record_count;portable_perf_action(999,false);portable_perf_count(PORTABLE_PERF_INPUT_READS);portable_perf_span(999,true);assert(record_count==before);}
 assert(record_count<300); /* Milestones, no movement/poll event flood. */
 fprintf(stderr,"trace %s: %u records; %u interactions; correct custody/correlation\n",argv[1],record_count,next_id);
 return 0;
}
