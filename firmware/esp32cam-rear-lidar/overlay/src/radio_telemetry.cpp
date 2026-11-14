// Rear-only implementation of the front camera's capture hook.
// Camera/RTSP/OTA never waits for a LiDAR initialization or a UART peer.
#include "radio_telemetry.h"
#include "rear_health.h"
#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <SparkFun_VL53L5CX_Library.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <sdkconfig.h>
#include <cstring>

namespace radio_telemetry {
namespace {
#if defined(CONFIG_SCCB_HARDWARE_I2C_PORT1) && CONFIG_SCCB_HARDWARE_I2C_PORT1
constexpr int SENSOR_BUS=0;
#elif defined(CONFIG_SCCB_HARDWARE_I2C_PORT0) && CONFIG_SCCB_HARDWARE_I2C_PORT0
constexpr int SENSOR_BUS=1;
#else
#error Confirm camera SCCB controller before building rear firmware.
#endif
constexpr int SDA_PIN=41,SCL_PIN=42;
constexpr int LPN[2]={1,21};
constexpr uint8_t ADDRESS[2]={0x2a,0x2b};
constexpr int UART_TX=38,UART_RX=39;
TwoWire sensorWire(SENSOR_BUS);
HardwareSerial peer(2);
struct Sample {
 bool ready=false,valid=false;
 uint64_t observed=0;
 uint32_t sequence=0,errors=0,retries=0;
 int16_t distance[64]={};
 uint8_t status[64]={};
 const char *error="not_detected";
};
struct Device {
 SparkFun_VL53L5CX_IO io;
 SparkFun_VL53L5CX driver;
 VL53L5CX_Configuration *config=nullptr;
 VL53L5CX_ResultsData *results=nullptr;
 bool ready=false;
 uint32_t retryAt=0;
 uint64_t lastRead=0;
 Sample sample;
};
Device devices[2];
SemaphoreHandle_t snapshots=nullptr;
Sample published[2];
volatile bool paused=false;
bool started=false;
bool lidarWorker=false,uartWorker=false;
uint64_t lastPeerRx=0;
uint32_t uartErrors=0;
portMUX_TYPE peerMux=portMUX_INITIALIZER_UNLOCKED;
char boot[40],receiver[40];
uint64_t nowUs(){return static_cast<uint64_t>(esp_timer_get_time());}
void publish(int i) {
 if (snapshots && xSemaphoreTake(snapshots,0)==pdTRUE) {
  published[i]=devices[i].sample;xSemaphoreGive(snapshots);
 }
}
bool probe(uint8_t address) {
 sensorWire.beginTransmission(address);return sensorWire.endTransmission()==0;
}
void fail(int i,const char *why) {
 auto &d=devices[i];d.ready=false;d.sample.ready=false;d.sample.valid=false;
 d.sample.error=why;++d.sample.errors;d.retryAt=millis()+2000;
 digitalWrite(LPN[i],LOW);publish(i);
}
bool initialize(int i) {
 auto &d=devices[i];++d.sample.retries;
 // Disable only the peer's communications while claiming an address.
 // LPn is communications-enable, not a chip reset; try its retained address first.
 digitalWrite(LPN[1-i],LOW);digitalWrite(LPN[i],HIGH);
 vTaskDelay(pdMS_TO_TICKS(2));
 uint8_t address=probe(ADDRESS[i])?ADDRESS[i]:0x29;
 bool present=probe(address);
 if (!present) {
  digitalWrite(LPN[i],LOW);
  if(devices[1-i].ready)digitalWrite(LPN[1-i],HIGH);
  fail(i,"not_detected");return false;
 }
 if (!d.config) {
  d.config=static_cast<VL53L5CX_Configuration *>(heap_caps_calloc(1,sizeof(VL53L5CX_Configuration),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  d.results=static_cast<VL53L5CX_ResultsData *>(heap_caps_calloc(1,sizeof(VL53L5CX_ResultsData),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 }
 if (!d.config || !d.results) {
  if(devices[1-i].ready)digitalWrite(LPN[1-i],HIGH);
  fail(i,"sensor_memory_unavailable");return false;
 }
 memset(d.config,0,sizeof(*d.config));
 d.config->platform.VL53L5CX_i2c=&d.io;
 d.driver.Dev=d.config;d.driver.VL53L5CX_i2c=&d.io;
 d.io.begin(address,sensorWire);
 d.driver.setWireMaxPacketSize(128);
 uint8_t alive=0;
 bool ok=vl53l5cx_is_alive(d.config,&alive)==0 && alive;
 // Assign before the long firmware download, so another live sensor is never
 // exposed at the same address on the bus.
 if(ok && address!=ADDRESS[i])ok=d.driver.setAddress(ADDRESS[i]);
 if(devices[1-i].ready)digitalWrite(LPN[1-i],HIGH);
 if(ok)ok=vl53l5cx_init(d.config)==0;
 if(ok)ok=d.driver.setResolution(64) && d.driver.setRangingFrequency(5) && d.driver.startRanging();
 if(!ok){fail(i,"initialization_failed");return false;}
 d.ready=true;d.sample.ready=true;d.sample.error=nullptr;d.lastRead=nowUs();publish(i);return true;
}
void sensorTask(void *) {
 bool busReady=sensorWire.begin(SDA_PIN,SCL_PIN,400000);
 sensorWire.setTimeOut(10);
 while(true) {
  if(paused){vTaskDelay(pdMS_TO_TICKS(20));continue;}
  if(!busReady) {
   busReady=sensorWire.begin(SDA_PIN,SCL_PIN,400000);sensorWire.setTimeOut(10);
   vTaskDelay(pdMS_TO_TICKS(100));continue;
  }
  for(int i=0;i<2 && !paused;++i) {
   auto &d=devices[i];
   if(!d.ready){if(rear_health::due(millis(),d.retryAt))initialize(i);continue;}
   if(!d.driver.isConnected()){fail(i,"connection_lost");continue;}
   if(d.driver.isDataReady()) {
    if(!d.driver.getRangingData(d.results)){fail(i,"read_failed");continue;}
    for(int zone=0;zone<64;++zone) {
     d.sample.distance[zone]=d.results->distance_mm[zone];
     d.sample.status[zone]=d.results->target_status[zone];
    }
    d.lastRead=nowUs();d.sample.observed=d.lastRead;
    d.sample.valid=true;d.sample.ready=true;d.sample.error=nullptr;++d.sample.sequence;publish(i);
   } else if(nowUs()-d.lastRead>2000000)fail(i,"data_timeout");
  }
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
void writeIdentity(JsonObject out) {
 out["receiver_id"]=receiver;out["boot_id"]=boot;
 out["camera_id"]="rear";out["clock"]="esp_timer";
}
void writeSensors(JsonObject out,uint64_t at) {
 Sample samples[2];
 bool copied=snapshots && xSemaphoreTake(snapshots,0)==pdTRUE;
 if(copied){samples[0]=published[0];samples[1]=published[1];xSemaphoreGive(snapshots);}
 out["schema"]="rover.lidar.v1";out["worker_ready"]=lidarWorker;
 JsonArray array=out.createNestedArray("sensors");
 for(int i=0;i<2;++i) {
  auto &s=samples[i];JsonObject row=array.createNestedObject();row["id"]=i==0?"front":"rear";
  row["address_7bit"]=ADDRESS[i];row["ready"]=s.ready;row["sequence"]=s.sequence;
  bool valid=copied && s.valid && rear_health::fresh(at,s.observed);
  row["valid"]=valid;row["resolution"]=64;
  row["observed_monotonic_us"]=s.observed;
  if(s.observed && at>=s.observed)row["age_ms"]=(at-s.observed)/1000;else row["age_ms"]=nullptr;
  row["errors"]=s.errors;row["initialization_attempts"]=s.retries;
  row["error"]=copied?s.error:"snapshot_busy";
  // Do not emit post-capture readings or stale ranges as if captured with image.
  if(valid) {
   JsonArray ranges=row.createNestedArray("distance_mm"),status=row.createNestedArray("target_status");
   for(int z=0;z<64;++z){ranges.add(s.distance[z]);status.add(s.status[z]);}
  }
 }
}
void writeLink(JsonObject out,uint64_t at) {
 uint64_t rx;uint32_t errors;
 portENTER_CRITICAL(&peerMux);rx=lastPeerRx;errors=uartErrors;portEXIT_CRITICAL(&peerMux);
 out["worker_ready"]=uartWorker;out["connected"]=rear_health::linkFresh(at,rx);
 out["last_rx_monotonic_us"]=rx;out["parse_errors"]=errors;
 out["clock_sync_status"]="unmapped_until_primary_time_exchange";
}
void uartTask(void *) {
 peer.setRxBufferSize(2048);peer.begin(921600,SERIAL_8N1,UART_RX,UART_TX);
 char input[1024];size_t used=0;bool discard=false;
 String pending;size_t sent=0;
 while(true) {
  if(paused){vTaskDelay(pdMS_TO_TICKS(20));continue;}
  while(peer.available()) {
   char c=static_cast<char>(peer.read());
   if(c=='\r')continue;
   if(c!='\n') {
    if(!discard && used<sizeof(input)-1)input[used++]=c;
    else discard=true;
    continue;
   }
   uint64_t received=nowUs();input[used]=0;
   StaticJsonDocument<1536> request;
   bool valid=!discard && !deserializeJson(request,input) && request["schema"]=="rover.uart.v1";
   used=0;discard=false;
   const char *op=valid?(request["op"]|""):"";
   bool recognized=valid && (!strcmp(op,"ping")||!strcmp(op,"time_sync")||!strcmp(op,"status"));
   if(!recognized) {
    portENTER_CRITICAL(&peerMux);++uartErrors;portEXIT_CRITICAL(&peerMux);continue;
   }
   portENTER_CRITICAL(&peerMux);lastPeerRx=received;portEXIT_CRITICAL(&peerMux);
   // Replace stale unsent replies rather than growing a UART backlog.
   StaticJsonDocument<6144> reply;JsonObject object=reply.to<JsonObject>();
   object["schema"]="rover.uart.v1";object["op"]=!strcmp(op,"time_sync")?"time_sync_reply":"status";
   object["request_id"]=request["request_id"];object["t1_primary_us"]=request["t1_primary_us"];
   object["t2_rear_us"]=received;writeIdentity(object);
   if(strcmp(op,"time_sync"))writeSensors(object.createNestedObject("lidar"),nowUs());
   object["t3_rear_us"]=nowUs();
   object["timestamp_semantics"]="t2_after_receive_t3_before_software_tx_queue";
   pending="";serializeJson(reply,pending);pending+='\n';sent=0;
  }
  int room=peer.availableForWrite();
  if(room>0 && sent<pending.length()) {
   size_t n=pending.length()-sent;if(n>static_cast<size_t>(room))n=room;
   sent+=peer.write(reinterpret_cast<const uint8_t *>(pending.c_str())+sent,n);
  }
  vTaskDelay(pdMS_TO_TICKS(2));
 }
}
void sendStatus(WebServer &server) {
 DynamicJsonDocument document(8192);JsonObject out=document.to<JsonObject>();
 writeIdentity(out);writeSensors(out.createNestedObject("lidar"),nowUs());
 writeLink(out.createNestedObject("uart"),nowUs());out["camera_independent"]=true;
 out["firmware"]="1.0-rear.1";out["ota_supported"]=true;
 String text;serializeJson(document,text);server.send(200,"application/json",text);
}
}
void begin(WebServer &server,bool (*authorize)()) {
 if(started)return;started=true;
 snprintf(receiver,sizeof(receiver),"rear-%012llx",static_cast<unsigned long long>(ESP.getEfuseMac()));
 snprintf(boot,sizeof(boot),"%08lx%08lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
 for(int pin:LPN){pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
 snapshots=xSemaphoreCreateMutex();
 if(snapshots)lidarWorker=xTaskCreate(sensorTask,"rear-lidar",8192,nullptr,1,nullptr)==pdPASS;
 uartWorker=xTaskCreate(uartTask,"rear-uart",12288,nullptr,1,nullptr)==pdPASS;
 server.on("/api/telemetry",HTTP_GET,[&server,authorize](){if(authorize())sendStatus(server);});
 server.on("/api/lidar",HTTP_GET,[&server,authorize](){if(authorize())sendStatus(server);});
 server.on("/api/rear",HTTP_GET,[&server,authorize](){if(authorize())sendStatus(server);});
}
void loop(){}
void pause(bool value){paused=value;}
bool writeFrameJson(JsonObject out,uint64_t captureUs) {
 out["schema"]="biome.capture.v1";writeIdentity(out);out["capture_clock"]="esp_timer";
 out["capture_monotonic_us"]=captureUs;out["frame_id"]=String(boot)+":"+String(static_cast<unsigned long long>(captureUs));
 writeSensors(out.createNestedObject("lidar"),captureUs);
 writeLink(out.createNestedObject("uart"),nowUs());
 return true; // Optional peripherals are never a reason to reject an image.
}
}
