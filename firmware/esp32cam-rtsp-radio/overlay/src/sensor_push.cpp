#include "sensor_push.h"
#include "radio_telemetry.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace sensor_push {
namespace {
struct Psram {
    void *allocate(size_t n) { return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
    void deallocate(void *p) { heap_caps_free(p); }
    void *reallocate(void *p,size_t n) { return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
};
using Document=BasicJsonDocument<Psram>;
struct Config { String host,token; uint16_t port=9877; uint32_t revision=0; } config;
SemaphoreHandle_t mutex=nullptr;
WebServer *server=nullptr;
bool (*authorizeRequest)()=nullptr;
WebSocketsClient socket;
String headers,boot;
volatile bool connected=false;
bool awaitingAck=false,clockReady=false;
uint64_t inflightSequence=0;
size_t lastPacketBytes=0;
uint64_t acknowledged=0,sequence=0;
uint32_t sent=0,failed=0;

void writePacket(JsonObject packet,bool streaming=true) {
    const uint64_t now=esp_timer_get_time();
    radio_telemetry::writeFrameJson(packet,now,acknowledged ? acknowledged : 1);
    packet["schema"]="biome.sensor_tick.v1";
    packet["image_available"]=false;
    packet["capture_timestamp_semantics"]="sensor_snapshot_tick_no_camera_frame";
    packet["stream_sequence"]=streaming ? ++sequence : sequence;
    packet["served_monotonic_us"]=static_cast<uint64_t>(esp_timer_get_time());
}

void event(WStype_t type,uint8_t *payload,size_t length) {
    if(type==WStype_CONNECTED) { connected=true; acknowledged=0; awaitingAck=false; clockReady=false; }
    if(type==WStype_DISCONNECTED) { connected=false; acknowledged=0; awaitingAck=false; clockReady=false; }
    if(type!=WStype_TEXT) return;
    const uint64_t received=esp_timer_get_time();
    DynamicJsonDocument request(1024);
    if(deserializeJson(request,payload,length)) return;
    const String kind=request["type"]|"";
    if(kind=="clock") {
        DynamicJsonDocument reply(512);
        reply["type"]="clock";reply["id"]=request["id"];
        reply["boot_id"]=boot;reply["received_monotonic_us"]=received;
        reply["served_monotonic_us"]=static_cast<uint64_t>(esp_timer_get_time());
        String text;serializeJson(reply,text);clockReady=socket.sendTXT(text);
    } else if(kind=="ack" && String(request["boot_id"]|"")==boot &&
              (request["stream_sequence"]|uint64_t(0))==inflightSequence) {
        const uint64_t value=request["monotonic_us"]|uint64_t(0);
        if(value>acknowledged && value<=static_cast<uint64_t>(esp_timer_get_time())) acknowledged=value;
        awaitingAck=false;
    }
}

void worker(void *) {
    uint32_t revision=UINT32_MAX,last=0;
    socket.onEvent(event);socket.setReconnectInterval(1000);socket.enableHeartbeat(5000,2000,2);
    for(;;) {
        Config current;
        xSemaphoreTake(mutex,portMAX_DELAY);current=config;xSemaphoreGive(mutex);
        if(current.revision!=revision) {
            socket.disconnect();connected=false;acknowledged=0;awaitingAck=false;clockReady=false;revision=current.revision;
            if(current.host.length() && current.token.length()) {
                headers="Authorization: Sensor "+current.token;
                socket.setExtraHeaders(headers.c_str());socket.begin(current.host,current.port,"/sensors");
            }
        }
        if(current.host.length() && WiFi.status()==WL_CONNECTED) socket.loop();
        const uint32_t now=millis();
        if(connected && clockReady && !awaitingAck && now-last>=100) {
            last=now;
            Document document(131072);JsonObject packet=document.to<JsonObject>();writePacket(packet);
            if(boot.isEmpty()) boot=String(packet["boot_id"]|"");
            if(!document.overflowed()) {
                const size_t size=measureJson(document);
                auto bytes=static_cast<uint8_t *>(heap_caps_malloc(size+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
                if(bytes) { serializeJson(document,reinterpret_cast<char *>(bytes),size+1);
                    if(socket.sendTXT(static_cast<const uint8_t *>(bytes),size)) {
                        ++sent;awaitingAck=true;inflightSequence=sequence;lastPacketBytes=size;
                    } else ++failed;
                    heap_caps_free(bytes);
                } else ++failed;
            } else ++failed;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void status() {
    if(!authorizeRequest || !authorizeRequest()) return;
    Config current;xSemaphoreTake(mutex,portMAX_DELAY);current=config;xSemaphoreGive(mutex);
    DynamicJsonDocument d(1024);
    d["transport"]="websocket_push";d["configured"]=!current.host.isEmpty();
    d["host"]=current.host;d["port"]=current.port;d["connected"]=connected;
    d["sent"]=sent;d["failed"]=failed;
    d["awaiting_ack"]=awaitingAck;d["clock_ready"]=clockReady;
    d["last_packet_bytes"]=lastPacketBytes;d["inflight_sequence"]=inflightSequence;
    String text;serializeJson(d,text);server->send(200,"application/json",text);
}

void configure() {
    if(!authorizeRequest || !authorizeRequest()) return;
    DynamicJsonDocument d(2048);
    if(deserializeJson(d,server->arg("plain")) || !d["host"].is<const char *>() ||
       !d["token"].is<const char *>() || !d["port"].is<unsigned int>()) {
        server->send(400,"application/json","{\"error\":\"host, port and token required\"}");return;
    }
    Config next;next.host=String(d["host"].as<const char *>());next.token=String(d["token"].as<const char *>());
    const unsigned int port=d["port"].as<unsigned int>();
    if(next.host.isEmpty() || next.host.length()>253 || next.host.indexOf('/')>=0 ||
       next.token.length()<32 || next.token.length()>128 || next.token.indexOf('\r')>=0 || next.token.indexOf('\n')>=0 || !port || port>65535) {
        server->send(400,"application/json","{\"error\":\"invalid stream configuration\"}");return;
    }
    next.port=port;
    Preferences prefs;
    if(!prefs.begin("rover-push",false)) {server->send(503,"application/json","{\"error\":\"storage unavailable\"}");return;}
    const bool stored=prefs.putString("host",next.host)>0 && prefs.putString("token",next.token)>0 && prefs.putUShort("port",next.port)>0;
    prefs.end();
    if(!stored) {server->send(503,"application/json","{\"error\":\"stream configuration not persisted\"}");return;}
    xSemaphoreTake(mutex,portMAX_DELAY);next.revision=config.revision+1;config=next;xSemaphoreGive(mutex);
    status();
}

void capture() {
    if(!authorizeRequest || !authorizeRequest()) return;
    Document d(131072);writePacket(d.to<JsonObject>(),false);
    if(d.overflowed()) {server->send(503,"application/json","{\"error\":\"sensor allocation failed\"}");return;}
    String text;serializeJson(d,text);server->sendHeader("Cache-Control","no-store");server->send(200,"application/json",text);
}
} // namespace

void begin(WebServer &http,bool (*authorize)()) {
    server=&http;authorizeRequest=authorize;mutex=xSemaphoreCreateMutex();
    if(!mutex) return;
    Preferences prefs;
    if(prefs.begin("rover-push",true)) {config.host=prefs.getString("host","");config.token=prefs.getString("token","");config.port=prefs.getUShort("port",9877);prefs.end();}
    Document initial(131072);writePacket(initial.to<JsonObject>(),false);boot=String(initial["boot_id"]|"");
    http.on("/api/sensors/capture",HTTP_GET,capture);
    http.on("/api/telemetry/stream",HTTP_GET,status);
    http.on("/api/telemetry/stream",HTTP_POST,configure);
    xTaskCreate(worker,"sensor-websocket",8192,nullptr,1,nullptr);
}
} // namespace sensor_push
