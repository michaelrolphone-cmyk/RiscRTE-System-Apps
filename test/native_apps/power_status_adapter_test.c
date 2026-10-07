/* The app-local extension copies the existing four-byte generic sample. */
#define PORTABLE_POWER_STATUS
#include "../../lib/PortableApps/src/adapter.c"
#include <assert.h>
#include <stdio.h>
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){(void)v;return NULL;}
static unsigned reads;
static bool success;
static risc_battery_sample_v1 value;
static bool sample(void *c,risc_battery_sample_v1 *out){(void)c;reads++;*out=value;return success;}
int main(void) {
    risc_battery_gauge_api_v1 provider={1,sizeof(provider),NULL,sample};
    risc_battery_sample_v1 out={4200,100,255};
    assert(sizeof(out)==4 && sizeof(risc_battery_gauge_api_v1)==offsetof(risc_battery_gauge_api_v1,read)+sizeof(provider.read));
    assert(!portable_power_read(NULL));assert(!portable_power_read(&out));
    assert(!out.millivolts && out.percent==255 && out.flags==RISC_BATTERY_PROFILE_MISSING);
    gauge=&provider;bg.api=gauge;
    for(unsigned n=0;n<256;n++) {
        value=(risc_battery_sample_v1){3970,(uint8_t)n,(uint8_t)n};success=true;
        assert(portable_power_read(&out) && !memcmp(&out,&value,sizeof(out)));
        success=false;assert(!portable_power_read(&out));
        assert(out.millivolts==0 && out.percent==255 && out.flags==RISC_BATTERY_PROFILE_MISSING);
    }
    success=true;failed=true;unsigned before=reads;assert(!portable_power_read(&out)&&reads==before);
    failed=false;bg.api=NULL;assert(!portable_power_read(&out)&&reads==before);
    bg.api=gauge;provider.read=NULL;assert(!portable_power_read(&out)&&reads==before);
    puts("Power adapter: exact legacy layout, 256 copied flags/percent states, provider failure reset, inactive/failed guards PASS");
}
