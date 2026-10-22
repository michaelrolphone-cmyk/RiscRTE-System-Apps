#include "PortableRemoteBattery.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only the fake capability below is invoked. No radio or storage implementation
 * is linked, so this fixture cannot change settings or produce RF activity. */
static struct {
 bool acquire_ok,open_ok,poll_ok,status_ok,device_ok,close_ok,release_ok;
 bool acquire_empty,acquire_ids_only,mutate_failed_release;
 uint32_t grant_size,status_size,state,count,failed_device;
 bool restore_failed,missing_cleanup,closed,restore_on_close;
 bool close_status_failed,close_status_truncated;
 uint64_t open_token;
 const void *acquire_api;
 risc_ble_sensor_device_v1 devices[PQB_MAX_CANDIDATES];
 unsigned acquires,opens,polls,statuses,device_calls,closes,releases;
 char order[1024];size_t order_size;
} f;
static risc_bluetooth_sensors_v1 api;
static risc_runtime_api_v1 runtime;
static void record(char c) {
 if(f.order_size+1<sizeof(f.order)){f.order[f.order_size++]=c;f.order[f.order_size]=0;}
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
 record('A');f.acquires++;
 assert(!strcmp(name,RISC_BLUETOOTH_SENSORS_CAPABILITY));assert(version==1);assert(instance==0);
 assert(g->struct_size==sizeof(*g) && !g->api && !g->slot && !g->generation);
 if(!f.acquire_empty) {
  g->struct_size=f.grant_size;g->slot=93;g->generation=47;
  g->api=f.acquire_ids_only?NULL:f.acquire_api;
 }
 return f.acquire_ok;
}
static bool release(risc_runtime_capability_v1 *g) {
 record('R');f.releases++;
 if(!f.acquire_empty) {
  assert(g->slot==93 && g->generation==47);
  assert(g->api==(f.acquire_ids_only?NULL:f.acquire_api));
  assert(g->struct_size==f.grant_size);
 }
 if(f.release_ok || f.mutate_failed_release)memset(g,0,sizeof(*g));
 return f.release_ok;
}
static bool open_scan(void *context,uint64_t *token) {
 assert(context==&f);record('O');f.opens++;
 assert(*token==0);f.closed=false;*token=f.open_token;return f.open_ok;
}
static bool poll_scan(void *context,uint64_t token,uint32_t maximum) {
 assert(context==&f);record('P');f.polls++;
 assert(token==f.open_token && token);assert(maximum && maximum<=16);
 return f.poll_ok;
}
static bool scan_status(void *context,risc_ble_sensor_status_v1 *out) {
 assert(context==&f);record('S');f.statuses++;
 assert(out->struct_size==sizeof(*out));
 bool ok=f.status_ok && !(f.closed && f.close_status_failed);
 if(ok)*out=(risc_ble_sensor_status_v1){
  .struct_size=f.closed && f.close_status_truncated?1:f.status_size,
  .state=f.closed?RISC_BLE_SENSOR_COMPLETE:f.state,.count=f.count,
  .cleanup_pending=!f.closed && !f.missing_cleanup,.restore_failed=f.restore_failed};
 return ok;
}
static bool scan_device(void *context,uint32_t i,risc_ble_sensor_device_v1 *out) {
 assert(context==&f);record('D');f.device_calls++;
 assert(i<PQB_MAX_CANDIDATES);
 if(!f.device_ok && i==f.failed_device)return false;
 *out=f.devices[i];return true;
}
static bool close_scan(void *context,uint64_t token) {
 assert(context==&f);record('C');f.closes++;
 assert(token==f.open_token && token);
 if(f.close_ok){f.closed=true;if(f.restore_on_close)f.restore_failed=true;}
 return f.close_ok;
}
static void fixture(void) {
 memset(&f,0,sizeof(f));
 f.acquire_ok=f.open_ok=f.poll_ok=f.status_ok=f.device_ok=f.close_ok=f.release_ok=true;
 f.open_token=UINT64_C(0x123456789abcdef0);f.grant_size=sizeof(risc_runtime_capability_v1);
 f.status_size=sizeof(risc_ble_sensor_status_v1);f.state=RISC_BLE_SENSOR_SCANNING;
 api=(risc_bluetooth_sensors_v1){1,sizeof(api),&f,open_scan,poll_scan,scan_status,scan_device,close_scan};
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
 f.acquire_api=&api;
}
static void measurement(unsigned i,uint8_t id,uint8_t type,int32_t value) {
 assert(i<PQB_MAX_CANDIDATES);
 risc_ble_sensor_device_v1 *d=&f.devices[i];memset(d,0,sizeof(*d));
 d->address[0]=id;d->address[1]=0x12;d->address[2]=0x34;d->address[3]=0x56;
 d->address[4]=0x78;d->address[5]=0x90;d->address_type=type;
 memcpy(d->name,"Watch",6);d->has_battery=true;d->battery=(uint8_t)value;
 d->reading_count=1;d->readings[0]=(risc_ble_reading_v1){RISC_TELEMETRY_BATTERY_PERCENT,value};
 d->reports=1;d->measurement_seen=0;d->measurement_age_ms=0;
 if(f.count<i+1)f.count=i+1;
}
static void start(pqb_session *s) {
 assert(pqb_init(s));assert(pqb_start(s,&runtime,true));
 assert(s->active && !s->current && !s->retained);
}
static void selected(pqb_session *s,unsigned i) {
 assert(pqb_select(s,f.devices[i].address,f.devices[i].address_type));assert(s->selected);
}
static unsigned calls(void) {
 return f.acquires+f.opens+f.polls+f.statuses+f.device_calls+f.closes+f.releases;
}
static void no_rf(void) {
 fixture();pqb_session s={0};pqb_candidate rows[2];uint8_t address[6]={0};
 assert(pqb_init(&s));assert(s.status==PQB_IDLE);assert(!s.last_valid);
 assert(pqb_poll(&s));assert(pqb_close(&s));assert(pqb_reset(&s));
 assert(!pqb_select(&s,address,0));assert(!pqb_candidates(&s,rows,2));
 assert(!pqb_start(&s,&runtime,false));assert(s.status==PQB_DISABLED);
 assert(calls()==0);assert(pqb_poll(&s));assert(calls()==0);
 assert(!pqb_start(&s,NULL,true));assert(s.status==PQB_UNAVAILABLE);assert(calls()==0);
 assert(!pqb_init(NULL));assert(!pqb_reset(NULL));assert(!pqb_poll(NULL));
 assert(!pqb_start(NULL,&runtime,true));assert(!pqb_close(NULL));
 assert(!pqb_select(NULL,address,0));assert(!pqb_candidates(NULL,rows,2));
}
static void selection_and_cache(void) {
 fixture();pqb_session s={0};measurement(0,1,0,0);measurement(1,2,1,100);
 start(&s);assert(pqb_poll(&s));assert(!s.selected && !s.last_valid && !s.current);
 assert(s.status==PQB_SCANNING && s.count==2);selected(&s,0);
 assert(s.last_valid && s.last_percent==0 && s.current && s.status==PQB_CURRENT);
 unsigned before=calls();selected(&s,1);assert(calls()==before);
 assert(s.last_percent==100 && s.current);
 uint8_t unknown[6]={8};assert(!pqb_select(&s,unknown,1));assert(s.last_percent==100);
 assert(!pqb_select(&s,NULL,0));assert(!pqb_select(&s,f.devices[1].address,0));
 assert(!pqb_start(&s,&runtime,true));assert(!pqb_init(&s));assert(!pqb_reset(&s));
 assert(calls()==before);
 assert(pqb_close(&s));assert(!s.current && !s.active && s.last_percent==100);
 assert(s.status==PQB_COMPLETE && s.selected && s.last_valid);
 before=calls();assert(pqb_poll(&s));assert(pqb_close(&s));assert(calls()==before);
 assert(pqb_start(&s,&runtime,true));assert(!s.current && s.last_percent==100 && s.selected);
 assert(s.status==PQB_WAITING && s.count==0);
 f.count=0;assert(pqb_poll(&s));assert(!s.current && s.last_percent==100 && s.status==PQB_WAITING);
 measurement(0,2,0,21); /* Same address but changed type must not rebind. */
 assert(pqb_poll(&s));assert(!s.current && s.last_percent==100 && s.status==PQB_WAITING);
 measurement(0,3,1,21);memcpy(f.devices[0].name,"Watch",6);
 assert(pqb_poll(&s));assert(!s.current && s.last_percent==100);
 selected(&s,0);assert(s.last_percent==21 && s.current);
 assert(pqb_close(&s));assert(pqb_reset(&s));assert(!s.selected && !s.last_valid);
}
static void validity_and_staleness(void) {
 fixture();pqb_session s={0};measurement(0,1,0,56);start(&s);assert(pqb_poll(&s));selected(&s,0);
 risc_ble_sensor_device_v1 good=f.devices[0];
 for(unsigned invalid=0;invalid<10;invalid++) {
  f.devices[0]=good;
  switch(invalid) {
   case 0:f.devices[0].encrypted=true;break;
   case 1:f.devices[0].measurement_invalid=true;break;
   case 2:f.devices[0].measurement_partial=true;break;
   case 3:f.devices[0].reading_count=0;break;
   case 4:f.devices[0].reading_count=RISC_BLE_MAX_READINGS+1;break;
   case 5:f.devices[0].readings[0].value=-1;break;
   case 6:f.devices[0].readings[0].value=101;break;
   case 7:f.devices[0].readings[0].metric=RISC_TELEMETRY_VOLTAGE_MV;break;
   case 8:f.devices[0].reading_count=2;f.devices[0].readings[1]=good.readings[0];break;
   case 9:f.devices[0].reading_count=2;f.devices[0].readings[1]=(risc_ble_reading_v1){RISC_TELEMETRY_BATTERY_PERCENT,-1};break;
  }
  assert(pqb_poll(&s));assert(!s.current && s.last_valid && s.last_percent==56);
  assert(s.status==PQB_UNAVAILABLE);assert(!pqb_select(&s,good.address,good.address_type));
  pqb_candidate row;assert(pqb_candidates(&s,&row,1)==1);assert(!row.battery_valid);
  assert(row.last_valid && row.last_percent==56);
 }
 f.devices[0]=good;f.devices[0].measurement_age_ms=PQB_FRESH_MS;
 assert(pqb_poll(&s));assert(s.current);
 f.devices[0].measurement_age_ms=PQB_FRESH_MS+1;f.devices[0].readings[0].value=99;
 assert(pqb_poll(&s));assert(!s.current && s.status==PQB_STALE && s.last_percent==56);
 selected(&s,0);assert(!s.current && s.last_percent==56);
 f.devices[0].measurement_age_ms=UINT32_MAX;assert(pqb_poll(&s));assert(!s.current);
 f.devices[0]=good;f.devices[0].reading_count=2;
 f.devices[0].readings[1]=(risc_ble_reading_v1){RISC_TELEMETRY_TEMPERATURE_CENTIC,2300};
 assert(pqb_poll(&s));assert(s.current && s.last_percent==56);
 f.count=0;assert(pqb_poll(&s));assert(!s.current && s.status==PQB_WAITING && s.last_percent==56);
 assert(pqb_close(&s));
}
static void stale_reselection(void) {
 fixture();pqb_session s={0};measurement(0,1,0,50);measurement(1,2,0,75);
 f.devices[1].measurement_age_ms=PQB_FRESH_MS+1;start(&s);assert(pqb_poll(&s));selected(&s,0);
 selected(&s,1);assert(!s.last_valid && !s.current && s.status==PQB_STALE);
 assert(s.last_percent==0);f.devices[1].measurement_age_ms=0;
 assert(pqb_poll(&s));assert(s.current && s.last_valid && s.last_percent==75);
 assert(pqb_close(&s));assert(pqb_start(&s,&runtime,true));
 f.devices[1].encrypted=true;assert(pqb_poll(&s));assert(!s.current && s.last_percent==75);
 assert(pqb_close(&s));
}
static void copies_and_bounds(void) {
 fixture();pqb_session s={0};
 for(unsigned i=0;i<PQB_MAX_CANDIDATES;i++)measurement(i,(uint8_t)i,0,(int)i);
 memset(f.devices[0].name,0xff,sizeof(f.devices[0].name));
 f.devices[0].name[0]='A';f.devices[0].name[1]='\n';f.devices[0].name[2]='\x1b';
 start(&s);assert(pqb_poll(&s));assert(f.device_calls==32 && s.count==32);
 assert(pqb_candidates(&s,NULL,0)==32);assert(pqb_candidates(&s,NULL,2)==32);
 struct {pqb_candidate rows[32];uint32_t fence;} out;
 memset(&out,0xff,sizeof(out));out.fence=0xabcdef12;
 assert(pqb_candidates(&s,out.rows,32)==32 && out.fence==0xabcdef12);
 assert(out.rows[0].name[0]=='A' && out.rows[0].name[1]=='?' && out.rows[0].name[2]=='?');
 assert(out.rows[0].name[31]==0);
 for(unsigned i=0;i<31;i++)assert(out.rows[0].name[i]>=32 && out.rows[0].name[i]<=126);
 pqb_candidate first=out.rows[0];f.devices[0].name[0]='Z';
 assert(!memcmp(&first,&out.rows[0],sizeof(first))); /* No borrowed pointers. */
 assert(pqb_candidates(&s,out.rows,0)==0);assert(pqb_candidates(&s,out.rows,1)==1);
 unsigned previous=f.device_calls;f.count=UINT32_MAX;
 assert(!pqb_poll(&s));assert(s.status==PQB_ERROR && !s.active && !s.current);
 assert(f.device_calls==previous && f.closes==1 && f.releases==1);
}
static void repeated_and_duplicate_rows(void) {
 fixture();pqb_session s={0};measurement(0,1,0,40);start(&s);assert(pqb_poll(&s));selected(&s,0);
 pqb_candidate a,b;assert(pqb_candidates(&s,&a,1)==1);
 for(unsigned i=0;i<100;i++){assert(pqb_poll(&s));assert(s.count==1 && s.current);}
 assert(pqb_candidates(&s,&b,1)==1);assert(!memcmp(&a,&b,sizeof(a)));
 f.devices[0].readings[0].value=88;f.devices[1]=f.devices[0];f.devices[1].readings[0].value=90;f.count=2;
 assert(pqb_poll(&s));assert(s.count==1 && !s.current && s.last_percent==40);
 assert(s.status==PQB_UNAVAILABLE && !pqb_select(&s,f.devices[0].address,0));
 assert(pqb_candidates(&s,&b,1)==1 && b.last_percent==40 && !b.battery_valid);
 f.count=1;f.devices[0].address_type=255;
 assert(pqb_poll(&s));assert(s.count==1 && !s.current && s.status==PQB_WAITING);
 assert(pqb_close(&s));
 for(unsigned i=0;i<50;i++) {
  f.count=0;assert(pqb_start(&s,&runtime,true));assert(s.selected && s.last_percent==40);
  assert(pqb_poll(&s));assert(pqb_close(&s));assert(!s.active && !s.current && !s.retained);
 }
 assert(f.acquires==51 && f.opens==51 && f.closes==51 && f.releases==51);
}
static void cleanup_custody(void) {
 fixture();pqb_session s={0};measurement(0,1,0,67);start(&s);assert(pqb_poll(&s));selected(&s,0);
 f.close_ok=false;uint64_t token=s.token;risc_runtime_capability_v1 grant=s.grant;
 assert(!pqb_close(&s));assert(s.retained && s.status==PQB_RETAINED && !s.current && !s.active);
 assert(s.token==token && s.last_percent==67 && !memcmp(&s.grant,&grant,sizeof(grant)));
 assert(f.closes==1 && !f.releases);unsigned before=calls();
 assert(!pqb_poll(&s));assert(!pqb_start(&s,&runtime,true));assert(!pqb_start(&s,&runtime,false));
 assert(!pqb_init(&s));assert(!pqb_reset(&s));assert(!pqb_select(&s,f.devices[0].address,0));
 pqb_candidate row;assert(pqb_candidates(&s,&row,1)==1);assert(calls()==before);
 assert(!pqb_close(&s));assert(f.closes==2 && !f.releases && s.token==token);
 f.close_ok=true;f.release_ok=false;f.mutate_failed_release=true;
 assert(!pqb_close(&s));assert(s.token==0 && s.retained && s.grant_owned);
 assert(s.grant.slot==93 && s.grant.generation==47 && s.grant.api==&api);
 before=calls();assert(!pqb_poll(&s));assert(!pqb_start(&s,&runtime,true));assert(calls()==before);
 f.release_ok=true;assert(pqb_close(&s));assert(f.closes==3 && f.releases==2);
 assert(!s.retained && !s.grant_owned && !s.grant.api && !s.token);
 assert(s.selected && s.last_valid && s.last_percent==67 && !s.current);
 assert(pqb_start(&s,&runtime,true));assert(pqb_close(&s));
}
static void failed_start(void) {
 for(unsigned mode=0;mode<9;mode++) {
  fixture();pqb_session s={0};assert(pqb_init(&s));
  switch(mode) {
   case 0:f.acquire_ok=false;f.acquire_empty=true;break;
   case 1:f.acquire_ok=false;break;
   case 2:f.acquire_ok=false;f.acquire_ids_only=true;break;
   case 3:f.acquire_ok=false;f.release_ok=false;f.mutate_failed_release=true;break;
   case 4:f.open_ok=false;f.open_token=0;break;
   case 5:f.open_ok=false;break;
   case 6:f.open_ok=false;f.close_ok=false;break;
   case 7:f.open_token=0;break;
   case 8:f.grant_size=1;break;
  }
  assert(!pqb_start(&s,&runtime,true));assert(!s.active && !s.current);
  if(mode==0){assert(!f.opens && !f.closes && !f.releases);assert(!s.retained);}
  if(mode==1 || mode==2 || mode==8){assert(!f.opens && f.releases==1 && !s.retained);}
  if(mode==3){assert(s.retained && s.grant.api==&api && s.grant.slot==93);}
  if(mode==4 || mode==7){assert(f.opens==1 && !f.closes && f.releases==1);}
  if(mode==5){assert(!strcmp(f.order,"AOCSR") && !s.retained && !s.token);}
  if(mode==6){assert(!strcmp(f.order,"AOC") && s.retained && s.token==f.open_token);}
  f.close_ok=f.release_ok=true;assert(pqb_close(&s));assert(!s.retained && !s.token && !s.grant.api);
 }
}
static void polling_failures_and_completion(void) {
 for(unsigned mode=0;mode<10;mode++) {
  fixture();pqb_session s={0};measurement(0,1,0,42);start(&s);assert(pqb_poll(&s));selected(&s,0);
  switch(mode) {
   case 0:f.poll_ok=false;break;
   case 1:f.status_ok=false;break;
   case 2:f.status_size=1;break;
   case 3:f.state=99;break;
   case 4:f.state=RISC_BLE_SENSOR_ERROR;break;
   case 5:f.state=RISC_BLE_SENSOR_IDLE;break;
   case 6:f.restore_failed=true;break;
   case 7:f.device_ok=false;break;
   case 8:f.state=RISC_BLE_SENSOR_COMPLETE;f.devices[0].readings[0].value=65;break;
   case 9:f.missing_cleanup=true;break;
  }
  unsigned reads=f.device_calls;bool ok=pqb_poll(&s);
  assert(ok==(mode==8));assert(!s.current && !s.active && !s.retained);
  assert(f.closes==1 && f.releases==1);
  assert(s.last_valid && s.last_percent==(mode==8?65:42));
  assert(s.status==(mode==8?PQB_COMPLETE:PQB_ERROR));
  if(mode<7 || mode==9)assert(f.device_calls==reads);
  assert(pqb_close(&s));
 }
 fixture();pqb_session s={0};start(&s);f.poll_ok=false;f.close_ok=false;
 assert(!pqb_poll(&s));assert(s.retained && s.token==f.open_token && !f.releases);
 f.close_ok=true;assert(pqb_close(&s));
}
static void cached_failed_refresh(void) {
 fixture();pqb_session s={0};measurement(0,1,0,31);start(&s);assert(pqb_poll(&s));selected(&s,0);
 assert(pqb_close(&s));uint8_t address[6];memcpy(address,s.selected_address,6);
 unsigned before=calls();assert(!pqb_start(&s,&runtime,false));assert(calls()==before);
 assert(s.status==PQB_DISABLED && s.selected && s.last_valid && s.last_percent==31 && !s.current);
 f.open_ok=false;assert(!pqb_start(&s,&runtime,true));
 assert(s.status==PQB_ERROR && s.selected && s.last_valid && s.last_percent==31 && !s.current);
 assert(!memcmp(address,s.selected_address,6));f.open_ok=true;
 assert(pqb_start(&s,&runtime,true));assert(!s.current && s.last_percent==31);
 f.devices[0].readings[0].value=32;assert(pqb_poll(&s));assert(s.current && s.last_percent==32);
 assert(pqb_close(&s));f.acquire_ok=false;assert(!pqb_start(&s,&runtime,true));
 assert(s.status==PQB_UNAVAILABLE && s.selected && s.last_percent==32 && !s.current);
 assert(pqb_reset(&s));
 /* A separate session never inherits selection, even with identical data. */
 f.acquire_ok=true;assert(pqb_start(&s,&runtime,true));assert(pqb_poll(&s));
 assert(!s.selected && !s.current && !s.last_valid);assert(pqb_close(&s));
}
static void restoration_outcomes(void) {
 for(unsigned mode=0;mode<5;mode++) {
  fixture();pqb_session s={0};measurement(0,1,0,73);start(&s);assert(pqb_poll(&s));selected(&s,0);
  assert(!s.restoration_failed);
  if(mode==0 || mode==1 || mode==4)f.restore_on_close=true;
  if(mode==2)f.close_status_failed=true;
  if(mode==3)f.close_status_truncated=true;
  if(mode==4)f.release_ok=false;
  unsigned status_reads=f.statuses;
  if(mode==1){f.state=RISC_BLE_SENSOR_COMPLETE;assert(!pqb_poll(&s));}
  else assert(pqb_close(&s)==(mode!=4));
  assert(f.statuses==status_reads+(mode==1?2u:1u));
  assert(f.closes==1 && f.releases==1 && !s.token && !s.active && !s.current);
  assert(s.restoration_failed && s.last_valid && s.last_percent==73 && s.selected);
  if(mode==4) {
   assert(s.retained && s.status==PQB_RETAINED && s.grant.api==&api);
   status_reads=f.statuses;f.release_ok=true;assert(pqb_close(&s));
   assert(f.statuses==status_reads && f.closes==1 && f.releases==2);
  }
  assert(!s.retained && !s.grant_owned && !s.grant.api && s.status==PQB_ERROR);
  unsigned before=calls();assert(pqb_close(&s));assert(pqb_poll(&s));
  assert(calls()==before && s.restoration_failed && s.status==PQB_ERROR);
  assert(!pqb_start(&s,&runtime,false));assert(calls()==before && s.restoration_failed);
  /* Only a new explicitly permitted attempt clears this warning. No automatic
   * scan or Bluetooth-setting call exists in this component. */
  f.restore_failed=f.restore_on_close=f.close_status_failed=f.close_status_truncated=false;
  f.state=RISC_BLE_SENSOR_SCANNING;assert(pqb_start(&s,&runtime,true));
  assert(!s.restoration_failed && s.last_valid && s.last_percent==73 && !s.current);
  f.devices[0].readings[0].value=74;assert(pqb_poll(&s));assert(s.current && s.last_percent==74);
  assert(pqb_close(&s));assert(!s.restoration_failed && s.status==PQB_COMPLETE && !s.current);
 }
 /* Failed open still owns its returned token; its cleanup can reveal the same
  * restoration gap and must not overwrite it with a generic completion. */
 fixture();pqb_session s={0};f.open_ok=false;f.restore_on_close=true;
 assert(!pqb_start(&s,&runtime,true));
 assert(!s.retained && !s.token && !s.grant.api && s.restoration_failed && s.status==PQB_ERROR);
 assert(!strcmp(f.order,"AOCSR"));
}
static void api_shapes(void) {
 for(unsigned mode=0;mode<10;mode++) {
  fixture();pqb_session s={0};assert(pqb_init(&s));
  switch(mode) {
   case 0:api.api_version=2;break;
   case 1:api.struct_size=sizeof(api)-1;break;
   case 2:api.open=NULL;break;
   case 3:api.poll=NULL;break;
   case 4:api.status=NULL;break;
   case 5:api.device=NULL;break;
   case 6:api.close=NULL;break;
   case 7:f.acquire_api=NULL;break;
   case 8:f.acquire_empty=true;break;
   case 9:f.grant_size=sizeof(risc_runtime_capability_v1)-1;break;
  }
  assert(!pqb_start(&s,&runtime,true));assert(s.status==PQB_UNAVAILABLE);
  assert(!f.opens && f.releases==1 && !s.retained);
 }
 for(unsigned mode=0;mode<4;mode++) {
  fixture();pqb_session s={0};
  switch(mode) {
   case 0:runtime.api_version=2;break;
   case 1:runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE-1;break;
   case 2:runtime.acquire=NULL;break;
   case 3:runtime.release=NULL;break;
  }
  assert(!pqb_start(&s,&runtime,true));assert(calls()==0);
 }
 /* Real short allocations: sanitizers catch any suffix read before size checks. */
 fixture();pqb_session s={0};uint32_t *header=malloc(2*sizeof(*header));assert(header);
 header[0]=1;header[1]=2*sizeof(*header);
 assert(!pqb_start(&s,(const risc_runtime_api_v1 *)header,true));assert(calls()==0);
 f.acquire_api=header;assert(!pqb_start(&s,&runtime,true));assert(f.releases==1 && !f.opens);
 free(header);
 fixture();api.struct_size=8;f.release_ok=false;f.mutate_failed_release=true;
 assert(!pqb_start(&s,&runtime,true));assert(s.retained && s.grant.api==&api);
 f.release_ok=true;assert(pqb_close(&s));
}
int main(void) {
 no_rf();selection_and_cache();validity_and_staleness();stale_reselection();
 copies_and_bounds();repeated_and_duplicate_rows();cleanup_custody();
 failed_start();polling_failures_and_completion();cached_failed_refresh();restoration_outcomes();api_shapes();
 puts("remote battery session: all fake-provider cases passed");return 0;
}
