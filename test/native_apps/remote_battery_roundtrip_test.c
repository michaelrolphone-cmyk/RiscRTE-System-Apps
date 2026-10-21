/* Exact producer codec -> actual sensor AD parser -> actual consumer session.
 * Reuse deterministic capability callbacks only; no physical radio operation. */
#define main component_suite_main
#include "remote_battery_test.c"
#undef main
#include "Drivers/ble_telemetry/codec.h"
#include "Drivers/ble_sensors/ble_scan_core.h"
static void packet(ble_device *device,const uint8_t *ad,uint8_t length,uint32_t now) {
 assert(ble_ad_valid(ad,length));device->seen=now;device->reports++;
 ble_ad(device,ad,length);device->measurement_age_ms=now-device->measurement_seen;
 f.devices[0]=*device;f.count=1;
}
int main(void) {
 fixture();pqb_session s={0};ble_device device={.address={1,2,3,4,5,6},.address_type=1};
 uint8_t ad[31],length=0;risc_telemetry_field_v1 field={37,RISC_TELEMETRY_BATTERY_PERCENT,"Watch battery"};
 int32_t value=63;assert(telemetry_encode(&field,&value,1,ad,&length));packet(&device,ad,length,100);
 start(&s);assert(pqb_poll(&s));assert(!s.selected);selected(&s,0);
 assert(s.current&&s.last_valid&&s.last_percent==63);
 const uint8_t duplicate[]={6,0x16,0xd2,0xfc,0x40,1,40,6,0x16,0xd2,0xfc,0x40,1,80};
 packet(&device,duplicate,sizeof(duplicate),200);assert(pqb_poll(&s));
 assert(device.measurement_invalid&&!device.reading_count);
 assert(!s.current&&s.last_valid&&s.last_percent==63&&!s.rows[0].battery_valid);
 value=81;assert(telemetry_encode(&field,&value,1,ad,&length));packet(&device,ad,length,300);
 assert(pqb_poll(&s));assert(s.current&&s.last_percent==81);
 /* Presence-only reports do not refresh the measurement timestamp. */
 const uint8_t flags[]={2,1,6};packet(&device,flags,sizeof(flags),10400);
 assert(pqb_poll(&s));assert(!s.current&&s.last_percent==81);
 /* Same observed address, two battery fields: consumer rejects ambiguity. */
 risc_telemetry_field_v1 fields[]={{37,RISC_TELEMETRY_BATTERY_PERCENT,"A"},{38,RISC_TELEMETRY_BATTERY_PERCENT,"B"}};
 int32_t values[]={12,34};assert(telemetry_encode(fields,values,2,ad,&length));packet(&device,ad,length,10500);
 assert(pqb_poll(&s));assert(!s.current&&s.last_percent==81&&!s.rows[0].battery_valid);
 assert(pqb_close(&s));assert(!s.current&&s.last_percent==81);
 puts("Actual codec/parser/session: explicit selection, duplicate AD rejection, recovery, age and multiple battery fields passed");
 return 0;
}
