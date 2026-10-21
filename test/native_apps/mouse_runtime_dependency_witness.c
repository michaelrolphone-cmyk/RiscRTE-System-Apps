/* TEST ONLY: exact existing usb.hid@1 / platform.clock@1 dependency ABIs.
 * Compiled twice as two independent provider ELFs, never product-installed. */
#include "mouse_runtime_fixture.h"
#include <RiscUsbHidV1.h>
#include <RiscPlatformClockV1.h>
#include <assert.h>
#include <string.h>

#if defined(MOUSE_RUNTIME_HID_WITNESS)
static const uint8_t descriptor[] = {
 0x05,1, 0x09,2, 0xa1,1, 0x09,1, 0xa1,0,
 0x05,9, 0x19,1, 0x29,3, 0x15,0, 0x25,1, 0x75,1, 0x95,3, 0x81,2,
 0x75,5, 0x95,1, 0x81,3,
 0x05,1, 0x09,0x30, 0x09,0x31, 0x09,0x38,
 0x15,0x81, 0x25,0x7f, 0x75,8, 0x95,3, 0x81,6,
 0x05,0x0c, 0x0a,0x38,2, 0x95,1, 0x81,6, 0xc0, 0xc0
};
static const uint64_t raw_token = UINT64_C(0x9753000042);
static void check_raw(uint64_t raw) {
 assert(raw == raw_token && mouse_runtime_control()->raw == raw);
}
static bool scan(void *context, size_t budget) {
 (void)context;mouse_runtime_lower_call("hid:scan");assert(budget == 16);return true;
}
static bool interfaces(void *context, risc_usb_hid_interface_v1 *out, size_t *n) {
 (void)context;mouse_runtime_lower_call("hid:interfaces");assert(out && n && *n >= 1);
 *n = mouse_runtime_control()->present ? 1 : 0;
 if(*n)out[0] = (risc_usb_hid_interface_v1){.device=100,.interface_number=2,
   .report_descriptor_length=sizeof(descriptor),.max_packet=64};
 return true;
}
static uint64_t open_hid(void *context, uint64_t device, uint8_t iface, uint8_t alt) {
 (void)context;mouse_runtime_lower_call("hid:open");
 mouse_runtime_controls *c = mouse_runtime_control();
 assert(c->present && !c->raw && device == 100 && iface == 2 && !alt);
 ++c->opens;return c->raw = raw_token;
}
static bool report_descriptor(void *context, uint64_t raw, uint8_t *out, size_t *n) {
 (void)context;mouse_runtime_lower_call("hid:descriptor");check_raw(raw);
 assert(out && n && *n >= sizeof(descriptor));
 memcpy(out,descriptor,sizeof(descriptor));*n=sizeof(descriptor);return true;
}
static bool boot(void *context, uint64_t raw, bool enabled) {
 (void)context;(void)enabled;mouse_runtime_lower_call("hid:boot");check_raw(raw);
 assert(!"Non-boot witness must use the real report descriptor");return false;
}
static int32_t read_hid(void *context, uint64_t raw, uint8_t *out, size_t n, uint32_t ms) {
 (void)context;mouse_runtime_lower_call("hid:read");check_raw(raw);
 mouse_runtime_controls *c=mouse_runtime_control();
 assert(c->present && out && n == 64 && ms == 1);
 if(!c->remaining)return 0;
 --c->remaining;memcpy(out,c->report,sizeof(c->report));return sizeof(c->report);
}
static bool present(void *context, uint64_t raw) {
 (void)context;mouse_runtime_lower_call("hid:present");check_raw(raw);
 return mouse_runtime_control()->present;
}
static bool close_hid(void *context, uint64_t raw) {
 (void)context;mouse_runtime_lower_call("hid:close");check_raw(raw);
 mouse_runtime_controls *c=mouse_runtime_control();++c->closes;c->last_close=raw;
 if(c->close_fails)return false;
 c->raw=0;return true;
}
static bool start(const risc_provider_dependency_v1 *deps, size_t n) {
 (void)deps;mouse_runtime_event("hid:start");return !n;
}
static bool quiesce(void) {
 mouse_runtime_event("hid:quiesce");return !mouse_runtime_control()->raw;
}
static void stop(void) {assert(!mouse_runtime_control()->raw);mouse_runtime_event("hid:stop");}
static const risc_usb_hid_api_v1 api={1,sizeof(api),0,scan,interfaces,open_hid,
 report_descriptor,boot,read_hid,present,close_hid};
static const risc_driver_v2 driver={2,sizeof(driver),"mouse-test-hid","usb.hid",1,
 &api,start,stop,quiesce};
#elif defined(MOUSE_RUNTIME_CLOCK_WITNESS)
static uint64_t now(void *context) {
 (void)context;mouse_runtime_lower_call("clock:now");return mouse_runtime_control()->now;
}
static void sleep_ms(void *context, uint32_t ms) {
 (void)context;mouse_runtime_lower_call("clock:sleep");assert(ms == 1);
 mouse_runtime_control()->now+=ms; /* simulated time only, not a scheduler proof */
}
static bool start(const risc_provider_dependency_v1 *deps, size_t n) {
 (void)deps;mouse_runtime_event("clock:start");return !n;
}
static bool quiesce(void) {mouse_runtime_event("clock:quiesce");return true;}
static void stop(void) {mouse_runtime_event("clock:stop");}
static const risc_platform_clock_api_v1 api={1,sizeof(api),0,now,sleep_ms};
static const risc_driver_v2 driver={2,sizeof(driver),"mouse-test-clock","platform.clock",1,
 &api,start,stop,quiesce};
#else
#error Select exactly one software dependency witness
#endif
__attribute__((visibility("default")))
const risc_driver_v2 *t5_driver_get(uint32_t abi) {return abi == 2 ? &driver : 0;}
