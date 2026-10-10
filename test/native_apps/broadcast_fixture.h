/* Optional strict service double for real native-controller integration. */
#ifdef PORTABLE_BLE_BROADCAST
#include "TelemetryBroadcastV1.h"
static unsigned fixture_broadcast_steps,fixture_broadcast_pauses;
static bool fixture_broadcast_live,fixture_broadcast_pause_fail;
static bool fixture_broadcast_step(void *c,bool allow,const telemetry_broadcast_policy_v1 *p) {
 (void)c;(void)allow;io();assert(p&&p->struct_size==sizeof(*p)&&!p->enabled);fixture_broadcast_live=allow&&p->enabled&&p->settings_valid&&p->radios_allowed;++fixture_broadcast_steps;return true;
}
static bool fixture_broadcast_pause(void*c){(void)c;io();++fixture_broadcast_pauses;if(fixture_broadcast_pause_fail)return false;fixture_broadcast_live=false;return true;}
static bool fixture_broadcast_status(void*c,telemetry_broadcast_status_v1*out){(void)c;io();*out=(telemetry_broadcast_status_v1){.struct_size=sizeof(*out),.state=TELEMETRY_BROADCAST_OFF};return true;}
static int32_t fixture_broadcast_enumerate(void*c,uint32_t i,risc_telemetry_field_v1*out){(void)c;(void)i;(void)out;io();return 0;}
static int32_t fixture_broadcast_read(void*c,uint32_t i,int32_t*out){(void)c;(void)i;(void)out;io();return 0;}
static const telemetry_broadcast_v1 fixture_broadcast_api={1,sizeof(fixture_broadcast_api),NULL,fixture_broadcast_step,fixture_broadcast_pause,fixture_broadcast_status,fixture_broadcast_enumerate,fixture_broadcast_read};
#endif
