/* Production native KV facade with an already-active boot advertiser. */
#define main unused_native_settings_main
#include "portable_native_time_settings_test.c"
#undef main
static unsigned probe_reads,probe_writes;
static int32_t probe_get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *used) {
 assert(!fixture_broadcast_live);++probe_reads;return kv_get(c,key,out,capacity,used);
}
static int32_t probe_put(void *c,const char *key,const void *bytes,uint32_t size) {
 (void)c;(void)key;(void)bytes;(void)size;assert(!fixture_broadcast_live);io();++probe_writes;return RISC_KEY_VALUE_OK;
}
int main(int argc,char **argv) {
 assert(argc==2);bool fail=atoi(argv[1])!=0;test_name="broadcast-storage";zone_id="UTC";configure_records();
 assert(app_module_init()==0);in_main=true;assert(t5_app_get_api(1));assert(settings_store);
 custody_kv_slot *slot=settings_store->context;const risc_key_value_v1 *original=slot->source;
 risc_key_value_v1 probe=*original;probe.get=probe_get;probe.put=probe_put;slot->source=&probe;
 unsigned pauses=fixture_broadcast_pauses;fixture_broadcast_live=true;fixture_broadcast_pause_fail=fail;
 uint8_t bytes[4]={0};uint32_t used=0;
 int32_t result=settings_store->get(settings_store->context,PORTABLE_TIME_FORMAT_KEY,bytes,4,&used);
 assert(fixture_broadcast_pauses==pauses+1);
 if(fail) {
  assert(result==RISC_KEY_VALUE_CONTEXT&&retained&&barriers==1&&!probe_reads);
  unsigned calls=provider_calls;
  assert(settings_store->get(settings_store->context,PORTABLE_TIME_FORMAT_KEY,bytes,4,&used)==RISC_KEY_VALUE_CONTEXT);
  assert(settings_store->put(settings_store->context,PORTABLE_TIME_FORMAT_KEY,bytes,4)==RISC_KEY_VALUE_CONTEXT);
  in_main=false;in_fini=true;app_module_fini();assert(provider_calls==calls&&!probe_writes);
 } else {
  assert(result==RISC_KEY_VALUE_NOT_FOUND&&!fixture_broadcast_live&&probe_reads==1);
  fixture_broadcast_live=true;assert(settings_store->put(settings_store->context,PORTABLE_TIME_FORMAT_KEY,bytes,4)==RISC_KEY_VALUE_OK);
  assert(!fixture_broadcast_live&&probe_writes==1);slot->source=original;
  pauses=fixture_broadcast_pauses;fixture_broadcast_live=true;
  assert(broadcast_tick());assert(fixture_broadcast_pauses==pauses+1&&!fixture_broadcast_live);
  assert(!broadcast_client.grant.api&&!broadcast_client.storage.api&&!broadcast_policy_io);
  in_main=false;in_fini=true;app_module_fini();assert(!live&&!frames&&!subscriptions);
 }
 printf("Native storage broadcast guard %s: pause-before-KV, own-policy recursion and terminal cleanup PASS\n",fail?"retained":"healthy");
}
