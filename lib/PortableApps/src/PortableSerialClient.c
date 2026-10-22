#include "PortableSerialClient.h"
#include <string.h>

static bool result(portable_serial_client *c,int code) {
 if(c)c->result=code;
 return code==PSC_OK;
}
static bool retained(portable_serial_client *c) {
 c->cleanup_required=true;c->halted=true;c->connected=false;
 return result(c,PSC_RETAINED);
}
static bool object(portable_serial_client *c) {
 if(!c)return false;
 if(c->self!=c || !c->initialized)return result(c,PSC_INVALID);
 if(c->halted || c->cleanup_required)return false;
 return true;
}
static bool table_valid(const risc_stream_client_v1 *s) {
 return s->api_version==RISC_STREAM_CLIENT_API_V1 && s->struct_size>=sizeof(*s) &&
        s->context && s->open && s->call && s->close && s->read && s->write && s->info;
}
/* Verify invocation before dereferencing a borrowed capability table. Runtime
 * also checks every stream operation, including stale/forged handle use. */
static bool current(portable_serial_client *c) {
 if(!object(c))return false;
 risc_stream_client_v1 now={.struct_size=sizeof(now)};
 if(!c->runtime->stream_client(&now) || !table_valid(&now) ||
    now.context!=c->streams.context) {
  c->halted=true;
  if(c->grant.api || c->session)return retained(c);
  return result(c,PSC_DENIED);
 }
 return true;
}
static bool configuration(const t5_serial_config_t *v) {
 return v && v->baud_rate>=300 && v->baud_rate<=3000000 &&
        v->data_bits>=5 && v->data_bits<=8 && v->parity<=T5_SERIAL_PARITY_SPACE &&
        (v->stop_bits==1 || v->stop_bits==2) && v->flow_control==T5_SERIAL_FLOW_NONE;
}
static bool status_valid(int rc) {return rc>=PSC_RETAINED && rc<=PSC_EOF;}
static bool control_status(int rc) {return rc>=PSC_RETAINED && rc<=PSC_OK;}
static bool grant_valid(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g) && g->slot && g->generation && g->api;
}
static bool grant_empty(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g) && !g->slot && !g->generation && !g->api;
}
static bool release_grant(portable_serial_client *c) {
 if(!c->grant.api)return true;
 risc_runtime_capability_v1 copy=c->grant;
 if(!c->runtime->release(&copy) || !grant_empty(&copy))return retained(c);
 c->grant=copy;c->inventory_count=0;c->inventory_loaded=false;
 return true;
}
static void discard(portable_serial_client *c) {c->queued=c->sent=0;}
/* No retry even for BUSY/CLOSED: these do not prove physical teardown. Keep
 * original tokens as evidence if either close or release is uncertain. */
