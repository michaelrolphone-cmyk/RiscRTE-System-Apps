#include "OutputStream.h"
#include "FreshIntent.h"
#include "Board.h"
#include <esp_websocket_client.h>
#include <esp_crt_bundle.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "sdkconfig.h"
#include <esp_netif_sntp.h>
#include <sys/time.h>
#include <atomic>
#include <new>
#include <mutex>
namespace BiomeRelay {
static_assert(CONFIG_WS_BUFFER_SIZE>=6144,"Stream credentials require the bounded HTTP upgrade buffer");
static_assert(CONFIG_WS_DYNAMIC_BUFFER,"Release the upgrade buffer after connection");
static_assert(CONFIG_MBEDTLS_DYNAMIC_BUFFER&&CONFIG_ESP_WS_CLIENT_ENABLE_DYNAMIC_BUFFER,"Stream and HTTP TLS must share bounded internal RAM");
struct OutputStream::State {
 esp_websocket_client_handle_t socket=nullptr;
 FreshIntent intent;JsonDocument grant;char url[1024]{},headers[4096]{},buffer[2049]{};
 size_t used=0;bool assembling=false;std::atomic<bool> connected{false},failed{false};
 uint64_t lastApplied=0,lastRejectedLog=0;std::mutex ackMutex;
 void ack(const char* stage,uint64_t sequence){
  std::lock_guard<std::mutex> guard(ackMutex);
  if(!connected.load())return;
  const uint64_t next=intent.acknowledge(sequence,!strcmp(stage,"applied"));if(!next)return;
  JsonDocument value;value["type"]="ack";value["stage"]=stage;value["sequence"]=next;
  value["intent_sequence"]=sequence;value["epoch"]=grant["epoch"];value["authority"]=grant["authority"];
  value["generation"]=grant["generation"];value["commanded_mask"]=RelayBoard::streamOutput();
  value["contact_feedback"]=false;value["provider_monotonic_ms"]=esp_timer_get_time()/1000;
  char data[512];const size_t n=serializeJson(value,data,sizeof(data));
  if(n<sizeof(data))esp_websocket_client_send_text(socket,data,n,pdMS_TO_TICKS(20));
 }
 void data(const esp_websocket_event_data_t& event){
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
  if(tv.tv_sec<1704067200){
   if(mono-lastRejectedLog>=5000){lastRejectedLog=mono;ESP_LOGW("relay-stream","Intent rejected: clock is not synchronized");}
   return;
  } // No unsynchronized clock can authorize motion.
  const uint64_t utc=uint64_t(tv.tv_sec)*1000+tv.tv_usec/1000;
  if(!intent.accept(msg["authority"]|"",msg["generation"],msg["epoch"]|"",msg["sequence"],
      mask,uint64_t(msg["issued_at"].as<double>()*1000),uint64_t(msg["expires_at"].as<double>()*1000),utc,mono)){
   if(mono-lastRejectedLog>=5000){lastRejectedLog=mono;ESP_LOGW("relay-stream","Intent rejected: binding, sequence or freshness; remaining_ms=%lld",
    (long long)(int64_t(msg["expires_at"].as<double>()*1000)-int64_t(utc)));}
   return;
  }
  RelayBoard::streamDesired(intent.desired,intent.deadline,intent.sequence);
  ack("received",intent.sequence);
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
 s->grant.set(grant);s->intent=retainedIntent;
 if(!s->intent.bind(grant["authority"]|"",grant["generation"],grant["epoch"]|"")||!RelayBoard::streamBegin()){delete s;return false;}
 snprintf(s->url,sizeof(s->url),"%s/control",url);snprintf(s->headers,sizeof(s->headers),"Authorization: Stream %s\r\n",key);
 static bool ntp=false;
 if(!ntp){esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");if(esp_netif_sntp_init(&config)==ESP_OK)ntp=true;}
 esp_websocket_client_config_t config{};config.uri=s->url;config.headers=s->headers;
 config.crt_bundle_attach=esp_crt_bundle_attach;config.buffer_size=2048;config.network_timeout_ms=10000;config.disable_auto_reconnect=true;
 s->socket=esp_websocket_client_init(&config);
 if(!s->socket||esp_websocket_register_events(s->socket,WEBSOCKET_EVENT_ANY,State::event,s)!=ESP_OK||
    esp_websocket_client_start(s->socket)!=ESP_OK){
  if(s->socket)esp_websocket_client_destroy(s->socket);RelayBoard::streamEnd();delete s;return false;
 }
 state=s;return true;
}
void OutputStream::tick(){
 if(!state)return;
 const uint64_t applied=RelayBoard::streamApplied();
 if(applied!=state->lastApplied){state->lastApplied=applied;state->ack("applied",applied);}
}
void OutputStream::close(){
 if(!state)return;
 RelayBoard::streamNeutral();
 if(state->socket){esp_websocket_client_stop(state->socket);esp_websocket_client_destroy(state->socket);}
 retainedIntent=state->intent;retainedIntent.detach();
 state->intent.revoke();RelayBoard::streamEnd();delete state;state=nullptr;
}
// An opening transport owns its attempt until the SDK reports a terminal event.
// A provider poll must not tear down an in-flight TLS/WebSocket handshake.
bool OutputStream::active()const{return state&&!state->failed.load();}
}
