/* Reuse provider fixtures; exercise the actual adapter, not a copied limit. */
#define main previous_fixture_main
#define portable_catalog old_fixture_catalog
#define portable_catalog_count old_fixture_count
#define risc_runtime_get_api previous_fixture_runtime
#include "portable_adapter_test.c"
#undef main
#undef portable_catalog
#undef portable_catalog_count
#undef risc_runtime_get_api
#ifndef TEST_CATALOG_COUNT
#define TEST_CATALOG_COUNT 17
#endif

/* Fixed backing storage also catches accidental reads of a rejected count. */
#define ENTRY(n) {.display_name="Entry " #n,.file_name="app-" #n ".elf",.icon="solid:f017",.compatible=true}
const t5_app_manifest_t portable_catalog[20] = {
 ENTRY(0),
 {.display_name="Unavailable",.file_name="unavailable.elf",.icon="solid:f017",.compatible=false},
 ENTRY(2),ENTRY(3),ENTRY(4),ENTRY(5),ENTRY(6),ENTRY(7),ENTRY(8),
 ENTRY(9),ENTRY(10),ENTRY(11),ENTRY(12),ENTRY(13),ENTRY(14),ENTRY(15),
 {.display_name="Timecard",.file_name="timecard.elf",.icon="solid:f274",.compatible=true},
 {.display_name="Waterfall",.file_name="waterfall.elf",.icon="solid:f0ec",.compatible=true},
 ENTRY(18),ENTRY(19)
};
const unsigned portable_catalog_count=TEST_CATALOG_COUNT;
static unsigned launch_calls;
static bool runtime_unavailable;
static bool catalog_launch(const char *file) {
 ++launch_calls;
 if(runtime_unavailable || !strcmp(file,"unavailable.elf"))return false;
 return launch(file);
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
 static risc_runtime_api_v1 catalog_runtime;
 if(version!=1)return NULL;
 catalog_runtime=rt;catalog_runtime.request_launch=catalog_launch;
 return &catalog_runtime;
}
void app_main(void) {
 const t5_app_api_v1 *api=t5_app_get_api(1);assert(api);
 t5_app_manifest_t item={0};
 const bool admitted=portable_catalog_count<=PORTABLE_CATALOG_LIMIT;
 assert(api->installed_apps_refresh()==admitted);
 const unsigned expected=admitted?portable_catalog_count:0;
 assert(api->installed_apps_count()==expected);
 assert(!api->installed_apps_get(0,NULL));
 assert(!api->installed_apps_get(expected,&item));
 assert(!api->installed_apps_get(UINT32_MAX,&item));
 assert(!api->request_app_launch(expected));
 assert(!api->request_app_launch(UINT32_MAX));
 assert(!launch_calls && !launched[0]);
 for(unsigned i=0;i<expected;i++) {
  assert(api->installed_apps_get(i,&item));
  assert(!memcmp(&item,&portable_catalog[i],sizeof(item)));
  unsigned before=launch_calls;
  bool ok=api->request_app_launch(i);
  assert(launch_calls==before+1);
  assert(ok==item.compatible);
  if(ok)assert(!strcmp(launched,item.file_name));
 }
 if(expected) {
  /* A manifest is not a promise that the runtime can launch it now. */
  unsigned last=expected-1,before=launch_calls;
  char previous[sizeof(launched)];memcpy(previous,launched,sizeof(previous));
  runtime_unavailable=true;
  assert(!api->request_app_launch(last) && launch_calls==before+1);
  assert(!memcmp(previous,launched,sizeof(previous)));
  assert(api->installed_apps_refresh() && api->installed_apps_count()==expected);
  runtime_unavailable=false;
 }
 api->clear();api->fill_rect(0,0,240,240,true);
 assert(api->draw_icon(90,90,"solid:f274",36,false));api->present(false);
}
int main(void) {
 assert(app_module_init()==0);app_main();app_module_fini();
 assert(!frames && !grants && !subs);
 printf("Portable catalog limit %u, count %u: refresh/get/launch/unavailable/cleanup passed\n",
        (unsigned)PORTABLE_CATALOG_LIMIT,portable_catalog_count);
 return 0;
}
