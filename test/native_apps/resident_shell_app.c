#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "../../Apps/PaperPresentation.h"
#include <assert.h>
extern void resident_fixture_event(unsigned event,int value);
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
extern bool resident_fixture_guard(void);
bool portable_app_before_launch(const char *path){(void)path;return resident_fixture_guard();}
#endif
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
void app_main(void) {
 const t5_app_api_v1 *app=t5_app_get_api(1);
 const paper_presentation *paper=paper_presentation_get();assert(app && paper);
 app->set_back_exits_app(false);
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_LEGACY_HANDOFF)
 int resumed=portable_resident_run_foreground(NULL);
 if(resumed!=PORTABLE_RESIDENT_NO_PENDING) {
  resident_fixture_event(1,resumed);
  return;
 }
#endif
 paper->begin();app->fill_rect(90,180,110,140,true);app->present(true);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 int result=portable_resident_run_foreground("client.elf");
 resident_fixture_event(1,result);
 if(result>=0 && result!=PORTABLE_RESIDENT_HANDOFF) {
  resident_fixture_event(5,portable_resident_take_sleep());
  resident_fixture_event(6,1);
  for(unsigned i=0;i<4;i++){t5_app_input_t input={0};assert(app->poll(&input,20)&&!input.exit_requested);}
  resident_fixture_event(6,0);
 }
#else
 int state=71;unsigned polls=0;
 for(;;) {
  t5_app_input_t input={0};
  if(!app->poll(&input,20))break;
  assert(state==71);++polls;
  if(input.exit_requested)break;
  resident_fixture_event(2,(int)polls);
 }
 resident_fixture_event(3,state);
#endif
}
