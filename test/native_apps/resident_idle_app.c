#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "../../Apps/PaperPresentation.h"
#include <assert.h>
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
extern void idle_result(int);
extern void idle_test_fail(void),idle_test_retain(void);
extern bool idle_test_flipped(void);
extern void idle_fixture_bind(void (*)(void),void (*)(void),bool (*)(void));
void app_main(void) {
 const t5_app_api_v1 *app=t5_app_get_api(1);
 const paper_presentation *paper=paper_presentation_get();assert(app&&paper);
 paper->begin();app->fill_rect(90,180,110,140,true);app->present(true);
 idle_fixture_bind(idle_test_fail,idle_test_retain,idle_test_flipped);
 idle_result(portable_resident_run_foreground("child.elf"));
}
