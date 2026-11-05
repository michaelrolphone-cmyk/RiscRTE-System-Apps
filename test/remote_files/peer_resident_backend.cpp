/* Production peer/WebDAV fixtures with controlled native ownership boundaries. */
#include "peer_ui_backend.cpp"
static bool connected, allowClose, holdEnd, heldLease, retainedNetwork, workerBusy;
static unsigned networkBegins, networkCancels, wifiAcquires;
static int32_t (*originalBegin)(void*,const risc_radio_request_v1*,uint32_t*);
static int32_t (*originalCancel)(void*,uint32_t);
extern "C" void peer_resident_begin(bool direct){
 peer_backend_begin();backend->privateNetwork=direct;
 connected=allowClose=holdEnd=heldLease=retainedNetwork=workerBusy=false;networkBegins=networkCancels=wifiAcquires=0;
 originalBegin=backend->wifi.begin;originalCancel=backend->wifi.cancel;
 backend->wifi.begin=[](void*c,const risc_radio_request_v1*r,uint32_t*t)->int32_t{++networkBegins;return originalBegin(c,r,t);};
 backend->wifi.poll=[](void*c,uint32_t id,risc_radio_progress_v1*out)->int32_t{
  auto&p=*static_cast<PeerControllerFixture*>(c);assert(id==51);*out={};out->struct_size=sizeof(*out);out->operation_id=id;out->scan.struct_size=sizeof(out->scan);
  bool done=p.networkCancelled&&allowClose;
  out->phase=retainedNetwork?RISC_RADIO_CLEANUP_FAILED:done?RISC_RADIO_IDLE:p.networkCancelled?RISC_RADIO_STOPPING:connected?RISC_RADIO_CONNECTED:RISC_RADIO_JOINING;
  out->quiescent=done;out->owned=!done;out->station_state=done?0:connected?2:1;
  if(connected){out->station[0]=192;out->station[1]=168;out->station[2]=4;out->station[3]=2;}
  if(done)p.networkLive=false;
  if(retainedNetwork){out->failure=RISC_RADIO_FAILURE_CLEANUP;return RISC_RADIO_RETAINED;}
  return done?RISC_RADIO_QUIESCENT:RISC_RADIO_PENDING;
 };
 backend->wifi.cancel=[](void*c,uint32_t id)->int32_t{++networkCancels;auto rc=originalCancel(c,id);backend->networkLive=true;return rc;};
 backend->wifi.service_begin=[](void*,uint32_t*t)->int32_t{assert(!heldLease);if(retainedNetwork){*t=0;return RISC_RADIO_RETAINED;}if(workerBusy){*t=0;return RISC_RADIO_BUSY;}heldLease=true;*t=9;return RISC_RADIO_ACCEPTED;};
 backend->wifi.service_end=[](void*,uint32_t t)->int32_t{assert(heldLease&&t==9);if(retainedNetwork)return RISC_RADIO_RETAINED;if(holdEnd)return RISC_RADIO_AGAIN;heldLease=false;return RISC_RADIO_ACCEPTED;};
}
extern "C" const void*peer_resident_capability(const char*n){if(!strcmp(n,"net.wifi")){++wifiAcquires;return &backend->wifi;}return peer_backend_capability(n);}
extern "C" unsigned peer_resident_acquires(){return wifiAcquires;}
extern "C" unsigned peer_resident_begins(){return networkBegins;}
extern "C" unsigned peer_resident_cancels(){return networkCancels;}
extern "C" bool peer_resident_native_busy(){return backend->networkLive||heldLease||backend->f.link.clientLive;}
extern "C" bool peer_resident_tcp_live(){return backend->f.link.clientLive;}
extern "C" bool peer_resident_lease_live(){return heldLease;}
extern "C" void peer_resident_connect(){connected=true;}
extern "C" void peer_resident_quiesce(){allowClose=true;}
extern "C" void peer_resident_hold_end(bool value){holdEnd=value;}
extern "C" void peer_resident_retain(){retainedNetwork=true;}

extern "C" void peer_resident_worker_busy(bool value){workerBusy=value;}
