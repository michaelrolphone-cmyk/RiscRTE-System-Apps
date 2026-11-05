/* Real shared alarm controller, deterministic scheduler, real modal presenter. */
extern const risc_driver_v2 *alarm_driver_get(uint32_t);
static bool alarm_fixture;
static const risc_driver_v2 *alarm_driver;
static alarm_status_v1 fixture_alarm={.api_version=1,.struct_size=sizeof(fixture_alarm),.state=ALARM_STATE_READY};
static unsigned fixture_alarm_acks,fixture_alarm_delay;
static alarm_token_v1 fixture_alarm_acknowledged;
static int32_t fixture_alarm_status(void*c,alarm_status_v1*out){(void)c;edge("alarm-status");assert(!radio_id||wm.service);*out=fixture_alarm;return ALARM_OK;}
static int32_t fixture_alarm_step(void*c){
 (void)c;edge("alarm-step");assert(!radio_id||wm.service);
 if(fixture_alarm.state==ALARM_STATE_DISMISSING&&fixture_alarm_delay&&!--fixture_alarm_delay){
  fixture_alarm.state=ALARM_STATE_READY;memset(&fixture_alarm.occurrence,0,sizeof(fixture_alarm.occurrence));
 }
 return ALARM_OK;
}
static int32_t fixture_alarm_refresh(void*c){(void)c;edge("alarm-refresh");assert(!radio_id||wm.service);return ALARM_OK;}
static int32_t fixture_alarm_ack(void*c,const alarm_token_v1*t){
 (void)c;edge("alarm-ack");assert(!radio_id||wm.service);fixture_alarm_acknowledged=*t;fixture_alarm_acks++;
 if(memcmp(t,&fixture_alarm.occurrence,sizeof(*t)))return ALARM_STALE;
 fixture_alarm.state=ALARM_STATE_DISMISSING;fixture_alarm_delay=3;return ALARM_PENDING;
}
static int32_t fixture_alarm_sleep(void*c,alarm_sleep_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t fixture_alarm_stop(void*c){(void)c;edge("alarm-stop");return ALARM_OK;}
static const alarm_service_v1 fixture_scheduler={1,sizeof(fixture_scheduler),NULL,
 fixture_alarm_status,fixture_alarm_step,fixture_alarm_refresh,fixture_alarm_ack,fixture_alarm_sleep,fixture_alarm_stop};
static void alarm_fixture_start(void){
 alarm_driver=alarm_driver_get(2);assert(alarm_driver);
 risc_provider_dependency_v1 deps[]={{"ui.scene",1,api},{"alarm.service",1,&fixture_scheduler}};
 assert(alarm_driver->start(deps,2));
}
