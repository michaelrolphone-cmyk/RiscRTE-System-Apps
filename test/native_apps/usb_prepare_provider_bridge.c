/* Reuse the provider's SD/PHY/DCD fixtures; link its actual driver and TinyUSB. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main provider_fixture_main
#include USB_OWNER_TEST_SOURCE
#undef main
#pragma GCC diagnostic pop
extern void usb_fixture_sd_transaction(void);
static int32_t checked_prepare(void*c,uint64_t t,uint64_t*n,uint32_t*z){
 usb_fixture_sd_transaction();return sd_prepare_step(c,t,n,z);
}
const risc_usb_device_msc_api_v1 *usb_fixture_provider(unsigned mode) {
 const risc_driver_v2*d=t5_driver_get(2);api=d->capability;
 volume.prepare_step=checked_prepare;pending_steps=mode==1?100:2;
 if(mode==2)prepare_result=RISC_STORAGE_EXPORT_REFUSED;
 if(mode==3){prepare_result=RISC_STORAGE_EXPORT_RETAINED;fail_end=true;}
 risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clock_api},{RISC_USB_PHY_RESOURCE_CAPABILITY,1,&phy},{"storage.volume",1,&volume}};
 assert(d->start(deps,3));return api;
}
void usb_fixture_recover(void){fail_end=false;}
unsigned usb_fixture_prepare_calls(void){return prepare_calls;}
bool usb_fixture_phy_owned(void){return phy_owned;}
bool usb_fixture_quiesce(void){return t5_driver_get(2)->quiesce();}
