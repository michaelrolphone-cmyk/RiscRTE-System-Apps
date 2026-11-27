/* Include after radio/storage fixture globals, before rt_acquire.
 * The real app's shared bridge must pause this provider before RF/storage. */
static bool broadcast_live;
static unsigned broadcast_pauses,broadcast_steps;
static bool fixture_broadcast_pause(void*c){
 (void)c;edge("broadcast-pause");assert(!radio_id&&!wm.service&&!storage_pending(&wm));
 broadcast_live=false;broadcast_pauses++;return true;
}
static bool fixture_broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){
 (void)c;edge("broadcast-step");assert(!radio_id&&!wm.service&&!storage_pending(&wm));
 assert(!held&&!presenting&&p&&p->struct_size==sizeof(*p));
 broadcast_live=allow&&p->enabled&&p->settings_valid&&p->radios_allowed;broadcast_steps++;return true;
}
static bool fixture_broadcast_status(void*c,telemetry_broadcast_status_v1*s){
 (void)c;*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.state=broadcast_live?TELEMETRY_BROADCAST_LIVE:TELEMETRY_BROADCAST_PAUSED};return true;
}
static int32_t fixture_broadcast_enumerate(void*c,uint32_t i,risc_telemetry_field_v1*f){(void)c;(void)i;(void)f;return 0;}
static int32_t fixture_broadcast_read(void*c,uint32_t i,int32_t*v){(void)c;(void)i;(void)v;return 0;}
static const telemetry_broadcast_v1 fixture_broadcast={1,sizeof(fixture_broadcast),NULL,
 fixture_broadcast_step,fixture_broadcast_pause,fixture_broadcast_status,
 fixture_broadcast_enumerate,fixture_broadcast_read};
