/* Actual Home rules loop: consume/evaluate, ascending fields, partial reports,
 * no editor mask, and workflow claiming after preference completion. */
#define PORTABLE_NATIVE_TIME_TOOLBAR
#define PORTABLE_RESIDENT_SHELL_HOST
#define main policy_fixture_main
#include "home_automatic_test.c"
#undef main
#include "PortableApps.h"
static portable_contexts_client contexts_client;
static const risc_battery_gauge_api_v1 *gauge;
static unsigned now=1000,rule_reports,workflow_claims,workflow_runs;
static uint32_t reported[CR_RULES];
static bool evaluated;
static unsigned action_kind;
static uint32_t millis_now(void){return now;}
bool portable_app_native_local_time(twatch_rtc_time_v1 *out){(void)out;return false;}
static const alarm_service_v1 *alarm_sleep_api(void){return NULL;}
static bool contexts_face_save(const risc_key_value_v1 *view,unsigned id){(void)view;(void)id;return false;}
const t5_app_manifest_t portable_catalog[]={{.compatible=true,.file_name="test.elf"}};
const unsigned portable_catalog_count=1;
static bool app_allows_launch(const char *path){return !strcmp(path,"test.elf");}
static bool resident_queue_launch(const char *path){assert(!strcmp(path,"test.elf"));return true;}
static bool display_settled=true;
static struct {bool frame;} surface;
static bool paper_present_progress(void){return false;}
static void clear(void){}
static void label(int x,int y,int w,const char *text){(void)x;(void)y;(void)w;(void)text;}
static int width(void){return 480;}
static int height(void){return 800;}
static void present(bool full){(void)full;}
static bool alarm_foreground(bool *consumed){*consumed=false;return true;}
static void input_navigation_take(t5_app_input_t *out){(void)out;}
#include "PortableTouch.h"
static void input_take(portable_touch_sample *out){(void)out;}
#include "../../lib/PortableApps/src/contexts_rules_adapter.inc"
static bool fingerprint(void *unused,uint32_t op,void *request){
 (void)unused;io();
 if(op==CONTEXTS_FP_STATUS)return true;
 if(op==CONTEXTS_FP_CONFIG)return true;
 if(op==CONTEXTS_FP_EVALUATE){
  contexts_evaluate_v1 *q=request;assert(q->observation.enabled);
  if(evaluated)return true;evaluated=true;q->result.changed=(1u<<CR_SETTINGS)-1u;
  int32_t values[CR_SETTINGS]={2,0,30,600,1,1,7,5,2,1};
  for(unsigned f=0;f<CR_SETTINGS;++f){q->result.settings[f]=values[f];q->result.winners[f]=(int8_t)(f%2);}
  return true;
 }
 assert(op==CONTEXTS_FP_WORKFLOW);contexts_workflow_v1 *q=request;
 if(q->claim){
  ++workflow_claims;
  if(action_kind&&!workflow_runs){q->available=true;q->step.kind=action_kind;strcpy(q->step.text,action_kind==CR_MESSAGE?"Hello":"test.elf");q->step.value=20;}
 }else if(q->result==CR_APPLIED||q->result==CR_PARTIAL){assert(q->rule<2);reported[q->rule]=q->result;++rule_reports;}
 else {++workflow_runs;assert(q->result==(action_kind==CR_TIMER?CR_FAILED:CR_RAN));}
 return true;
}
static const contexts_fingerprint_service_v1 fp={.base={.api_version=1,.struct_size=sizeof(fp),.pause=pause_service},.fingerprint_abi=0x31504643u,.fingerprint=fingerprint};
static bool rule_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out){
 if(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)){io();assert(version==1&&!id);return false;}
 return acquire(name,version,id,out);
}
static const risc_runtime_api_v1 rule_runtime={.api_version=1,.struct_size=sizeof(rule_runtime),.acquire=rule_acquire,.release=release,.yield_ms=yield};
int main(int argc,char **argv){
 assert(argc==2);test=argv[1];rt=&rule_runtime;preset_mask=0x2a;alert_allowed=false;
 contexts_client=(portable_contexts_client){.runtime=rt,.api=&fp.base,.rules_available=true,.settings_valid=true,.policy={.enabled=true}};
 if(is("rule-loop-partial")){fault_key="sleep_mode";fault_status=RISC_ATOMIC_IO;}
 if(is("rule-loop-low"))contexts_client.low_battery=true;
 if(is("rule-loop-message"))action_kind=CR_MESSAGE;
 if(is("rule-loop-launch"))action_kind=CR_LAUNCH;
 if(is("rule-loop-timer"))action_kind=CR_TIMER;
 assert(contexts_rules_tick());assert(rule_reports==2&&workflow_claims==1);
 if(is("rule-loop-low"))assert(count==6&&reported[0]==CR_PARTIAL&&reported[1]==CR_PARTIAL);
 else {assert(count==10&&reported[0]==CR_PARTIAL&&reported[1]==(is("rule-loop-partial")?CR_PARTIAL:CR_APPLIED));assert(!strcmp(order[0],"sleep_mode")&&!strcmp(order[2],"sleep_deep")&&!strcmp(order[6],"alarm_volume"));}
 if(action_kind)assert(workflow_runs==1);
 if(action_kind==CR_MESSAGE)assert(context_message_open&&!strcmp(context_message,"Hello"));
 unsigned saved=count;now+=600;assert(contexts_rules_tick()&&count==saved&&rule_reports==2);
 assert(!legacy_writes&&!home_automatic_owned());(void)contexts_message_foreground;
 printf("Home rules controller: %s passed (%u puts; consumed once; reports preserved)\n",test,count);return 0;
}
