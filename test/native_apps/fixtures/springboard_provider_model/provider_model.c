#include "DriverSupport.h"
#include "RiscDisplayBackendV1.h"
#include "GardenPlatformV1.h"
#include "RiscPowerRailV1.h"
#include "provider_log_mock.h"
#include <assert.h>
#include <stdio.h>
static uint64_t now; static unsigned pair_cost_us, model_polls;static unsigned opened,closed,rows,levels;
static bool open_ok=true,close_ok=true,row_ok=true,level_ok=true;
static uint8_t last_level;
static bool backend_live,gpio_live,hold_live,release_ok=true;
static int32_t hold_result;
static bool claim_reset(void*c,uint8_t pin,bool output,bool initial,bool pullup,uint64_t*out){
 (void)c;assert(pin==8&&output&&!initial&&!pullup&&!backend_live&&!gpio_live);gpio_live=true;*out=9;return true;
}
static bool release_gpio(void*c,uint64_t t){(void)c;assert(t==9&&gpio_live&&!hold_live);if(!release_ok)return false;gpio_live=false;return true;}
static int32_t hold_reset(void*c,uint64_t t,bool enabled){(void)c;assert(t==9&&gpio_live&&!backend_live);if(hold_result)return hold_result;hold_live=enabled;return 0;}
static garden_gpio_v1 reset_api={.api_version=1,.struct_size=sizeof(reset_api),.claim=claim_reset,.release=release_gpio,.deep_sleep_hold=hold_reset};
static uint64_t mono(void* c){(void)c;return now/1000;}
static void delay(void*c,uint32_t ms){(void)c;now+=(uint64_t)ms*1000;}
static bool backend_open(void*c,uint64_t*t){(void)c;assert(!gpio_live&&!hold_live);++opened;*t=123;backend_live=true;return open_ok;}
static bool backend_row(void*c,uint64_t t,uint32_t y,const uint16_t*p,size_t n){(void)c;assert(t==123&&y<502&&p&&n==410);if(!row_ok)return false;++rows;if(y&1)now+=pair_cost_us;return true;}
static bool backend_level(void*c,uint64_t t,uint8_t v){(void)c;assert(t==123);++levels;last_level=v;return level_ok;}
static bool backend_close(void*c,uint64_t t){(void)c;assert(t==123);++closed;if(close_ok)backend_live=false;return close_ok;}
static bool rail_info(void*c,risc_power_rail_info_v1*i){(void)c;*i=(risc_power_rail_info_v1){.struct_size=sizeof(*i),.enabled=true,.configuration_verified=true,.configured_millivolts=3300};return true;}
static bool rail_write(void*c,bool b){(void)c;(void)b;assert(!"vendor baseline must not modify PMIC");return false;}
static risc_display_backend_v1 native={1,sizeof(native),NULL,backend_open,backend_row,backend_level,backend_close};
static risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,mono,delay};
static risc_power_rail_api_v1 rail={1,sizeof(rail),NULL,rail_info,rail_write};
#ifndef PROFILE_PANEL_PROVIDER_SOURCE
#error Supply the real panel provider source path for this fixture
#endif
#include PROFILE_PANEL_PROVIDER_SOURCE

/* Measurement-only fake-clock model. All display methods/poll are production. */
void model_reset(unsigned w,unsigned h,unsigned cost_us){
 assert(w==WIDTH&&h==HEIGHT);
 if(running)assert(quiesce());
 now=0;rows=opened=closed=levels=model_polls=0;pair_cost_us=cost_us;
 risc_hw_spi_display_v2 cfg={.base={.struct_size=sizeof(cfg),.reset=8}};
 risc_hardware_device_v1 hw={.api_version=1,.struct_size=sizeof(hw),.instance_id=5,.compatible="chipone,co5300",.revision="unspecified",.config_type="display.spi",.config_version=2,.config_size=sizeof(cfg),.config=&cfg};
 risc_provider_dependency_v1 deps[]={{"hardware.device",1,&hw},{"platform.clock",1,&clock_api},{RISC_DISPLAY_BACKEND_CAPABILITY,1,&native},{"power.rail",1,&rail},{"platform.gpio",1,&reset_api}};
 assert(start(deps,5));
}
bool model_submit(void){
 risc_display_surface_v1 s;
 if(!api.base.acquire(NULL,FORMAT,&s))return false;
 risc_display_present_token_v1 token;
 return api.base.submit(NULL,s.frame,NULL,0,NULL,&token);
}
bool model_pending(void){return active;}
void model_yield(unsigned request_ms){display_poll(8);++model_polls;now+=(uint64_t)request_ms*1000;}
unsigned model_now_ms(void){return (unsigned)(now/1000);}
uint64_t model_now_us(void){return now;}
void model_advance_us(unsigned duration_us){now+=duration_us;}
unsigned model_row_count(void){return rows;}
unsigned model_poll_count(void){return model_polls;}
const risc_display_output_api_v1* model_display_api(void){return &api.base;}
void model_teardown(void){if(running)assert(quiesce());}
