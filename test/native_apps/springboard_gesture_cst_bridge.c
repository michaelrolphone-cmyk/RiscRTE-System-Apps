#include <stdlib.h>
#include "cst9217_test_model.h"
extern const risc_driver_v2 *t5_driver_get(uint32_t);
static bool faulted;static cst_test_model hardware;static const risc_touch_api_v1 *real_touch;static const risc_driver_v2 *driver;
const risc_touch_api_v1 *bridge_start(unsigned*ms){driver=t5_driver_get(2);real_touch=driver->capability;cst_test_setup(&hardware);hardware.touch=real_touch;assert(driver->start(hardware.dependencies,6));*ms=(unsigned)hardware.clock_ms;return real_touch;}
unsigned bridge_poll(unsigned x,unsigned y,bool down,unsigned ms,size_t n,bool*ok){cst_test_report(&hardware,x,y,down);if(getenv("TEST_DIAG_GAP")&&!faulted&&ms>1000&&down){hardware.raw[6]=0;faulted=true;}hardware.clock_ms=ms;*ok=real_touch->poll(NULL,n);return (unsigned)hardware.clock_ms-ms;}
void bridge_stop(void){assert(driver->quiesce());cst_test_clean(&hardware);}
