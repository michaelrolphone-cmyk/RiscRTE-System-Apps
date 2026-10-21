#include "PortableSerialClient.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const t5_serial_config_t config={9600,8,T5_SERIAL_PARITY_NONE,1,T5_SERIAL_FLOW_NONE};
static risc_runtime_api_v1 runtime;
static risc_stream_client_v1 streams;
static risc_serial_port_inventory_v1 provider;
static risc_serial_device_v1 devices[RISC_SERIAL_INVENTORY_MAX_DEVICES];
static size_t device_count;
static uint64_t context,expected_instance,selected_device,selected_generation;
static unsigned getters,acquires,releases,snapshots,opens,calls,closes,reads,writes,spent;
static unsigned fenced_calls;
static bool fenced,acquire_ok,release_ok,snapshot_ok,get_ok,acquire_retained,open_rollback;
static int open_rc,call_rc,close_rc,read_rc,write_rc;
static unsigned bad_grant,bad_open,bad_count,reply_bytes;
static uint32_t tx_step,rx_step;
static unsigned char echo[4096];
static size_t echo_size,echo_read;

static void enter(void) {if(fenced)++fenced_calls;assert(!fenced);}
static void budget(uint32_t ms) {assert(ms>=1 && ms<=1000);spent+=ms;assert(spent<=1000);}
static void retain_if(int rc) {if(rc==PSC_RETAINED)fenced=true;}
static bool snapshot(risc_serial_device_v1 *out,size_t *count) {
 enter();++snapshots;assert(out && count && *count==RISC_SERIAL_INVENTORY_MAX_DEVICES);
 memcpy(out,devices,sizeof(devices));*count=device_count;return snapshot_ok;
}
static bool acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *out) {
 enter();++acquires;assert(!strcmp(name,"serial.port") && api==1 && instance==expected_instance);
 if(acquire_ok || bad_grant)*out=(risc_runtime_capability_v1){sizeof(*out),1,3,&provider};
 if(bad_grant==1)out->slot=0;
 if(bad_grant==2)out->generation=0;
 if(bad_grant==3)out->struct_size=0;
 if(bad_grant==4)out->api=0;
 if(acquire_retained)get_ok=false;
 return acquire_ok;
}
static bool release(risc_runtime_capability_v1 *g) {
 enter();++releases;assert(g->api==&provider);
 if(!release_ok)return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static int32_t open_stream(uint64_t ctx,const risc_runtime_capability_v1 *grant,
 const void *bytes,uint32_t size,uint32_t ms,risc_stream_opened_v1 *out) {
 enter();++opens;budget(ms);assert(ctx==context && grant->api==&provider);
 if(open_rollback)budget(ms);
 assert(bytes && size==sizeof(risc_serial_stream_open_v1));
 const risc_serial_stream_open_v1 *r=bytes;
 assert(r->api_version==1 && r->struct_size==sizeof(*r));
 selected_device=r->provider_device;selected_generation=r->device_generation;
 assert(selected_device && selected_generation);
 if(open_rc==PSC_OK || bad_open)*out=(risc_stream_opened_v1){sizeof(*out),0,100,101,102};
 if(bad_open==1)out->session=0;
 if(bad_open==2)out->rx=out->tx;
 if(bad_open==3)out->reserved=1;
 if(bad_open==4)out->struct_size=0;
 if(bad_open==5)out->tx=out->session;
 retain_if(open_rc);return open_rc;
}
static int32_t call_stream(uint64_t ctx,uint64_t session,const void *bytes,uint32_t size,
 uint32_t ms,void *reply,uint32_t cap,uint32_t *actual) {
 enter();++calls;budget(ms);assert(ctx==context && session==100);
 assert(bytes && size==sizeof(risc_serial_stream_call_v1) && !reply && !cap && actual);
 const risc_serial_stream_call_v1 *r=bytes;
 assert(r->api_version==1 && r->struct_size==sizeof(*r) && !r->reserved);
 *actual=reply_bytes;retain_if(call_rc);
  if(call_rc!=PSC_OK)return call_rc;
 if(r->operation==RISC_SERIAL_STREAM_CHECK_DEVICE) {
  for(size_t i=0;i<8;i++)assert(!r->value.zero[i]);
  if(!snapshot_ok || device_count>RISC_SERIAL_INVENTORY_MAX_DEVICES)return PSC_IO;
  bool present=false;
  for(size_t i=0;i<device_count;i++) {
   if(!devices[i].provider_device || !devices[i].generation || devices[i].transport>RISC_SERIAL_TRANSPORT_IP)return PSC_IO;
   for(size_t j=0;j<7;j++)if(devices[i].reserved[j])return PSC_IO;
   for(size_t j=0;j<i;j++)if(devices[i].provider_device==devices[j].provider_device)return PSC_IO;
   if(devices[i].provider_device==selected_device && devices[i].generation==selected_generation)present=true;
  }
  return present?PSC_OK:PSC_DISCONNECTED;
 }
 if(r->operation==RISC_SERIAL_STREAM_CONTROL_LINES) {
  assert(r->value.lines.dtr<=1 && r->value.lines.rts<=1);
  for(size_t i=0;i<6;i++)assert(!r->value.lines.reserved[i]);
 } else assert(r->operation==RISC_SERIAL_STREAM_CONFIGURE);
 return PSC_OK;
}
static int32_t close_stream(uint64_t ctx,uint64_t session,uint32_t ms) {
 enter();++closes;budget(ms);assert(ctx==context && session==100);
 retain_if(close_rc);return close_rc;
}
static int32_t write_stream(uint64_t ctx,uint64_t stream,const void *data,uint32_t length,uint32_t *n) {
 enter();++writes;assert(ctx==context && stream==102 && data && length && length<=PSC_CHUNK);
 *n=0;
  if(write_rc!=PSC_OK){if(bad_count)*n=1;retain_if(write_rc);return write_rc;}
 *n=length<tx_step?length:tx_step;
 if(bad_count){*n=bad_count==1?length+1:0;return PSC_OK;}
 if(!*n)return PSC_AGAIN;
 assert(echo_size+*n<=sizeof(echo));memcpy(echo+echo_size,data,*n);echo_size+=*n;return PSC_OK;
}
static int32_t read_stream(uint64_t ctx,uint64_t stream,void *data,uint32_t capacity,uint32_t *n) {
 enter();++reads;assert(ctx==context && stream==101 && data && capacity && capacity<=PSC_CHUNK);
 *n=0;
  if(read_rc!=PSC_OK){if(bad_count)*n=1;retain_if(read_rc);return read_rc;}
 size_t available=echo_size-echo_read;
 *n=available<capacity?(uint32_t)available:capacity;
  if(*n>rx_step)*n=rx_step;
 if(bad_count){*n=bad_count==1?capacity+1:0;return PSC_OK;}
 if(!*n)return PSC_AGAIN;
 memcpy(data,echo+echo_read,*n);echo_read+=*n;return PSC_OK;
}
static int32_t info(uint64_t ctx,uint64_t stream,risc_stream_client_info_v1 *out) {
 (void)ctx;(void)stream;(void)out;assert(!"unused info");return PSC_UNSUPPORTED;
}
static bool getter(risc_stream_client_v1 *out) {
 enter();++getters;assert(out && out->struct_size==sizeof(*out));
 if(!get_ok)return false;
 *out=streams;return true;
}
static void reset(void) {
 getters=acquires=releases=snapshots=opens=calls=closes=reads=writes=spent=fenced_calls=0;
 context=42;expected_instance=0;selected_device=selected_generation=0;
 fenced=acquire_retained=open_rollback=false;get_ok=acquire_ok=release_ok=snapshot_ok=true;
 open_rc=call_rc=close_rc=read_rc=write_rc=PSC_OK;
 bad_grant=bad_open=bad_count=reply_bytes=0;tx_step=5;rx_step=1;echo_size=echo_read=0;
 memset(devices,0,sizeof(devices));devices[0]=(risc_serial_device_v1){17,9,0,{0}};device_count=1;
 provider=(risc_serial_port_inventory_v1){.discovery={.serial={.api_version=1,.struct_size=sizeof(provider)}},.snapshot=snapshot};
 streams=(risc_stream_client_v1){1,sizeof(streams),context,open_stream,call_stream,close_stream,read_stream,write_stream,info};
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.stream_client=getter};
}
static portable_serial_client client;
static void fresh(void) {reset();assert(portable_serial_init(&client,&runtime));}
static void opened(void) {fresh();assert(portable_serial_open(&client,0,&config));spent=0;}
static void closed(void) {spent=0;assert(portable_serial_close(&client));assert(!client.grant.api && !client.session && !client.rx_stream && !client.tx_stream && !client.connected);}
static unsigned io_count(void) {return getters+acquires+releases+snapshots+opens+calls+closes+reads+writes;}
static void stopped(void) {
 assert(client.cleanup_required && client.halted && client.result==PSC_RETAINED);
 unsigned count=io_count();uint8_t b=0;fenced=true;
 assert(!portable_serial_open(&client,0,&config));assert(!portable_serial_refresh(&client));
 assert(!portable_serial_configure(&client,&config));assert(!portable_serial_control(&client,true,true));
 assert(!portable_serial_queue(&client,"x",0));assert(!portable_serial_tick(&client,&b,1,1));
 assert(!portable_serial_close(&client));assert(!portable_serial_set_control_budget(&client,10));
 portable_serial_cancel(&client);assert(io_count()==count && !fenced_calls && client.result==PSC_RETAINED);
}
static void invalid(void) {
 fresh();assert(!portable_serial_init(0,&runtime));assert(!portable_serial_open(0,0,&config));
 assert(!portable_serial_refresh(0));assert(!portable_serial_close(0));portable_serial_cancel(0);
 assert(!portable_serial_init(&client,0));assert(!portable_serial_open(&client,0,&config));
 for(unsigned mode=0;mode<6;mode++) {
  reset();
  if(mode==0)runtime.struct_size=RISC_RUNTIME_STREAM_CLIENT_V1_SIZE-1;
  if(mode==1)runtime.api_version=2;
  if(mode==2)runtime.stream_client=0;
  if(mode==3)runtime.acquire=0;
  if(mode==4)runtime.release=0;
  if(mode==5)get_ok=false;
  assert(!portable_serial_init(&client,&runtime));assert(!acquires);
 }
 for(unsigned mode=0;mode<9;mode++) {
  reset();
  if(mode==0)streams.struct_size--;
  if(mode==1)streams.api_version=2;
  if(mode==2)streams.context=0;
  if(mode==3)streams.open=0;
  if(mode==4)streams.call=0;
  if(mode==5)streams.close=0;
  if(mode==6)streams.read=0;
  if(mode==7)streams.write=0;
  if(mode==8)streams.info=0;
  assert(!portable_serial_init(&client,&runtime));assert(!acquires);
 }
 fresh();unsigned before=io_count();
 assert(!portable_serial_set_control_budget(&client,0));assert(!portable_serial_set_control_budget(&client,1001));
 assert(!portable_serial_open(&client,8,&config));assert(!portable_serial_open(&client,0,0));
 for(unsigned i=0;i<8;i++) {
  t5_serial_config_t bad=config;
  if(i==0)bad.baud_rate=299;
  if(i==1)bad.baud_rate=3000001;
  if(i==2)bad.data_bits=4;
  if(i==3)bad.data_bits=9;
  if(i==4)bad.parity=5;
  if(i==5)bad.stop_bits=0;
  if(i==6)bad.stop_bits=3;
  if(i==7)bad.flow_control=1;
  assert(!portable_serial_open(&client,0,&bad));
 }
 assert(io_count()==before);assert(!portable_serial_queue(&client,"x",0));
 puts("serial client invalid arguments, truncated tables and configuration PASS");
}
static void inventories(void) {
 for(unsigned mode=0;mode<8;mode++) {
  fresh();
  if(mode==0)snapshot_ok=false;
  if(mode==1)device_count=9;
  if(mode==2)devices[0].provider_device=0;
  if(mode==3)devices[0].generation=0;
  if(mode==4)devices[0].transport=9;
  if(mode==5)devices[0].reserved[3]=1;
  if(mode==6){device_count=2;devices[1]=devices[0];}
  if(mode==7){snapshot_ok=false;device_count=0;}
  assert(!portable_serial_open(&client,0,&config));assert(client.result==PSC_IO && !opens && !client.cleanup_required);
  assert(!portable_serial_open(&client,0,&config));assert(client.result==PSC_IO && !opens);closed();
 }
 fresh();device_count=0;assert(!portable_serial_open(&client,0,&config));assert(client.result==PSC_DISCONNECTED && !opens);closed();
 fresh();assert(portable_serial_refresh(&client));devices[0].generation=10;snapshot_ok=false;
 assert(!portable_serial_refresh(&client));assert(client.inventory[0].generation==9 && client.inventory_count==1);closed();
 reset();expected_instance=73;assert(portable_serial_init_instance(&client,&runtime,73));
 devices[1]=(risc_serial_device_v1){18,12,RISC_SERIAL_TRANSPORT_USB,{0}};device_count=2;
 assert(portable_serial_refresh(&client));unsigned before=snapshots;
 assert(portable_serial_open(&client,1,&config));assert(selected_device==18 && selected_generation==12 && snapshots==before);
 assert(!portable_serial_refresh(&client) && client.result==PSC_BUSY);closed();
 puts("serial client complete inventory, unknown recovery and explicit selection PASS");
}
static void retained_paths(void) {
 fresh();acquire_ok=false;acquire_retained=true;assert(!portable_serial_open(&client,0,&config));assert(!releases);stopped();
 for(unsigned i=1;i<=4;i++){fresh();bad_grant=i;assert(!portable_serial_open(&client,0,&config));stopped();}
 fresh();open_rc=PSC_RETAINED;assert(!portable_serial_open(&client,0,&config));assert(!closes && !releases);stopped();
 for(unsigned i=1;i<=5;i++){fresh();bad_open=i;assert(!portable_serial_open(&client,0,&config));assert(!closes && !releases);stopped();}
 fresh();open_rc=PSC_IO;bad_open=1;assert(!portable_serial_open(&client,0,&config));stopped();
 fresh();open_rc=-100;assert(!portable_serial_open(&client,0,&config));stopped();
 for(int rc=PSC_AGAIN;rc<=PSC_EOF;rc++) {
  fresh();open_rc=rc;assert(!portable_serial_open(&client,0,&config));assert(!closes && !releases);stopped();
  opened();call_rc=rc;assert(!portable_serial_configure(&client,&config));assert(!closes && !releases);stopped();
 }
 opened();call_rc=PSC_RETAINED;assert(!portable_serial_configure(&client,&config));assert(!closes && !releases);stopped();
 opened();call_rc=PSC_RETAINED;assert(!portable_serial_control(&client,true,true));stopped();
 opened();reply_bytes=1;assert(!portable_serial_configure(&client,&config));stopped();
 opened();call_rc=-100;assert(!portable_serial_control(&client,true,true));stopped();
 int codes[]={PSC_RETAINED,PSC_IO,PSC_BUSY,PSC_CLOSED};
 for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);i++){
  opened();close_rc=codes[i];assert(!portable_serial_close(&client));assert(closes==1 && !releases);stopped();
 }
 opened();release_ok=false;assert(!portable_serial_close(&client));assert(closes==1 && releases==1);stopped();
 opened();call_rc=PSC_IO;close_rc=PSC_RETAINED;assert(!portable_serial_configure(&client,&config));assert(closes==1 && !releases);stopped();
 puts("serial client retained/malformed open, call, close and release never retry PASS");
}
static void lifecycle(void) {
 fresh();acquire_ok=false;assert(!portable_serial_open(&client,0,&config));
 assert(client.result==PSC_DENIED && !client.cleanup_required && !opens && !releases);
 acquire_ok=true;assert(portable_serial_open(&client,0,&config));closed();
 fresh();assert(portable_serial_set_control_budget(&client,1));unsigned before=io_count();
 assert(!portable_serial_open(&client,0,&config));assert(client.result==PSC_INVALID && before==io_count());
 fresh();open_rc=PSC_LIMIT;open_rollback=true;assert(!portable_serial_open(&client,0,&config));
 assert(spent==1000 && releases==1 && client.result==PSC_LIMIT && !client.cleanup_required);
 fresh();open_rc=PSC_IO;assert(!portable_serial_open(&client,0,&config));assert(client.result==PSC_IO && releases==1 && !closes && !client.cleanup_required);
 spent=0;open_rc=PSC_OK;assert(portable_serial_open(&client,0,&config));closed();
 fresh();provider.discovery.serial.struct_size=sizeof(risc_serial_port_api_v1);
 assert(!portable_serial_open(&client,0,&config));assert(client.result==PSC_UNSUPPORTED && releases==1 && !snapshots);
 fresh();provider.snapshot=0;assert(!portable_serial_open(&client,0,&config));assert(client.result==PSC_UNSUPPORTED && releases==1);
 opened();t5_serial_config_t changed={3000000,5,4,2,0};assert(portable_serial_configure(&client,&changed));
 assert(client.config.baud_rate==3000000 && spent<=1000);spent=0;
 assert(portable_serial_control(&client,true,true) && client.dtr && client.rts);closed();
 opened();call_rc=PSC_IO;assert(!portable_serial_configure(&client,&config));assert(!client.connected && !client.cleanup_required && client.result==PSC_IO && spent==1000 && releases==1);
 opened();call_rc=PSC_IO;assert(!portable_serial_control(&client,true,true));assert(!client.connected && client.result==PSC_IO && spent==1000);
 opened();assert(portable_serial_set_control_budget(&client,1));call_rc=PSC_IO;
 assert(!portable_serial_configure(&client,&config));assert(spent==1 && !closes && !releases);stopped();
 opened();assert(portable_serial_set_control_budget(&client,999));call_rc=PSC_IO;
 assert(!portable_serial_configure(&client,&config));assert(spent==999 && closes==1);
 opened();closed();unsigned count=closes;closed();assert(closes==count);
 spent=0;devices[0].generation=10;assert(portable_serial_open(&client,0,&config));assert(client.generation==10);closed();
 puts("serial client lifecycle, reconfiguration and total 1..1000 ms control budgets PASS");
}
static void stale(void) {
 opened();portable_serial_client copied=client;unsigned before=io_count();
 assert(!portable_serial_close(&copied));assert(!portable_serial_refresh(&copied));
 uint8_t byte;assert(!portable_serial_tick(&copied,&byte,1,0));assert(io_count()==before);closed();
 fresh();context=43;streams.context=context;assert(!portable_serial_open(&client,0,&config));
 assert(client.result==PSC_DENIED && !acquires);assert(!portable_serial_close(&client));
 fresh();assert(portable_serial_refresh(&client));context=43;streams.context=context;
 assert(!portable_serial_refresh(&client));assert(snapshots==1 && !releases);stopped();
 opened();context=43;streams.context=context;assert(!portable_serial_tick(&client,&byte,1,0));assert(!calls && !reads && !writes);stopped();
 opened();get_ok=false;assert(!portable_serial_close(&client));assert(!closes && !releases);stopped();
 puts("serial client copied objects and stale invocation contexts never enter provider PASS");
}
static void transfers(void) {
 opened();assert(portable_serial_queue(&client,"ABCDEFG",0));assert(client.queued==9);
 assert(!portable_serial_queue(&client,"x",0));uint8_t output[600]={0};size_t total=0;
 for(unsigned tick=1;tick<20 && total<9;tick++) {spent=0;total+=portable_serial_tick(&client,output+total,sizeof(output)-total,tick);}
 assert(total==9 && !memcmp(output,"ABCDEFG\r\n",9) && client.tx_bytes==9 && client.rx_bytes==9 && !client.queued);closed();
 opened();char line[PSC_QUEUE_CAPACITY+1];memset(line,'x',sizeof(line));line[PSC_QUEUE_CAPACITY]=0;
 assert(!portable_serial_queue(&client,line,0) && client.result==PSC_LIMIT);
 line[PSC_QUEUE_CAPACITY-2]=0;assert(portable_serial_queue(&client,line,0) && client.queued==PSC_QUEUE_CAPACITY);
 portable_serial_cancel(&client);assert(!client.queued && client.result==PSC_CANCELLED);
 assert(!portable_serial_queue(&client,0,0));assert(portable_serial_queue(&client,"",0));assert(client.queued==2);closed();
 opened();assert(portable_serial_queue(&client,"ABCDEFG",0));assert(!portable_serial_tick(&client,0,0,1));
 assert(client.tx_bytes==5 && client.sent==5);portable_serial_cancel(&client);
 assert(client.tx_bytes==5 && !client.queued && !client.sent && client.result==PSC_CANCELLED);closed();
 opened();assert(portable_serial_queue(&client,"ABCDEFG",UINT32_MAX-10u));spent=0;
 assert(!portable_serial_tick(&client,0,0,UINT32_MAX-9u));assert(client.tx_bytes==5);tx_step=0;
 spent=0;assert(!portable_serial_tick(&client,0,0,1989));assert(client.queued && client.result==PSC_OK);
 spent=0;assert(!portable_serial_tick(&client,0,0,1990));assert(!client.queued && client.result==PSC_TIMEOUT && client.tx_bytes==5);closed();
 opened();client.tx_bytes=client.rx_bytes=UINT64_MAX-1;assert(portable_serial_queue(&client,"a",0));tx_step=rx_step=512;
 assert(portable_serial_tick(&client,output,sizeof(output),1)==3);
 assert(client.tx_bytes==UINT64_MAX && client.rx_bytes==UINT64_MAX);closed();
 opened();unsigned before=io_count();assert(!portable_serial_tick(&client,0,1,0));assert(io_count()==before && client.result==PSC_INVALID);closed();
 puts("serial client partial transfers, bounded CRLF queue, cancellation, wrap deadline and saturation PASS");
}
static void terminals(void) {
 uint8_t output[PSC_CHUNK];
 for(unsigned mode=0;mode<2;mode++) {
  opened();assert(portable_serial_queue(&client,"ABCDEFG",0));
  if(mode==0)snapshot_ok=false;else devices[0].generation=0;
  assert(!portable_serial_tick(&client,output,sizeof(output),1));
  assert(client.connected && client.generation==9 && client.queued==9 && !writes && !reads && !closes && client.result==PSC_IO);
  snapshot_ok=true;devices[0].generation=9;spent=0;assert(portable_serial_tick(&client,output,sizeof(output),2)==1);closed();
 }
 for(unsigned mode=0;mode<2;mode++) {
  opened();
  if(mode==0)device_count=0;else devices[0].generation=10;
  assert(!portable_serial_tick(&client,output,sizeof(output),1));
  assert(!client.connected && client.result==PSC_DISCONNECTED && closes==1 && releases==1 && !writes && !reads);
 }
 int terminals[]={PSC_EOF,PSC_IO,PSC_DISCONNECTED,PSC_CANCELLED,PSC_CLOSED,PSC_DENIED};
 for(unsigned i=0;i<sizeof(terminals)/sizeof(*terminals);i++) {
  opened();read_rc=terminals[i];assert(!portable_serial_tick(&client,output,sizeof(output),1));
  assert(!client.connected && !client.cleanup_required && client.result==terminals[i] && closes==1 && releases==1);
 }
 opened();call_rc=PSC_RETAINED;assert(!portable_serial_tick(&client,output,sizeof(output),1));assert(!reads && !writes && !closes);stopped();
 opened();read_rc=PSC_RETAINED;assert(!portable_serial_tick(&client,output,sizeof(output),1));assert(!closes);stopped();
 opened();write_rc=PSC_RETAINED;assert(portable_serial_queue(&client,"x",0));
 assert(!portable_serial_tick(&client,output,sizeof(output),1));assert(!reads && !closes);stopped();
 for(unsigned mode=0;mode<4;mode++) {
  opened();bad_count=(mode%2)+1;
  if(mode<2)assert(portable_serial_queue(&client,"x",0));
  assert(!portable_serial_tick(&client,output,sizeof(output),1));assert(!closes);stopped();
 }
 opened();bad_count=1;write_rc=PSC_AGAIN;assert(portable_serial_queue(&client,"x",0));
 assert(!portable_serial_tick(&client,output,sizeof(output),1));stopped();
 puts("serial client detach/reconnect, EOF, malformed counts and retained transfers PASS");
}
int main(void) {
 invalid();inventories();retained_paths();lifecycle();stale();transfers();terminals();
 puts("portable serial client all local checks PASS");return 0;
}
