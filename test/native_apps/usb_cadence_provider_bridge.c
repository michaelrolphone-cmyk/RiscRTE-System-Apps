/* Production provider/TinyUSB with packet timing at the DCD boundary. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main owner_fixture_main
#define risc_msc_transport_poll baseline_transport_poll
#include USB_OWNER_TEST_SOURCE
#undef main
#undef risc_msc_transport_poll
#pragma GCC diagnostic pop
extern const risc_storage_volume_api_v1_export_prepare *directory_sd_start(void);
extern void directory_sd_frozen(void),directory_sd_finish(void);
extern unsigned directory_sd_reads(void),directory_sd_writes(void);
extern uint32_t directory_sd_hash(void);
static const risc_storage_volume_api_v1_export_prepare *sd_actual;
static uint64_t clock_us,wire_due,host_due,started_us,finished_us;
static uint32_t host_gap=200,packet_us=65;
static unsigned transactions,host_polls,packet_count,phase,sectors_done,packet_offset;
static unsigned entries,long_entries,clusters,work_phase;
static uint32_t partition,fat,data,cluster,spc,lba;static uint16_t count;
static uint8_t response[128*512];static bool configured_model,enabled,request_eject,done;
static unsigned before_reads,before_writes,export_reads;static uint32_t before_hash;
static uint32_t u32(const uint8_t*p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t u16(const uint8_t*p){return p[0]|((uint16_t)p[1]<<8);}
static int32_t media_begin(void*c,uint64_t*t){int32_t r=sd_actual->begin_prepare(c,t);assert(r==RISC_STORAGE_EXPORT_PREPARING);sd_owned=true;return r;}
static int32_t media_end(void*c,uint64_t t){assert(!usb_live);int32_t r=sd_actual->base.export_end(c,t);if(!r)sd_owned=false;return r;}
static void set_time(void){now_ms=clock_us/1000;}
uint64_t bench_ms(void){set_time();return now_ms;}
void bench_yield(uint32_t n){clock_us+=(uint64_t)n*1000;set_time();}
void bench_cpu(void){clock_us+=5;set_time();}
static void next_read(void){
 if(work_phase==0){lba=0;count=1;work_phase=1;return;}
 if(work_phase==1){assert(response[510]==0x55&&response[511]==0xaa);partition=u32(response+454);lba=partition;count=1;work_phase=2;return;}
 if(work_phase==2){assert(u16(response+11)==512);spc=response[13];fat=partition+u16(response+14);data=fat+response[16]*u32(response+36);cluster=u32(response+44);work_phase=3;}
 else if(work_phase==3){bool end=false;for(unsigned i=0;i<count*512;i+=32){if(!response[i]){end=true;break;}if(response[i]==0xe5)continue;if(response[i+11]==15)++long_entries;else if(!(response[i+11]&8))++entries;}
  if(end){assert(entries==322&&long_entries>=640&&clusters>32);work_phase=5;lba=data;count=128;return;}
  lba=fat+cluster/128;count=1;work_phase=4;return;
 }else if(work_phase==4){cluster=u32(response+(cluster%128)*4)&0xfffffff;assert(cluster>=2&&cluster<0xffffff8u);work_phase=3;}
 else if(work_phase==5){lba=100000;count=1;work_phase=6;return;}
 else if(work_phase==6){lba=131071;count=1;work_phase=7;return;}
 else {request_eject=true;count=0;return;}
 assert(work_phase==3);++clusters;lba=data+(cluster-2)*spc;count=(uint16_t)spc;
}
static bool ready_model(void){
 ++clock_us;set_time();if(tud_task_event_ready())return true;
 if(done)return false;
 if(phase==0)return ep[1][0].pending&&clock_us>=host_due;
 if(!ep[1][1].pending)return false;
 if(!wire_due)wire_due=clock_us+packet_us;
 return clock_us>=wire_due;
}
static void hardware_model(void){
 if(done||tud_task_event_ready())return;
 if(phase==0){
  if(!ep[1][0].pending||clock_us<host_due)return;
  msc_cbw_t cbw={.signature=MSC_CBW_SIGNATURE,.tag=++tag,.total_bytes=(uint32_t)count*512,.dir=request_eject?0:0x80,.lun=0,.cmd_len=request_eject?6:10};
  cbw.command[0]=request_eject?0x1b:0x28;
  if(request_eject)cbw.command[4]=2;
  else{cbw.command[2]=lba>>24;cbw.command[3]=lba>>16;cbw.command[4]=lba>>8;cbw.command[5]=lba;cbw.command[7]=count>>8;cbw.command[8]=count;}
  complete(1,&cbw,sizeof(cbw));phase=request_eject?2:1;sectors_done=packet_offset=0;wire_due=0;++packet_count;return;
 }
 if(!ep[1][1].pending||clock_us<wire_due)return;
 ++packet_count;wire_due=clock_us+packet_us;
 if(phase==1){
  assert(ep[1][1].length==512);packet_offset+=64;if(packet_offset<512)return;
  memcpy(response+sectors_done*512,ep[1][1].buffer,512);complete(0x81,NULL,512);packet_offset=0;++sectors_done;if(sectors_done==count)phase=2;
 }else{
  assert(ep[1][1].length==13);msc_csw_t csw;memcpy(&csw,ep[1][1].buffer,13);assert(csw.signature==MSC_CSW_SIGNATURE&&csw.tag==tag&&csw.status==0&&!csw.data_residue);
  complete(0x81,NULL,13);++transactions;
  if(request_eject){done=true;finished_us=clock_us;export_reads=directory_sd_reads()-before_reads;return;}
  directory_sd_frozen();next_read();phase=0;host_due=clock_us+host_gap;wire_due=0;
 }
}
bool risc_msc_transport_poll(void){
 assert(usb_live);if(!enabled)return baseline_transport_poll();++host_polls;
 (void)risc_msc_owner_pump(ready_model,hardware_model,pump_event);return healthy;
}
const risc_usb_device_msc_api_v1*bench_provider(unsigned gap){
 host_gap=gap;sd_actual=directory_sd_start();volume=*sd_actual;volume.begin_prepare=media_begin;volume.base.export_end=media_end;
 const risc_driver_v2*d=t5_driver_get(2);api=d->capability;risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clock_api},{RISC_USB_PHY_RESOURCE_CAPABILITY,1,&phy},{"storage.volume",1,&volume}};assert(d->start(deps,3));return api;
}
void bench_token(uint64_t t){token=t;}
void bench_after_poll(uint32_t state){
 if(!configured_model&&state==RISC_USB_MSC_WAITING){configure();configured_model=true;enabled=true;started_us=clock_us;before_reads=directory_sd_reads();before_writes=directory_sd_writes();before_hash=directory_sd_hash();next_read();host_due=clock_us+host_gap;}
}
bool bench_quiesce(void){return t5_driver_get(2)->quiesce();}
void bench_report(void){
 assert(done&&entries==322&&directory_sd_writes()==before_writes&&directory_sd_hash()==before_hash);
 assert(!sd_owned&&!phy_owned&&!usb_live);directory_sd_finish();
 printf("{\"host_gap_us\":%u,\"elapsed_us\":%llu,\"commands\":%u,\"packets\":%u,\"owner_polls\":%u,\"sd_reads\":%u,\"entries\":%u,\"lfn_entries\":%u,\"clusters\":%u,\"unchanged_card\":true}\n",host_gap,(unsigned long long)(finished_us-started_us),transactions,packet_count,host_polls,export_reads,entries,long_entries,clusters);
}
