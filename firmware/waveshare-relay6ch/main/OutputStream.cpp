#include "OutputStream.h"
#include "FreshIntent.h"
#include "PendingAcks.h"
#include "CompensatedClock.h"
#include "esp_sntp.h"
#include "lwip/apps/sntp_opts.h"
#include "Board.h"
#include <esp_websocket_client.h>
#include <esp_crt_bundle.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "sdkconfig.h"
#include <esp_netif_sntp.h>
#include <sys/time.h>
#include <atomic>
#include <mutex>
#include <new>
namespace BiomeRelay {
static_assert(SNTP_COMP_ROUNDTRIP==1&&SNTP_CHECK_RESPONSE>=2,"Control requires compensated, origin-checked SNTP");
namespace {
CompensatedClock streamClock;
void clockSynchronized(timeval* value){
 const unsigned samples=streamClock.synchronized(value->tv_sec);
 ESP_LOGI("relay-stream","SNTP sync samples=%u utc=%lld.%06ld compensated_ready=%u",samples,
  (long long)value->tv_sec,(long)value->tv_usec,unsigned(streamClock.ready()));
}
}
static_assert(CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK,"Control receive must not share the ACK transmit lock");
static_assert(CONFIG_WS_BUFFER_SIZE>=6144,"Stream credentials require the bounded HTTP upgrade buffer");
static_assert(CONFIG_WS_DYNAMIC_BUFFER,"Release the upgrade buffer after connection");
#ifdef CONFIG_ESP_WS_CLIENT_ENABLE_DYNAMIC_BUFFER
#error "Control RX/TX buffers must remain allocated for the attachment lifetime"
#endif
static_assert(CONFIG_MBEDTLS_DYNAMIC_BUFFER,"HTTP and stream TLS use bounded record buffers");
struct OutputStream::State {
 esp_websocket_client_handle_t socket=nullptr;
 FreshIntent intent;char buffer[2049]{};
 size_t used=0;bool assembling=false,started=false;std::atomic<bool> connected{false},failed{false};
 uint64_t lastRejectedLog=0,lastAckFailureLog=0,lastReceivedAt=0;PendingAcks pending;
 std::atomic<bool> closing{false},feedbackRunning{false},feedbackStopped{false};bool feedbackStarted=false;std::mutex inputMutex;
 std::atomic<uint32_t> intentAgeMs{0},receiveGapMs{0};
 bool ack(const char* stage,uint64_t sequence){
  if(!connected.load()||sequence>(UINT64_MAX-2)/2)return false;
  const bool applied=!strcmp(stage,"applied");
  if(sequence*2+(applied?2:1)<=intent.ackSequence)return true;
  return intent.acknowledge(sequence,applied,[&](uint64_t next){
   JsonDocument value;value["type"]="ack";value["stage"]=stage;value["sequence"]=next;
   value["intent_sequence"]=sequence;value["epoch"]=intent.epoch;value["authority"]=intent.authority;
   value["generation"]=intent.generation;value["commanded_mask"]=RelayBoard::streamOutput();
   value["output_transitions"]=RelayBoard::streamTransitions();value["intent_expirations"]=RelayBoard::streamExpirations();
   value["intent_age_ms"]=intentAgeMs.load();value["receive_gap_ms"]=receiveGapMs.load();
   value["contact_feedback"]=false;value["provider_monotonic_ms"]=esp_timer_get_time()/1000;
   char data[512];const size_t n=serializeJson(value,data,sizeof(data));
   const int sent=n<sizeof(data)?esp_websocket_client_send_text(socket,data,n,pdMS_TO_TICKS(1000)):-1;
   if(sent==int(n))return true;
   const uint64_t now=esp_timer_get_time()/1000;
   if(now-lastAckFailureLog>=5000){
    lastAckFailureLog=now;ESP_LOGW("relay-stream","ACK write failed stage=%s sequence=%llu sent=%d expected=%u",
     stage,(unsigned long long)sequence,sent,unsigned(n));
   }
   return false;
  })!=0;
 }
 static void feedbackTask(void* context){
  auto* self=static_cast<State*>(context);
  while(self->feedbackRunning.load(std::memory_order_acquire)){
   if(self->connected.load())self->pending.drain(RelayBoard::streamApplied(),[&](bool applied,uint64_t sequence){
    return self->ack(applied?"applied":"received",sequence);
   });
   vTaskDelay(pdMS_TO_TICKS(20));
  }
  self->feedbackStopped.store(true,std::memory_order_release);
  vTaskDelete(nullptr);
 }
 void data(const esp_websocket_event_data_t& event){
  if(closing.load()||!connected.load())return;
  if(event.op_code==1&&event.payload_offset==0){used=0;assembling=true;}
  if(!assembling||event.payload_offset!=int(used)||event.data_len<0||
     event.payload_len>int(sizeof(buffer)-1)||used+event.data_len>=sizeof(buffer)){assembling=false;return;}
  memcpy(buffer+used,event.data_ptr,event.data_len);used+=event.data_len;
  if(used!=size_t(event.payload_len)||!event.fin)return;
  assembling=false;buffer[used]=0;JsonDocument msg;
  if(deserializeJson(msg,buffer,used)||!msg["sequence"].is<uint64_t>()||
     msg["type"]!="desired"||!msg["outputs"]["mask"].is<unsigned>()||
     !msg["issued_at"].is<double>()||!msg["expires_at"].is<double>()||
     !msg["generation"].is<uint64_t>())return;
  const unsigned mask=msg["outputs"]["mask"];if(mask>63)return;
  timeval tv{};gettimeofday(&tv,nullptr);
  const uint64_t mono=esp_timer_get_time()/1000;
  if(!streamClock.ready()||tv.tv_sec<1704067200){
   if(mono-lastRejectedLog>=5000){lastRejectedLog=mono;ESP_LOGW("relay-stream","Intent rejected: clock is not synchronized");}
   return;
  } // No unsynchronized clock can authorize motion.
  const uint64_t utc=uint64_t(tv.tv_sec)*1000+tv.tv_usec/1000;
  if(!intent.accept(msg["authority"]|"",msg["generation"],msg["epoch"]|"",msg["sequence"],
      mask,uint64_t(msg["issued_at"].as<double>()*1000),uint64_t(msg["expires_at"].as<double>()*1000),utc,mono)){
   if(mono-lastRejectedLog>=5000){lastRejectedLog=mono;ESP_LOGW("relay-stream","Intent rejected: binding, sequence or freshness; sequence=%llu age_ms=%lld remaining_ms=%lld",
    (unsigned long long)msg["sequence"].as<uint64_t>(),
    (long long)(int64_t(utc)-int64_t(msg["issued_at"].as<double>()*1000)),
    (long long)(int64_t(msg["expires_at"].as<double>()*1000)-int64_t(utc)));}
   return;
  }
  intentAgeMs.store(uint32_t(utc-uint64_t(msg["issued_at"].as<double>()*1000)));
  receiveGapMs.store(lastReceivedAt?uint32_t(mono-lastReceivedAt):0);lastReceivedAt=mono;
  {std::lock_guard<std::mutex> guard(inputMutex);
   if(closing.load())return;
   RelayBoard::streamDesired(intent.desired,intent.deadline,intent.sequence);
   pending.accepted(intent.sequence);
  }
 }
 static void event(void* context,esp_event_base_t,int32_t id,void* data){
  auto* self=static_cast<State*>(context);
  if(id==WEBSOCKET_EVENT_CONNECTED){self->failed=false;self->connected=true;RelayBoard::streamNeutral();ESP_LOGI("relay-stream","Connected");}
  else if(id==WEBSOCKET_EVENT_DISCONNECTED||id==WEBSOCKET_EVENT_ERROR||id==WEBSOCKET_EVENT_CLOSED){
   self->failed=true;self->connected=false;self->assembling=false;self->intent.detach();RelayBoard::streamNeutral();
  }else if(id==WEBSOCKET_EVENT_DATA)self->data(*static_cast<esp_websocket_event_data_t*>(data));
 }
};
bool OutputStream::attach(JsonObjectConst grant){
 close();
 if(grant["role"]!="provider"||grant["contract"]!="device.outputs.stream:1"||
    !grant["generation"].is<uint64_t>()||!grant["url"].is<const char*>()||
    !grant["attachment_credential"].is<const char*>())return false;
 const char* url=grant["url"];const char* key=grant["attachment_credential"];
 if(strncmp(url,"wss://",6)||strlen(url)>950||strlen(key)>3900||strchr(key,'\r')||strchr(key,'\n'))return false;
 auto* s=new(std::nothrow) State;if(!s)return false;
 s->intent=retainedIntent;
 if(!s->intent.bind(grant["authority"]|"",grant["generation"],grant["epoch"]|"")||!RelayBoard::streamBegin()){delete s;return false;}
 // The SDK copies URI and headers during init. Do not retain another full
 // credential, grant document, or 5 KiB of header/URL storage per attachment.
 char streamUrl[1024],streamHeaders[4096];
 snprintf(streamUrl,sizeof(streamUrl),"%s/control",url);snprintf(streamHeaders,sizeof(streamHeaders),"Authorization: Stream %s\r\n",key);
 static bool ntp=false;
 if(!ntp){
  // Preserve subsecond SNTP time. HTTP/TLS session UTC never sets this clock.
  esp_sntp_set_sync_interval(60000);
  esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");config.sync_cb=clockSynchronized;
  if(esp_netif_sntp_init(&config)==ESP_OK)ntp=true;
 }
 if(!ntp){RelayBoard::streamEnd();delete s;return false;}
 esp_websocket_client_config_t config{};config.uri=streamUrl;config.headers=streamHeaders;
 config.crt_bundle_attach=esp_crt_bundle_attach;config.buffer_size=1024;config.network_timeout_ms=10000;config.disable_auto_reconnect=true;
 s->socket=esp_websocket_client_init(&config);
 if(!s->socket||esp_websocket_register_events(s->socket,WEBSOCKET_EVENT_ANY,State::event,s)!=ESP_OK){
  if(s->socket)esp_websocket_client_destroy(s->socket);RelayBoard::streamEnd();delete s;return false;
 }
 state=s;return true;
}
void OutputStream::tick(){
 if(!state)return;
 if(!state->started){
  if(!streamClock.ready())return;
  state->started=true;
  // A clock-qualified attachment can now become visible to router readiness.
  // If its signed credential expired while waiting, normal handshake failure
  // causes the provider to fetch a fresh attachment for the same binding.
  state->feedbackRunning=true;
  if(xTaskCreate(State::feedbackTask,"relay-feedback",4096,state,tskIDLE_PRIORITY+1,nullptr)!=pdPASS){
   state->feedbackRunning=false;state->failed=true;return;
  }
  state->feedbackStarted=true;
  if(esp_websocket_client_start(state->socket)!=ESP_OK)state->failed=true;
  return;
 }

}
void OutputStream::close(){
 if(!state)return;
 {std::lock_guard<std::mutex> guard(state->inputMutex);state->closing=true;RelayBoard::streamNeutral();}
 state->feedbackRunning.store(false,std::memory_order_release);
 // Wait for the bounded feedback write before destroying its socket or state.
 // Receive callbacks reject input as soon as closing is set; GPIO stays neutral.
 while(state->feedbackStarted&&!state->feedbackStopped.load(std::memory_order_acquire)){
  esp_task_wdt_reset();vTaskDelay(1);
 }
 if(state->socket){if(state->started)esp_websocket_client_stop(state->socket);esp_websocket_client_destroy(state->socket);}
 retainedIntent=state->intent;retainedIntent.detach();
 state->intent.revoke();RelayBoard::streamEnd();delete state;state=nullptr;
}
// An opening transport owns its attempt until the SDK reports a terminal event.
// A provider poll must not tear down an in-flight TLS/WebSocket handshake.
bool OutputStream::active()const{return state&&!state->failed.load();}
}
