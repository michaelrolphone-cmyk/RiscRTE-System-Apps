#include "PortableRemoteBattery.h"
#include <string.h>

static bool has_grant(const risc_runtime_capability_v1 *g) {
 return g->api || g->slot || g->generation;
}
static bool has_custody(const pqb_session *s) {
 return s->active || s->retained || s->token || s->grant_owned || has_grant(&s->grant);
}
bool pqb_init(pqb_session *s) {
 if(!s || has_custody(s))return false;
 *s=(pqb_session){0};return true;
}
bool pqb_reset(pqb_session *s) { return pqb_init(s); }
static bool runtime_valid(const risc_runtime_api_v1 *rt) {
 return rt && rt->api_version==RISC_RUNTIME_API_V1 &&
  rt->struct_size>=RISC_RUNTIME_CAPABILITIES_V1_SIZE && rt->acquire && rt->release;
}
static bool api_valid(const risc_bluetooth_sensors_v1 *api) {
 return api && api->api_version==1 && api->struct_size>=sizeof(*api) &&
  api->open && api->poll && api->status && api->device && api->close;
}
static bool retained(pqb_session *s) {
 s->active=false;s->current=false;s->retained=true;s->status=PQB_RETAINED;
 return false;
}
bool pqb_close(pqb_session *s) {
 if(!s)return false;
 bool was_active=s->active,was_retained=s->retained;
 s->active=false;s->current=false;
 if(s->token) {
  if(!s->api || !s->api->close || !s->api->close(s->api->context,s->token))return retained(s);
  s->token=0;
  /* close=true establishes RF-safe cleanup, not restoration of the prior ON
   * setting. Read that separate outcome while the capability is still owned.
   * A missing/malformed outcome is unconfirmed, never implicit permission to
   * enable Bluetooth. Release remains safe even if this status read fails. */
  risc_ble_sensor_status_v1 outcome={.struct_size=sizeof(outcome)};
  if(!s->api->status(s->api->context,&outcome) || outcome.struct_size<sizeof(outcome) ||
     outcome.cleanup_pending || outcome.restore_failed)s->restoration_failed=true;
 }
 if(s->grant_owned || has_grant(&s->grant)) {
  if(!s->runtime || !s->runtime->release)return retained(s);
  /* Even a misbehaving failed release must not erase our only custody record. */
  risc_runtime_capability_v1 grant=s->grant;
  if(!s->runtime->release(&grant))return retained(s);
  s->grant=(risc_runtime_capability_v1){0};s->grant_owned=false;
 }
 s->runtime=NULL;s->api=NULL;s->retained=false;
 if(s->restoration_failed)s->status=PQB_ERROR;
 else if(was_active || was_retained)s->status=PQB_COMPLETE;
 return true;
}
static bool finish(pqb_session *s,pqb_status status) {
 if(!pqb_close(s))return false;
 if(s->restoration_failed){s->status=PQB_ERROR;return false;}
 s->status=status;return status!=PQB_ERROR && status!=PQB_UNAVAILABLE;
}
bool pqb_start(pqb_session *s,const risc_runtime_api_v1 *rt,bool allowed) {
 if(!s || has_custody(s))return false;
 s->current=false;
 if(!allowed){s->status=PQB_DISABLED;return false;}
 if(!runtime_valid(rt)){s->status=PQB_UNAVAILABLE;return false;}
 s->restoration_failed=false;
 /* Selection and its last reading survive. Candidate rows belong to one scan. */
 s->count=0;memset(s->rows,0,sizeof(s->rows));
 memset(s->row_seen,0,sizeof(s->row_seen));
 memset(s->row_duplicate,0,sizeof(s->row_duplicate));
 memset(s->row_status,0,sizeof(s->row_status));
 s->runtime=rt;s->grant=(risc_runtime_capability_v1){.struct_size=sizeof(s->grant)};
 bool acquired=rt->acquire(RISC_BLUETOOTH_SENSORS_CAPABILITY,1,0,&s->grant);
 s->grant_owned=acquired || has_grant(&s->grant);
 if(!acquired || s->grant.struct_size<sizeof(s->grant) || !api_valid(s->grant.api)) {
  (void)finish(s,PQB_UNAVAILABLE);return false;
 }
 s->api=s->grant.api;
 bool opened=s->api->open(s->api->context,&s->token);
 if(!opened || !s->token) {
  /* A refused open can still return a live cleanup token. */
  (void)finish(s,PQB_ERROR);return false;
 }
 s->active=true;s->status=s->selected?PQB_WAITING:PQB_SCANNING;
 return true;
}
static bool identity(const uint8_t a[6],uint8_t type,const pqb_candidate *row) {
 return row->address_type==type && !memcmp(a,row->address,6);
}
static int find_row(const pqb_session *s,const uint8_t address[6],uint8_t type) {
 for(uint32_t i=0;i<s->count;i++)if(identity(address,type,&s->rows[i]))return (int)i;
 return -1;
}
static void copy_name(char out[32],const char in[32]) {
 memset(out,0,32);uint32_t i=0;
 for(;i<31 && in[i];i++) {
  unsigned char c=(unsigned char)in[i];out[i]=c>=32 && c<=126?(char)c:'?';
 }
 out[i]=0;
}
static pqb_status battery(const risc_ble_sensor_device_v1 *d,uint8_t *percent) {
 if(d->encrypted || d->measurement_invalid || d->measurement_partial ||
    d->reading_count>RISC_BLE_MAX_READINGS)return PQB_UNAVAILABLE;
 unsigned count=0;int32_t value=0;
 for(uint32_t i=0;i<d->reading_count;i++)if(d->readings[i].metric==RISC_TELEMETRY_BATTERY_PERCENT) {
  count++;value=d->readings[i].value;
 }
 if(count!=1 || value<0 || value>100)return PQB_UNAVAILABLE;
 *percent=(uint8_t)value;
 return d->measurement_age_ms<=PQB_FRESH_MS?PQB_CURRENT:PQB_STALE;
}
static void reflect(pqb_session *s) {
 s->current=false;
 if(!s->selected){s->status=PQB_SCANNING;return;}
 int index=find_row(s,s->selected_address,s->selected_address_type);
 if(index<0 || !s->row_seen[index]){s->status=PQB_WAITING;return;}
 pqb_candidate *row=&s->rows[index];
 s->status=s->row_status[index];
 if(s->status==PQB_CURRENT && !s->row_duplicate[index]) {
  s->last_valid=row->last_valid;s->last_percent=row->last_percent;
  s->current=s->active && s->last_valid;
 }
}
bool pqb_poll(pqb_session *s) {
 if(!s || s->retained)return false;
 if(!s->active)return true; /* Inactive/closed polling never starts RF. */
 s->current=false;
 if(!s->api->poll(s->api->context,s->token,PQB_POLL_EVENTS))return finish(s,PQB_ERROR);
 risc_ble_sensor_status_v1 status={.struct_size=sizeof(status)};
 if(!s->api->status(s->api->context,&status) || status.struct_size<sizeof(status) ||
    status.state>RISC_BLE_SENSOR_ERROR || status.state==RISC_BLE_SENSOR_ERROR ||
    status.state==RISC_BLE_SENSOR_IDLE || !status.cleanup_pending || status.restore_failed)return finish(s,PQB_ERROR);
 if(status.count>PQB_MAX_CANDIDATES)return finish(s,PQB_ERROR);
 bool previous_valid[PQB_MAX_CANDIDATES]={0};
 uint8_t previous_percent[PQB_MAX_CANDIDATES]={0};
 for(uint32_t i=0;i<s->count;i++) {
  previous_valid[i]=s->rows[i].last_valid;previous_percent[i]=s->rows[i].last_percent;
  s->row_seen[i]=false;s->row_duplicate[i]=false;
  s->rows[i].battery_valid=false;s->row_status[i]=PQB_WAITING;
 }
 for(uint32_t i=0;i<status.count;i++) {
  risc_ble_sensor_device_v1 device={0};
  if(!s->api->device(s->api->context,i,&device))return finish(s,PQB_ERROR);
  /* BLE address types are public/random and the two identity-address types. */
  if(device.address_type>3)continue;
  int index=find_row(s,device.address,device.address_type);
  if(index<0) {
   if(s->count==PQB_MAX_CANDIDATES)continue;
   index=(int)s->count++;
   s->rows[index]=(pqb_candidate){.address_type=device.address_type};
   memcpy(s->rows[index].address,device.address,6);
  }
  pqb_candidate *row=&s->rows[index];
  if(s->row_seen[index]) {
   /* A repeated identity in one table is ambiguous; never choose a winner. */
   s->row_duplicate[index]=true;row->battery_valid=false;
   row->last_valid=previous_valid[index];row->last_percent=previous_percent[index];
   s->row_status[index]=PQB_UNAVAILABLE;continue;
  }
  s->row_seen[index]=true;copy_name(row->name,device.name);
  uint8_t percent=0;s->row_status[index]=battery(&device,&percent);
  row->battery_valid=s->row_status[index]==PQB_CURRENT || s->row_status[index]==PQB_STALE;
  if(s->row_status[index]==PQB_CURRENT){row->last_valid=true;row->last_percent=percent;}
 }
 reflect(s);
 if(status.state==RISC_BLE_SENSOR_COMPLETE)return finish(s,PQB_COMPLETE);
 return true;
}
bool pqb_select(pqb_session *s,const uint8_t address[6],uint8_t address_type) {
 if(!s || !address || s->retained)return false;
 int index=find_row(s,address,address_type);
 if(index<0 || !s->row_seen[index] || !s->rows[index].battery_valid || s->row_duplicate[index])return false;
 bool same=s->selected && s->selected_address_type==address_type && !memcmp(s->selected_address,address,6);
 s->selected=true;s->selected_address_type=address_type;
 memcpy(s->selected_address,address,6);s->current=false;
 if(!same){s->last_valid=false;s->last_percent=0;}
 if(s->rows[index].last_valid) {
  s->last_valid=true;s->last_percent=s->rows[index].last_percent;
 }
 if(s->active)reflect(s);
 return true;
}
uint32_t pqb_candidates(const pqb_session *s,pqb_candidate *out,uint32_t capacity) {
 if(!s)return 0;
 if(!out)return s->count;
 uint32_t count=s->count<capacity?s->count:capacity;
 for(uint32_t i=0;i<count;i++) {
  /* Explicit member copies keep any implementation padding deterministic. */
  memset(&out[i],0,sizeof(out[i]));
  memcpy(out[i].address,s->rows[i].address,sizeof(out[i].address));
  out[i].address_type=s->rows[i].address_type;
  memcpy(out[i].name,s->rows[i].name,sizeof(out[i].name));
  out[i].battery_valid=s->rows[i].battery_valid;
  out[i].last_valid=s->rows[i].last_valid;out[i].last_percent=s->rows[i].last_percent;
 }
 return count;
}
