/* Minimal caller for the production resident host controller and renderer. */
#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "../../Apps/PaperPresentation.h"
#include "../../Apps/PaperFrame.h"
#include <assert.h>
extern void loading_result(int);
extern bool loading_mode(const char *);
const t5_app_manifest_t portable_catalog[]={
 {.display_name="File Browser",.file_name="client.elf",.icon="solid:f07c",.compatible=true},
 {.display_name="Game Boy",.file_name="second.elf",.icon="solid:f11b",.compatible=true},
 {.display_name="A deliberately long application display name with enough words to span the available title area",
  .file_name="long.elf",.icon="solid:f013",.compatible=true}
};
const unsigned portable_catalog_count=sizeof(portable_catalog)/sizeof(portable_catalog[0]);
void app_main(void) {
 const t5_app_api_v1 *app=t5_app_get_api(1);
 const paper_presentation *paper=paper_presentation_get();assert(app&&paper);
 app->set_back_exits_app(false);
 assert(paper_frame_ready());paper->begin();app->fill_rect(50,80,160,40,true);app->present(true);
 assert(paper_frame_drain());
 const char *path=loading_mode("unknown")?"new-app.elf":loading_mode("long")?"long.elf":"client.elf";
 if(loading_mode("no-pending")||loading_mode("resume"))path=NULL;
 unsigned repeat=loading_mode("repeat")?3:1;
 for(unsigned i=0;i<repeat;++i) {
  paper_transition_begin();
  if(loading_mode("writable-refused")){paper->begin();app->fill_rect(40,40,80,80,true);}
  int result=portable_resident_run_foreground(path);loading_result(result);
  if(result<0 || result==PORTABLE_RESIDENT_HANDOFF)return;
  /* The same recovery path used by Home can immediately paint and take input. */
  paper_transition_cancel();assert(paper_frame_ready());paper->begin();app->fill_rect(50,80,160,40,true);
  paper->text(24,160,432,result?"HOME":"UNABLE TO OPEN APP. RETRY.",1,true,true);
  app->present(true);assert(paper_frame_drain());
  t5_app_input_t input={0};assert(app->poll(&input,20)&&!input.exit_requested);
  assert(!portable_resident_take_sleep()&&!portable_resident_take_launch());
 }
}
