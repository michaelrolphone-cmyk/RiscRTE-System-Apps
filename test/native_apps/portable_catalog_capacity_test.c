/* Reuse provider fixtures; exercise actual client adapter at both boundaries. */
#define main previous_fixture_main
#define portable_catalog old_fixture_catalog
#define portable_catalog_count old_fixture_count
#include "portable_adapter_test.c"
#undef main
#undef portable_catalog
#undef portable_catalog_count
#ifndef TEST_CATALOG_COUNT
#define TEST_CATALOG_COUNT 17
#endif
const t5_app_manifest_t portable_catalog[TEST_CATALOG_COUNT] = {
 [16]={.display_name="Boundary entry",.file_name="catalog-last.elf",.icon="solid:f274",.compatible=true}
};
const unsigned portable_catalog_count=TEST_CATALOG_COUNT;
void app_main(void) {
 const t5_app_api_v1 *api=t5_app_get_api(1);assert(api);
 t5_app_manifest_t item={0};
#if TEST_CATALOG_COUNT == 17
 assert(api->installed_apps_refresh()&&api->installed_apps_count()==17);
 assert(api->installed_apps_get(16,&item)&&!strcmp(item.file_name,"catalog-last.elf"));
 assert(!api->installed_apps_get(17,&item));
 api->clear();api->fill_rect(0,0,240,240,true);
 assert(api->draw_icon(90,90,"solid:f274",36,false));api->present(false);
 assert(api->request_app_launch(16)&&!strcmp(launched,"catalog-last.elf"));
#else
 assert(!api->installed_apps_refresh()&&api->installed_apps_count()==0);
 assert(!api->installed_apps_get(16,&item)&&!api->request_app_launch(16)&&!launched[0]);
#endif
}
int main(void){assert(app_module_init()==0);app_main();app_module_fini();assert(!frames&&!grants&&!subs);puts("Portable catalog 17 acceptance / 18 rejection and real calendar-check glyph passed");return 0;}
