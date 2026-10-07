/* Real scoped GPIO and synchronization dependencies, never a Runtime shim. */
#include "desk_clock_runtime_gate.h"
#include <GardenPlatformV1.h>
#include <RiscHardwareConfigV1.h>
#include <RiscProviderSyncV1.h>
#include <RiscProviderV2.h>
#include <string.h>
static const garden_gpio_v1 *gpio;
static const risc_provider_sync_api_v1 *sync_api;
static uint64_t input, output, lock_token;
static bool hold(bool enabled) {
    return gpio->deep_sleep_hold(gpio->context,output,enabled)==0;
}
static bool lock(bool enabled) {
    return enabled ? sync_api->try_lock(sync_api->context,lock_token)
                   : sync_api->unlock(sync_api->context,lock_token);
}
static int32_t enter(uint32_t milliseconds) {
    /* Deliberately leave caller-owned hold/lock live on an ordinary refusal. */
    return gpio->deep_sleep_for(gpio->context,input,false,milliseconds);
}
static bool start(const risc_provider_dependency_v1 *dependencies,size_t count) {
    const risc_hardware_device_v1 *hw=0;
    for(size_t i=0;i<count;i++) {
        if(!strcmp(dependencies[i].capability_id,"platform.gpio"))gpio=dependencies[i].api;
        if(!strcmp(dependencies[i].capability_id,RISC_PROVIDER_SYNC_CAPABILITY))sync_api=dependencies[i].api;
        if(!strcmp(dependencies[i].capability_id,"hardware.device"))hw=dependencies[i].api;
    }
    if(!hw || !gpio || !sync_api || gpio->struct_size<GARDEN_GPIO_DEEP_SLEEP_FOR_V1_SIZE ||
       !gpio->deep_sleep_hold || !gpio->deep_sleep_for)return false;
    const risc_hw_gpio_bank_v1 *config=hw->config;
    return config->count==2 && sync_api->create(sync_api->context,&lock_token) &&
        gpio->claim(gpio->context,config->pins[0],false,false,true,&input) &&
        gpio->claim(gpio->context,config->pins[1],true,false,false,&output);
}
static bool quiesce(void) {
    if(output && !gpio->release(gpio->context,output))return false;
    output=0;
    if(input && !gpio->release(gpio->context,input))return false;
    input=0;
    if(lock_token && !sync_api->destroy(sync_api->context,lock_token))return false;
    lock_token=0;return true;
}
static void stop(void) { gpio=0;sync_api=0; }
static const desk_gate_api api={1,sizeof(api),hold,lock,enter};
static const risc_driver_v2 driver={2,sizeof(driver),"desk-clock-gate",DESK_GATE_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi) {
    return abi==2?&driver:0;
}
