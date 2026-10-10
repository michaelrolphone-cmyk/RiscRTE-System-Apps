/* Actual selected GT911 source; only its physical bus/platform are host fakes. */
#define main clock_gt911_provider_suite_main
#include X4_GT911_FIXTURE
#undef main
static uint64_t observer;
static unsigned emitted[6];
unsigned clock_gt911_emitted(unsigned kind){assert(kind<6);return emitted[kind];}
const risc_touch_api_v1 *clock_gt911_start(void) {
 driver=t5_driver_get(2);assert(driver);api=driver->capability;
 power=risc_touch_power(api);assert(power&&start());observer=api->subscribe(NULL);assert(observer);return api;
}
void clock_gt911_report(const risc_touch_snapshot_v1 *value,uint64_t when) {
 packet(value->contact_count,0,0,!!(value->buttons&RISC_TOUCH_BUTTON_PRIMARY));
 for(unsigned i=0;i<value->contact_count;i++) {
  assert(value->contacts[i].id>0);
  wire_point(i,value->contacts[i].id-1u,value->contacts[i].x,value->contacts[i].y);
 }
 assert(when>=time_ms);time_ms=when;assert(api->poll(NULL,1));
 risc_touch_event_v1 event;int status;
 while((status=api->next(NULL,observer,&event))==1) {
  assert(event.kind<6);emitted[event.kind]++;
  printf("provider_edge seq=%llu timestamp=%llu kind=%u id=%u x=%u y=%u\n",
   (unsigned long long)event.sequence,(unsigned long long)event.timestamp_ms,
   event.kind,event.id,event.x,event.y);
 }
 assert(!status);
}
void clock_gt911_stop(void) {assert(api->unsubscribe(NULL,observer));done(0);}