static bool close_with_budget(portable_serial_client *c,uint32_t budget,int final) {
 if(c->session) {
  if(!budget)return retained(c);
  int rc=c->streams.close(c->streams.context,c->session,budget);
  if(rc!=PSC_OK)return retained(c);
  c->session=0;c->rx_stream=c->tx_stream=0;c->connected=false;
 }
 if(!release_grant(c))return false;
 c->connected=false;c->device=c->generation=0;c->dtr=c->rts=false;
 c->inventory_count=0;c->inventory_loaded=false;discard(c);
 return result(c,final);
}
static uint32_t operation_budget(const portable_serial_client *c) {
 /* Reserve the remainder for at most one checked close. Even a one-ms budget
  * is valid; if it is consumed, uncertain custody must be retained. */
 return (c->control_budget_ms+1u)/2u;
}
static int call(portable_serial_client *c,risc_serial_stream_call_v1 *request,uint32_t budget) {
 uint32_t actual=0;
 int rc=c->streams.call(c->streams.context,c->session,request,sizeof(*request),budget,0,0,&actual);
 if(rc==PSC_RETAINED || !control_status(rc) || actual) {
  (void)retained(c);return PSC_RETAINED;
 }
 return rc;
}
static risc_serial_stream_config_v1 stream_config(const t5_serial_config_t *v) {
 risc_serial_stream_config_v1 out={v->baud_rate,v->data_bits,v->parity,v->stop_bits,v->flow_control};
 return out;
}
static void add(uint64_t *total,uint32_t n) {
 *total=UINT64_MAX-*total<n?UINT64_MAX:*total+n;
}
bool portable_serial_init_instance(portable_serial_client *c,const risc_runtime_api_v1 *rt,uint64_t instance) {
 if(!c)return false;
 memset(c,0,sizeof(*c));c->self=c;c->control_budget_ms=RISC_STREAM_CONTROL_MAX_MS_V1;
 c->instance=instance;c->grant.struct_size=sizeof(c->grant);
#ifndef PORTABLE_SERIAL_STREAMS
 (void)rt;return result(c,PSC_UNSUPPORTED);
#else
 if(!rt || rt->api_version!=RISC_RUNTIME_API_V1 ||
    rt->struct_size<RISC_RUNTIME_STREAM_CLIENT_V1_SIZE || !rt->acquire || !rt->release || !rt->stream_client)
  return result(c,PSC_UNSUPPORTED);
 c->streams.struct_size=sizeof(c->streams);
 if(!rt->stream_client(&c->streams) || !table_valid(&c->streams))return result(c,PSC_UNSUPPORTED);
 c->runtime=rt;c->initialized=true;c->stream_mode=true;
 return result(c,PSC_OK);
#endif
}
bool portable_serial_init(portable_serial_client *c,const risc_runtime_api_v1 *rt) {
 return portable_serial_init_instance(c,rt,0);
}
bool portable_serial_set_control_budget(portable_serial_client *c,uint32_t ms) {
 if(!object(c))return false;
 if(!ms || ms>RISC_STREAM_CONTROL_MAX_MS_V1)return result(c,PSC_INVALID);
 c->control_budget_ms=ms;return result(c,PSC_OK);
}
bool portable_serial_refresh(portable_serial_client *c) {
 if(!current(c))return false;
 if(c->session)return result(c,PSC_BUSY);
 if(!c->grant.api) {
  risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
  bool ok=c->runtime->acquire(T5_SERIAL_PORT_CAPABILITY,RISC_SERIAL_PORT_API_V1,c->instance,&g);
  c->grant=g;
  if(!ok) {
   if(!grant_empty(&g))return retained(c);
   /* Runtime's getter is provider-free and is unavailable after a retention
    * fence. Only a live same-invocation table proves an empty refusal clean.
    * This is not a provider retry or cleanup attempt. */
   risc_stream_client_v1 now={.struct_size=sizeof(now)};
   if(!c->runtime->stream_client(&now) || !table_valid(&now) || now.context!=c->streams.context)
    return retained(c);
   return result(c,PSC_DENIED);
  }
  if(!grant_valid(&g))return retained(c);
 }
 if(!current(c))return false;
 const risc_serial_port_inventory_v1 *api=c->grant.api;
 uint32_t header[2];memcpy(header,api,sizeof(header));
 if(header[0]!=RISC_SERIAL_PORT_API_V1 || header[1]<sizeof(*api) || !api->snapshot) {
  (void)close_with_budget(c,c->control_budget_ms,PSC_UNSUPPORTED);return false;
 }
 risc_serial_device_v1 snapshot[RISC_SERIAL_INVENTORY_MAX_DEVICES]={0};
 size_t count=RISC_SERIAL_INVENTORY_MAX_DEVICES;
 if(!api->snapshot(snapshot,&count) || count>RISC_SERIAL_INVENTORY_MAX_DEVICES)return result(c,PSC_IO);
 for(size_t i=0;i<count;i++) {
  if(!snapshot[i].provider_device || !snapshot[i].generation || snapshot[i].transport>RISC_SERIAL_TRANSPORT_IP)
   return result(c,PSC_IO);
  for(size_t b=0;b<sizeof(snapshot[i].reserved);b++)if(snapshot[i].reserved[b])return result(c,PSC_IO);
  for(size_t j=0;j<i;j++)if(snapshot[i].provider_device==snapshot[j].provider_device)return result(c,PSC_IO);
 }
 memcpy(c->inventory,snapshot,sizeof(snapshot));c->inventory_count=count;c->inventory_loaded=true;
 return result(c,PSC_OK);
}
bool portable_serial_open(portable_serial_client *c,size_t index,const t5_serial_config_t *config) {
 if(!object(c))return false;
 if(!configuration(config) || index>=RISC_SERIAL_INVENTORY_MAX_DEVICES || c->control_budget_ms<2u)
  return result(c,PSC_INVALID);
 if(c->connected || c->session)return result(c,PSC_BUSY);
 if(!current(c))return false;
 if(!c->inventory_loaded && !portable_serial_refresh(c))return false;
 if(index>=c->inventory_count)return result(c,PSC_DISCONNECTED);
 risc_serial_device_v1 selected=c->inventory[index];
 risc_serial_stream_open_v1 request={RISC_SERIAL_STREAM_REQUEST_V1,sizeof(request),
     selected.provider_device,selected.generation,stream_config(config)};
 risc_stream_opened_v1 opened={.struct_size=sizeof(opened)};
 /* The pinned broker can spend its entire budget again on one internal
  * grant-rollback close. Allocate both parts here, without changing its ABI. */
 int rc=c->streams.open(c->streams.context,&c->grant,&request,sizeof(request),c->control_budget_ms/2u,&opened);
 if(rc==PSC_RETAINED)return retained(c);
 if(!control_status(rc) || opened.struct_size!=sizeof(opened) || opened.reserved ||
    (rc!=PSC_OK && (opened.session || opened.rx || opened.tx)))return retained(c);
 if(rc!=PSC_OK) {
  if(!release_grant(c))return false;
  return result(c,rc);
 }
 if(!opened.session || !opened.rx || !opened.tx || opened.rx==opened.tx ||
    opened.session==opened.rx || opened.session==opened.tx)return retained(c);
 c->session=opened.session;c->rx_stream=opened.rx;c->tx_stream=opened.tx;
 c->device=selected.provider_device;c->generation=selected.generation;c->config=*config;
 c->connected=true;c->dtr=c->rts=false;discard(c);
 return result(c,PSC_OK);
}
static bool live(portable_serial_client *c) {
 if(!current(c))return false;
 return c->connected && c->session?true:result(c,PSC_CLOSED);
}
static bool control_result(portable_serial_client *c,int rc) {
 if(rc==PSC_RETAINED)return false;
 if(rc==PSC_OK)return result(c,PSC_OK);
 /* A failed framing/line change is not silently retried on an uncertain
  * physical configuration. Close once within the remaining control budget. */
 (void)close_with_budget(c,c->control_budget_ms-operation_budget(c),rc);
 return false;
}
bool portable_serial_configure(portable_serial_client *c,const t5_serial_config_t *config) {
 if(!object(c))return false;
 if(!configuration(config))return result(c,PSC_INVALID);
 if(!live(c))return false;
 risc_serial_stream_call_v1 request={.api_version=RISC_SERIAL_STREAM_REQUEST_V1,
     .struct_size=sizeof(request),.operation=RISC_SERIAL_STREAM_CONFIGURE};
 request.value.config=stream_config(config);
 if(!control_result(c,call(c,&request,operation_budget(c))))return false;
 c->config=*config;return true;
}
bool portable_serial_control(portable_serial_client *c,bool dtr,bool rts) {
 if(!live(c))return false;
 risc_serial_stream_call_v1 request={.api_version=RISC_SERIAL_STREAM_REQUEST_V1,
     .struct_size=sizeof(request),.operation=RISC_SERIAL_STREAM_CONTROL_LINES};
 request.value.lines.dtr=dtr;request.value.lines.rts=rts;
 if(!control_result(c,call(c,&request,operation_budget(c))))return false;
 c->dtr=dtr;c->rts=rts;return true;
}
bool portable_serial_queue(portable_serial_client *c,const char *line,uint32_t now) {
 if(!object(c))return false;
 if(!line)return result(c,PSC_INVALID);
 if(!c->connected)return result(c,PSC_CLOSED);
 if(c->queued)return result(c,PSC_BUSY);
 size_t length=0;
 while(length<=PSC_QUEUE_CAPACITY-2u && line[length])++length;
 if(length>PSC_QUEUE_CAPACITY-2u)return result(c,PSC_LIMIT);
 if(length)memcpy(c->queue,line,length);
 c->queue[length++]='\r';c->queue[length++]='\n';
 c->queued=length;c->sent=0;c->queued_at=now;
 return result(c,PSC_OK);
}
static bool transfer_result(portable_serial_client *c,int rc,uint32_t n,uint32_t bound) {
 if(rc==PSC_RETAINED || !status_valid(rc) || n>bound || (rc!=PSC_OK && n) || (rc==PSC_OK && !n))
  return retained(c);
 if(rc==PSC_OK || rc==PSC_AGAIN || rc==PSC_BUSY)return true;
 (void)close_with_budget(c,c->control_budget_ms-operation_budget(c),rc);return false;
}
size_t portable_serial_tick(portable_serial_client *c,void *rx,size_t capacity,uint32_t now) {
 if(!object(c))return 0;
 if(!rx && capacity){(void)result(c,PSC_INVALID);return 0;}
 if(!live(c))return 0;
 risc_serial_stream_call_v1 check={.api_version=RISC_SERIAL_STREAM_REQUEST_V1,
     .struct_size=sizeof(check),.operation=RISC_SERIAL_STREAM_CHECK_DEVICE};
 int rc=call(c,&check,operation_budget(c));
 if(rc==PSC_RETAINED)return 0;
 if(rc!=PSC_OK) {
  if(rc==PSC_IO || rc==PSC_AGAIN || rc==PSC_BUSY){(void)result(c,rc);return 0;}
  (void)close_with_budget(c,c->control_budget_ms-operation_budget(c),rc);return 0;
 }
 bool timeout=c->queued && (uint32_t)(now-c->queued_at)>=PSC_STALL_MS;
 if(timeout)discard(c);
 if(c->queued) {
  uint32_t n=0,length=(uint32_t)(c->queued-c->sent);
  rc=c->streams.write(c->streams.context,c->tx_stream,c->queue+c->sent,length,&n);
  if(!transfer_result(c,rc,n,length))return 0;
  if(n) {c->sent+=n;add(&c->tx_bytes,n);c->queued_at=now;}
  if(c->sent==c->queued)discard(c);
 }
 uint32_t n=0,bound=capacity>PSC_CHUNK?PSC_CHUNK:(uint32_t)capacity;
 if(bound) {
  rc=c->streams.read(c->streams.context,c->rx_stream,rx,bound,&n);
  if(!transfer_result(c,rc,n,bound))return 0;
  add(&c->rx_bytes,n);
 }
 (void)result(c,timeout?PSC_TIMEOUT:PSC_OK);return n;
}
void portable_serial_cancel(portable_serial_client *c) {
 if(!object(c))return;
 discard(c);(void)result(c,PSC_CANCELLED);
}
bool portable_serial_close(portable_serial_client *c) {
 if(!current(c))return false;
 return close_with_budget(c,c->control_budget_ms,PSC_OK);
}
