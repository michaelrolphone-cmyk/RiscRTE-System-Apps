#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "PortableAppSleep.h"
#include "RiscResidentShellV1.h"
#include "../../Apps/PaperPresentation.h"
#include "../../Apps/PaperFrame.h"
#include <assert.h>
#include <string.h>
extern const char *policy_fixture_mode(void);
extern void policy_fixture_event(unsigned,unsigned);
extern void policy_fixture_step(unsigned);
extern void policy_fixture_busy(bool);
extern void policy_fixture_capture(bool);
extern void policy_fixture_set_busy_hook(void (*)(bool));
extern unsigned policy_test_state(void);
extern int policy_test_checkpoint(unsigned);
extern void policy_test_host_busy(bool);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
#define ROLE 1
#else
#define ROLE 2
extern bool policy_fixture_guard(void);
static bool ui_active;
bool portable_app_ui_active(void){return ui_active;}
bool portable_app_before_launch(const char *path){(void)path;return policy_fixture_guard();}
#endif
__attribute__((constructor)) static void mapped(void){policy_fixture_event(10,ROLE);}
__attribute__((destructor)) static void unmapped(void){policy_fixture_event(11,ROLE);}
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
void app_main(void) {
 const t5_app_api_v1 *app=t5_app_get_api(1);const paper_presentation *paper=paper_presentation_get();assert(app&&paper);
 policy_fixture_event(12,ROLE);app->set_back_exits_app(false);
 assert(paper_frame_ready());
 paper->begin();app->fill_rect(90,180,110,140,true);app->present(true);
 assert(paper_frame_drain());
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 policy_fixture_set_busy_hook(policy_test_host_busy);
 int status=portable_resident_run_foreground("client.elf");policy_fixture_event(1,(unsigned)(status+1));
 if(status>=0 && portable_resident_take_sleep()) {
  policy_fixture_event(5,1);
  assert(portable_app_alarm_sleep(risc_runtime_get_api(1),NULL,NULL,NULL)==1);
 }
#else
 const char *mode=policy_fixture_mode();const unsigned model=71;
 if(!strcmp(mode,"animation")) {
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK);
  assert((policy_test_state()&2u)!=0);
  ui_active=true;
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY)==RISC_RESIDENT_BUSY);
  assert((policy_test_state()&2u)!=0);
  ui_active=false;
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY)==RISC_RESIDENT_OK);
  assert(policy_test_state()==0);policy_fixture_event(3,model);return;
 }
 if(!strcmp(mode,"policy-busy") || !strcmp(mode,"capture-pending")) {
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK);
  assert((policy_test_state()&2u)!=0);
  bool capturing=!strcmp(mode,"capture-pending");
  if(capturing)policy_fixture_capture(true);else policy_fixture_busy(true);
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY)==RISC_RESIDENT_BUSY);
  assert((policy_test_state()&2u)!=0);
  if(capturing){
   assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_BUSY);
   assert(policy_test_state()==3);policy_fixture_capture(false);
   assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK);
  }else policy_fixture_busy(false);
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY)==RISC_RESIDENT_OK);
  assert(policy_test_state()==0);policy_fixture_event(3,model);return;
 }
 for(unsigned step=0;step<12;++step) {
  policy_fixture_step(step);
  t5_app_input_t input={0};bool ok=app->poll(&input,20);
  assert(model==71);policy_fixture_event(2,policy_test_state());
  if(!ok || input.exit_requested)break;
 }
 if(!strcmp(mode,"dirty-edit")){
  policy_fixture_event(4,0);
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)==RISC_RESIDENT_BUSY);
  assert(policy_test_checkpoint(RISC_RESIDENT_CHECKPOINT_SLEEP)==RISC_RESIDENT_BUSY);
 }
 policy_fixture_event(3,model);
#endif
}
