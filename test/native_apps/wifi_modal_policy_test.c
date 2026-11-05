/* Actual Wi-Fi controller, adapter and checkpoint client. Deterministic provider
 * and resident dispatch boundaries; no host radio or real credentials. */
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_ALARM_TERMINAL_RETENTION
#define PORTABLE_TEXT_INPUT_POLICY
#define PORTABLE_RESIDENT_SHELL_CLIENT
#define PORTABLE_RESIDENT_POLICY
#define PORTABLE_HOME_APP "default.elf"
#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
static bool test_resident(risc_resident_client_v1 *);
static bool test_exit(void),test_retain(void);
#define TEST_WIFI_RUNTIME_BINDINGS .resident_shell=test_resident,.exit_requested=test_exit,.retain_invocation=test_retain,
#define WIFI_WORKFLOW_LIBRARY
#include "../../Apps/PaperFrame.h"
#ifdef TEST_MODAL_ATOMIC
#define WIFI_AVAILABILITY_LIBRARY
#include "wifi_availability_test.c"
#else
#include "wifi_workflow_test.c"
#endif
/* Deterministic provider boundary; the actual X4 app and adapter are linked. */
static unsigned policy_enables,policy_pauses,policy_resumes,policy_pending;
static uint32_t policy_snapshot_flags,policy_resume_flags;
static int policy_enable_result,policy_resume_result,policy_pause_result;
static bool policy_paused,policy_focus,policy_runtime_exit;
static int32_t policy_enable(void *c,uint64_t s){(void)c;assert(s==77&&text_live);policy_enables++;return policy_enable_result;}
static int32_t policy_snapshot(void *c,uint64_t s,risc_text_entry_checkpoint_state_v1 *out){
 (void)c;assert(s==77&&text_live);*out=(risc_text_entry_checkpoint_state_v1){sizeof(*out),policy_paused?RISC_TEXT_ENTRY_CHECKPOINT_PAUSED:policy_snapshot_flags};return 0;
}
static int32_t policy_pause(void *c,uint64_t s){
 (void)c;assert(s==77&&text_live);policy_pauses++;
 if(policy_pause_result)return policy_pause_result;
 if(policy_pending){policy_pending--;return RISC_TEXT_ENTRY_AGAIN;}
 policy_paused=true;return 0;
}
static int32_t policy_pause_focus(void *c,uint64_t s){policy_focus=true;return policy_pause(c,s);}
static int32_t policy_resume(void *c,uint64_t s,uint32_t flags){
 (void)c;assert(s==77&&text_live&&policy_paused);policy_resumes++;policy_resume_flags=flags;
 if(policy_resume_result)return policy_resume_result;
 policy_paused=false;return 0;
}
static int32_t unused_open_extended(void *c,const risc_text_entry_request_v1_extended *r,uint64_t *s){(void)c;(void)r;(void)s;assert(0);return -1;}
static int32_t unused_poll_extended(void *c,uint64_t s,risc_text_entry_state_v1_extended *r){(void)c;(void)s;(void)r;assert(0);return -1;}
static int32_t unused_open_long(void *c,const risc_text_entry_request_v1_long *r,uint64_t *s){(void)c;(void)r;(void)s;assert(0);return -1;}
static int32_t unused_poll_long(void *c,uint64_t s,risc_text_entry_state_v1_long *r){(void)c;(void)s;(void)r;assert(0);return -1;}
static risc_text_entry_api_v1_checkpoint text_api={
 .base={.base={.base={.base={.base={1,sizeof(text_api),NULL,text_open,text_poll,text_close},
  .home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG,.home_reason_version=RISC_TEXT_ENTRY_HOME_REASON_VERSION},
  .masked_tag=RISC_TEXT_ENTRY_MASKED_TAG,.masked_version=RISC_TEXT_ENTRY_MASKED_VERSION},
  .extended_tag=RISC_TEXT_ENTRY_EXTENDED_TAG,.extended_version=RISC_TEXT_ENTRY_EXTENDED_VERSION,
  .open_extended=unused_open_extended,.poll_extended=unused_poll_extended},
  .long_tag=RISC_TEXT_ENTRY_LONG_TAG,.long_version=RISC_TEXT_ENTRY_LONG_VERSION,.open_long=unused_open_long,.poll_long=unused_poll_long},
 .checkpoint_tag=RISC_TEXT_ENTRY_CHECKPOINT_TAG,.checkpoint_version=RISC_TEXT_ENTRY_CHECKPOINT_VERSION,
 .checkpoint_enable=policy_enable,.checkpoint_snapshot=policy_snapshot,.pause=policy_pause,.resume=policy_resume,.pause_for_focus_change=policy_pause_focus};


