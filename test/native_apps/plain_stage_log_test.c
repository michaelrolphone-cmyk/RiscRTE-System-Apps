#define PAPER_QUICK_MAIN unused_quick_main
#define PAPER_QUICK_RUNTIME unused_quick_runtime
#include "paper_quick_test.c"
#undef PAPER_QUICK_MAIN
#undef PAPER_QUICK_RUNTIME
static unsigned statements,touch_starts,touch_ends,draw_starts,draw_ends,submits,completed,opened,closed;
static bool plain_diagnostic(const char *line){
 assert(!strstr(line,"RTE_PERF") && !strstr(line,"phase="));
 if(!strncmp(line,"APP t_ms=",9)) {
  unsigned long long stamp=0;assert(sscanf(line,"APP t_ms=%llu",&stamp)==1&&stamp==ms);
  assert(strlen(line)<224);statements++;
  touch_starts+=strstr(line,"stage=touch-picked-up")!=NULL;
  touch_ends+=strstr(line,"stage=touch-released")!=NULL;
  draw_starts+=strstr(line,"stage=draw-begin")!=NULL;
  draw_ends+=strstr(line,"stage=draw-end")!=NULL;
  submits+=strstr(line,"stage=display-submit-end")!=NULL;
  completed+=strstr(line,"stage=display-complete")!=NULL;
  opened+=strstr(line,"name=quick-controls-open")!=NULL;
  closed+=strstr(line,"name=quick-controls-close")!=NULL;
 }
 return true;
}
static const risc_runtime_api_v1 plain_runtime={.api_version=1,.struct_size=sizeof(plain_runtime),.health=qa_health,.yield_ms=yield_ms,.diagnostic=plain_diagnostic,.request_launch=launch_app,.acquire=qa_acquire,.release=release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1?&plain_runtime:NULL;}
int main(void){
 qa_case=0;assert(app_module_init()==0);app_main();app_module_fini();
 assert(!frames&&!grants&&!subs&&!launches);
 assert(statements>10&&touch_starts==2&&touch_ends==2&&touch_starts<polls/4);
 assert(draw_starts==draw_ends&&draw_starts>=3&&submits==completed&&completed==presents);
 assert(opened==1&&closed==1);
 printf("Direct stage logs: statements=%u touch_down/up=%u/%u polls=%u frames=%u; no command, phase IDs or per-move log\n",statements,touch_starts,touch_ends,polls,presents);
 return 0;
}
