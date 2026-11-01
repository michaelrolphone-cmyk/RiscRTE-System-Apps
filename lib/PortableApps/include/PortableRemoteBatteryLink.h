#pragma once
#include "RiscBluetoothSensorLinkV1.h"
#include <string.h>
/* Pure validation of copied evidence. UNKNOWN never implies NOT_CONNECTED.
 * Selection is exact address/type; no name/nearest-peer matching. */
static inline bool pqb_link_sample_valid(const risc_ble_sensor_link_state_v1 *s,
 uint64_t token,const uint8_t address[6],uint8_t type) {
 if(!s||!address||s->struct_size!=sizeof(*s)||s->state>RISC_BLE_LINK_CONNECTED||
    s->reserved||s->address_type!=type||memcmp(s->address,address,6))return false;
 if(s->state==RISC_BLE_LINK_UNKNOWN)return !s->generation&&!s->observed_ms;
 return token&&s->generation==token&&s->observed_ms!=UINT64_MAX;
}
static inline const char *pqb_link_text(uint32_t state) {
 return state==RISC_BLE_LINK_CONNECTED?"WATCH LINK: CONNECTED TO RECEIVER":
  state==RISC_BLE_LINK_NOT_CONNECTED?"WATCH LINK: NOT CONNECTED TO RECEIVER":
  "WATCH LINK: CONNECTION UNKNOWN";
}