static unsigned resident_polls,resident_policies,retains;
static uint32_t resident_flags;
static bool request_policy=true,continuity,exit_apply,exit_poll,exit_close;
static bool test_exit(void){return policy_runtime_exit;}
static bool test_retain(void){++retains;return true;}
static int32_t fixture_checkpoint(uint64_t id,const risc_resident_request_v1 *q,risc_resident_reply_v1 *r){
 assert(id==2&&q->struct_size==sizeof(*q));r->flags=0;
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){++resident_polls;resident_flags=q->flags;r->flags=request_policy?RISC_RESIDENT_REPLY_POLICY_REQUEST|(continuity?PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY:0):0;}
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY){assert(policy_paused&&!surface.frame&&!sub_count);assert(!portable_wifi_async_owned());++resident_policies;request_policy=false;if(exit_apply)return RISC_RESIDENT_EXIT;}
 return RISC_RESIDENT_OK;
}
static bool test_resident(risc_resident_client_v1 *o){*o=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*o),.invocation=2,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=fixture_checkpoint};return true;}
static int32_t modal_poll(void*c,uint64_t id,risc_text_entry_state_v1*out){assert(!policy_paused&&!wifi_checkpoint.pausing);int rc=text_poll(c,id,out);if(exit_poll)policy_runtime_exit=true;return rc;}
static int32_t modal_close(void*c,uint64_t id){int rc=text_close(c,id);if(!rc){policy_paused=false;if(exit_close)policy_runtime_exit=true;}return rc;}
static void step(void){ticks+=501;wifi_tick();wifi_text_step();}
static void cancel(void){text_result.state=RISC_TEXT_ENTRY_CANCELLED;wifi_text_step();assert(!wifi_text_active&&!text_live&&!writes);}
int main(int argc,char **argv){
 (void)seed;(void)search_done;(void)text_done;
 assert(argc==2);unsigned n=(unsigned)atoi(argv[1]);
#ifdef TEST_MODAL_ATOMIC
 (void)availability_start;(void)change_availability;(void)advance;(void)explicit_discard;
 (void)assert_record;(void)drained;(void)clean_edit;(void)set_record;
 atomic_fixture_api=(risc_key_value_atomic_v1){1,sizeof(atomic_fixture_api),NULL,availability_atomic_state,availability_atomic_get,atomic_submit,atomic_collect,atomic_cancel};
 availability_fixture_api=(risc_key_value_availability_v1){1,sizeof(availability_fixture_api),NULL,availability_snapshot};
 current_availability=RISC_KEY_VALUE_AVAILABLE;
#endif
 workflow_start(true);draft();
 text_api.base.base.base.base.base.poll=modal_poll;text_api.base.base.base.base.base.close=modal_close;
 offered_text=&text_api.base.base.base.base.base;
 if(n==15)offered_text=&texts.base.base;
 if(n==16)policy_enable_result=RISC_TEXT_ENTRY_BUSY;
 if(n==17)policy_enable_result=RISC_TEXT_ENTRY_UNAVAILABLE;
 if(n==18)policy_enable_result=RISC_TEXT_ENTRY_INVALID;
 if(n==4||n==5||n==6||n==7){if(n==6){wifi_connect();
#ifdef TEST_MODAL_ATOMIC
 prepared();
#endif
 }else wifi_scan_start();assert(wifi_operation);}
 wifi_edit(n==14?WP_SSID:WP_PASSWORD);
 text_result=(risc_text_entry_state_v1){.struct_size=sizeof(text_result),.revision=1,.state=RISC_TEXT_ENTRY_EDITING};
 strcpy(text_result.text,"synthetic-draft");
 assert(wifi_text_active&&text_live&&!writes);
 if(n!=14)assert(text_request.reserved&RISC_TEXT_ENTRY_REQUEST_MASKED);
 else assert(!(text_request.reserved&RISC_TEXT_ENTRY_REQUEST_MASKED));
 if(n==0||n==1||n==14){continuity=n==1;step();assert(resident_policies==1&&policy_resumes==1&&!policy_paused);assert(policy_focus!=continuity);assert(policy_resume_flags==(RISC_TEXT_ENTRY_RESUME_REDRAW|(continuity?0:RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)));assert(!strcmp(credentials.password,"testpass123")&&!writes);cancel();}
 else if(n==2||n==3){policy_snapshot_flags=n==2?RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING:RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY|RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY;step();assert(!resident_policies&&!policy_pauses);policy_snapshot_flags=0;step();assert(resident_policies==1);cancel();}
 else if(n>=4&&n<=7){
  if(n==5){progress.phase=RISC_RADIO_RESULTS;progress.scan.state=GARDEN_RADIO_SCAN_DONE;}
  #ifndef TEST_MODAL_ATOMIC
  if(n==6){progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;}
#endif
  if(n==7)assert(!wifi_cancel_scan());
  step();assert(!resident_policies&&!policy_pauses);assert((resident_flags&(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY))==(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY));
  if(n==6)assert(!wifi_disconnect());else assert(!wifi_cancel_scan());stopped();step();assert(resident_policies==1);cancel();
 }
 else if(n==8){policy_pending=2;step();assert(wifi_checkpoint.pausing&&!resident_policies);unsigned polls=text_polls;step();assert(wifi_checkpoint.pausing&&!resident_policies&&text_polls==polls);step();assert(!wifi_checkpoint.pausing&&resident_policies==1&&policy_resumes==1&&text_polls==polls);cancel();}
 else if(n==9){policy_pause_result=RISC_TEXT_ENTRY_BUSY;step();assert(!wifi_checkpoint.pausing&&!resident_policies);policy_pause_result=0;step();assert(resident_policies==1);cancel();}
 else if(n==10){policy_resume_result=RISC_TEXT_ENTRY_INVALID;step();assert(wifi_text.retained&&retains==1&&!writes);unsigned polls=text_polls;wifi_text_step();assert(text_polls==polls);puts("resume failure fenced PASS");return 0;}
 else if(n==11){exit_apply=true;text_close_wait=2;step();assert(wifi_policy_exit&&wifi_text_active&&!wifi_home_return&&!policy_resumes);step();step();assert(wifi_text_active&&!wifi_home_return);step();assert(!wifi_text_active&&wifi_home_return&&!strcmp(credentials.password,"testpass123")&&!writes);}
 else if(n==12||n==13){text_result.state=RISC_TEXT_ENTRY_ACCEPTED;if(n==12)exit_poll=true;else exit_close=true;step();assert(wifi_home_return&&!wifi_text_active&&!strcmp(credentials.password,"testpass123")&&!writes);}
 else if(n==15||n==16||n==17){step();assert(!wifi_checkpoint.enabled&&!resident_policies);cancel();}
 else if(n==18){assert(wifi_text.retained&&retains==1&&!writes);puts("enable failure fenced PASS");return 0;}
 else if(n==19){text_result.state=RISC_TEXT_ENTRY_ACCEPTED;text_close_wait=2;step();assert(wifi_text_active&&!strcmp(credentials.password,"testpass123"));step();step();assert(!wifi_text_active&&draft_dirty&&!strcmp(credentials.password,"synthetic-draft")&&!writes);}
 else if(n==20){text_result.state=RISC_TEXT_ENTRY_CANCELLED;text_result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;step();assert(wifi_home_return&&!writes&&!strcmp(credentials.password,"testpass123"));}
 else if(n==21){policy_pending=1;step();assert(wifi_checkpoint.pausing);policy_runtime_exit=true;step();assert(wifi_home_return&&!wifi_text_active&&!resident_policies&&!writes);}
 else if(n==22){wifi_next_request.kind=RISC_RADIO_REQUEST_SCAN;ticks+=501;wifi_text_step();assert(!resident_policies);wifi_next_request.kind=0;step();assert(resident_policies==1);cancel();}
 else if(n==23){step();assert(resident_policies==1);text_result.state=RISC_TEXT_ENTRY_ACCEPTED;step();assert(draft_dirty&&!strcmp(credentials.password,"synthetic-draft")&&!writes);}
 else if(n==24){wifi_scan_start();policy_runtime_exit=true;text_close_wait=1;step();assert(wifi_text_active&&!wifi_home_return);step();assert(wifi_home_return&&!wifi_text_active);assert(!portable_wifi_close()&&portable_wifi_stop_pending());stopped();assert(portable_wifi_close()&&!wifi_operation&&!writes);}
 else assert(!"unknown case");
 policy_runtime_exit=false;wifi_home_return=false;
#ifdef TEST_MODAL_ATOMIC
 if(draft_dirty){assert(!portable_wifi_close()&&page==WP_STORAGE);wifi_storage_discard();}
#endif
 finish();printf("Wi-Fi modal policy %u PASS\n",n);return 0;
}
