#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "RiscRuntimeV1.h"
#include "RiscFailureEvidenceV1.h"
#include "RiscResidentShellV1.h"
#include "../../Apps/PaperPresentation.h"
#include <assert.h>
extern bool failure_mode(const char *);
extern void failure_event(unsigned,int);
extern unsigned failure_hosts(void);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static int32_t dispatch(void *context,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply){(void)context;(void)request;(void)reply;assert(false);return RISC_RESIDENT_BUSY;}
void app_main(void) {
 failure_event(20,1);
 const risc_runtime_api_v1 *runtime=risc_runtime_get_api(1);
 const t5_app_api_v1 *app=t5_app_get_api(1);const paper_presentation *paper=paper_presentation_get();assert(app&&paper);
 app->set_back_exits_app(false);
 if(failure_mode("repeat") && failure_hosts()==1) {
  risc_failure_evidence_client_v1 client={.struct_size=sizeof(client)};assert(runtime->failure_evidence(&client));
  for(unsigned i=0;i<2;++i){risc_failure_evidence_v1 record={.struct_size=sizeof(record)};assert(client.read(client.invocation,&record)==RISC_FAILURE_EVIDENCE_OK && (record.flags&RISC_FAILURE_PENDING));}
  risc_resident_client_v1 shell={.struct_size=sizeof(shell)};assert(runtime->resident_shell(&shell));
  const risc_resident_callbacks_v1 callbacks={1,sizeof(callbacks),NULL,dispatch,NULL};
  assert(shell.register_shell(shell.invocation,&callbacks)==RISC_RESIDENT_OK);
  risc_resident_result_v1 outcome={.struct_size=sizeof(outcome)};
  assert(shell.run_foreground(shell.invocation,"client.elf",&outcome)==RISC_RESIDENT_HANDOFF);return;
 }
 int initial=portable_resident_run_foreground(NULL);failure_event(1,initial);if(initial<0)return;
 assert(initial==PORTABLE_RESIDENT_NO_PENDING);
 if(failure_mode("repeat") && failure_hosts()==2){assert(portable_resident_run_foreground("client.elf")==PORTABLE_RESIDENT_HANDOFF);return;}
 paper->begin();app->fill_rect(60,160,150,140,true);app->present(true);
 for(unsigned i=0;i<3;++i){t5_app_input_t input={0};assert(app->poll(&input,20)&&!input.exit_requested);}
 assert(!portable_resident_take_sleep() && !portable_resident_take_launch());failure_event(3,1);
}
#else
void app_main(void) {
 const risc_runtime_api_v1 *runtime=risc_runtime_get_api(1);
 risc_failure_evidence_client_v1 client={.struct_size=sizeof(client)};
 assert(!runtime->failure_evidence(&client));
}
#endif
