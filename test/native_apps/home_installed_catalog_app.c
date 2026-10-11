#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "../../Apps/PaperPresentation.h"
#include <assert.h>
#include "PortableAppSleep.h"
extern void test_catalog_sleep_request(void);
extern void catalog_assert(unsigned);
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;catalog_assert(4);return 1;}
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
void app_main(void){
 const t5_app_api_v1*a=t5_app_get_api(1);const paper_presentation*p=paper_presentation_get();assert(a&&p);
 a->set_back_exits_app(false);t5_app_input_t in={0};assert(a->poll(&in,1));catalog_assert(0);
 p->begin();a->fill_rect(10,10,80,80,true);a->present(true);
 for(unsigned i=0;i<1000;++i)assert(a->poll(&in,1));catalog_assert(1);
 for(unsigned i=0;i<1000;++i)assert(portable_resident_run_foreground("points_in_time.elf")==0);
 catalog_assert(2);const char*path=portable_resident_take_launch();assert(path);
 assert(portable_resident_run_foreground(path)==1);catalog_assert(3);
 catalog_assert(5);assert(a->poll(&in,1));test_catalog_sleep_request();
 for(unsigned i=0;i<1000;++i)assert(a->poll(&in,1));catalog_assert(6);
 assert(a->poll(&in,1));catalog_assert(7);
}
