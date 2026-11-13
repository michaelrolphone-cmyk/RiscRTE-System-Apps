#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_system.h>
#if !CONFIG_ESP_WIFI_FTM_INITIATOR_SUPPORT || !CONFIG_ESP_WIFI_FTM_RESPONDER_SUPPORT
#error FTM initiator and responder support are required
#endif

static const char *VERSION="1.0.2";
static const uint16_t PORT=7446;
static const uint32_t MAX_AGE=30000;
WebServer web(80);
HTTPUpdateServer updater;
String otaPassword;
WiFiUDP udp;
Preferences prefs;
String nodeId, apName, wifiSSID, wifiPass, targetHost;
String serialLine;
uint32_t bootId, seq=0, lastPublish=0, lastScan=0, lastFTM=0, lastConnect=0;
bool scanning=false, radioBusy=false, retiring=false, provisionPending=false;
uint32_t ftmStarted=0;
String activeMac;
int activeChannel=0;
struct Peer {
 String mac, ssid;
 int channel=0, rssi=0;
 uint32_t seen=0, measured=0, reportSeq=0;
 float distance=NAN;
 uint32_t rtt=0;
 uint8_t samples=0;
 String status="unmeasured";
};
Peer peers[12];
struct Result { uint8_t mac[6]; int status; uint32_t cm, rtt; uint8_t samples; };
QueueHandle_t resultQueue;
float offsetM=0, heightM=0;
bool heightKnown=false;
bool lowTx=false;
int disconnectReason=0;
int ftmCode=-1;
String macText(const uint8_t *m) {
 char b[18]; snprintf(b,sizeof(b),"%02x:%02x:%02x:%02x:%02x:%02x",m[0],m[1],m[2],m[3],m[4],m[5]); return String(b);
}
bool parseMac(const String &s,uint8_t *m) {
 unsigned v[6]; if(sscanf(s.c_str(),"%x:%x:%x:%x:%x:%x",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5])!=6)return false;
 for(int i=0;i<6;i++){if(v[i]>255)return false; m[i]=v[i];} return true;
}
int peerIndex(const String &mac) {
 for(int i=0;i<12;i++)if(peers[i].mac==mac)return i;
 for(int i=0;i<12;i++)if(peers[i].mac.isEmpty()){peers[i].mac=mac;return i;}
 return -1;
}
void onWiFi(WiFiEvent_t event, WiFiEventInfo_t info) {
 if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED)disconnectReason=info.wifi_sta_disconnected.reason;
 if(event!=ARDUINO_EVENT_WIFI_FTM_REPORT)return;
 auto &r=info.wifi_ftm_report;
 Result out={}; memcpy(out.mac,r.peer_mac,6); out.status=r.status;
 out.cm=r.dist_est; out.rtt=r.rtt_est; out.samples=r.ftm_report_num_entries;
 xQueueSend(resultQueue,&out,0);
 free(r.ftm_report_data);
}
void statusDoc(JsonDocument &d) {
 uint32_t now=millis();
 d["schema"]="rover.tof.anchor/1"; d["firmware"]=VERSION;
 d["id"]=nodeId; d["boot_id"]=bootId; d["seq"]=seq; d["uptime_ms"]=now;
 d["ssid"]=apName; d["bssid"]=WiFi.softAPmacAddress(); wifi_ap_record_t ap={};d["channel"]=esp_wifi_sta_get_ap_info(&ap)==ESP_OK?ap.primary:WiFi.channel();
 d["ip"]=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString();
 d["wifi_status_code"]=int(WiFi.status());d["disconnect_reason"]=disconnectReason;d["low_tx_power"]=lowTx;d["last_ftm_code"]=ftmCode;
 d["connected"]=WiFi.status()==WL_CONNECTED; d["ftm_responder"]=true;
 d["ftm_state"]=retiring?"retirement_stalled":radioBusy?"measuring":scanning?"scanning":"idle";
 d["ota_available"]=otaPassword.length()>=16;
 d["offset_m"]=offsetM; d["height_known"]=heightKnown;
 if(heightKnown)d["height_m"]=heightM;
 auto a=d.createNestedArray("ranges");
 for(int i=0;i<12;i++){
  auto &p=peers[i]; if(p.mac.isEmpty()||now-p.seen>120000)continue;
  auto o=a.createNestedObject();o["target_bssid"]=p.mac; o["ssid"]=p.ssid;
  o["channel"]=p.channel; o["rssi_dbm"]=p.rssi; o["seen_age_ms"]=now-p.seen;
  o["status"]=p.status; o["range_seq"]=p.reportSeq;
  if(p.measured){o["age_ms"]=now-p.measured;o["rtt_ns"]=p.rtt;o["samples"]=p.samples;}
  bool valid=p.status=="ok"&&p.measured&&now-p.measured<MAX_AGE&&isfinite(p.distance)&&p.distance>=0;
  o["valid"]=valid; if(valid)o["distance_m"]=p.distance;
 }
}
String statusJSON(){DynamicJsonDocument d(8192);statusDoc(d);String out;serializeJson(d,out);return out;}
void publish() {
 ++seq; String data=statusJSON();
 if(WiFi.status()==WL_CONNECTED){
  IPAddress ip=WiFi.localIP(), mask=WiFi.subnetMask(), broad;
  for(int i=0;i<4;i++)broad[i]=ip[i]|~mask[i];
  udp.beginPacket(broad,PORT);udp.print(data);udp.endPacket();
  IPAddress target;
  if(!targetHost.isEmpty()&&WiFi.hostByName(targetHost.c_str(),target)==1){
   udp.beginPacket(target,PORT);udp.print(data);udp.endPacket();
  }
 }
 Serial.println(data);
}
bool applyConfig(JsonDocument &d,String &error) {
 if(d.containsKey("ssid")&&(d["ssid"].as<String>().length()>32)){error="SSID too long";return false;}
 if(d.containsKey("password")&&d["password"].as<String>().length()>63){error="password too long";return false;}
 if(d.containsKey("offset_m")&&(!isfinite(d["offset_m"].as<float>())||fabs(d["offset_m"].as<float>())>100)){error="invalid offset";return false;}
 if(d.containsKey("height_m")&&(!isfinite(d["height_m"].as<float>())||fabs(d["height_m"].as<float>())>100)){error="invalid height";return false;}
 prefs.begin("tof-anchor",false);
 if(d.containsKey("ota_password")){String key=d["ota_password"].as<String>();if(key.length()<16||key.length()>64){prefs.end();error="OTA key length";return false;}otaPassword=key;prefs.putString("ota_key",key);updater.setup(&web,"/update","admin",otaPassword.c_str());}
 if(d.containsKey("low_tx_power")){lowTx=d["low_tx_power"];prefs.putBool("low_tx",lowTx);WiFi.setTxPower(lowTx?WIFI_POWER_8_5dBm:WIFI_POWER_19_5dBm);}
 if(d.containsKey("ssid")){wifiSSID=d["ssid"].as<String>();prefs.putString("ssid",wifiSSID);provisionPending=true;}
 if(d.containsKey("password")){wifiPass=d["password"].as<String>();prefs.putString("password",wifiPass);provisionPending=true;}
 if(d.containsKey("target_host")){targetHost=d["target_host"].as<String>();prefs.putString("target",targetHost);}
 if(d.containsKey("offset_m")){offsetM=d["offset_m"];prefs.putFloat("offset",offsetM);}
 if(d.containsKey("height_m")){heightM=d["height_m"];heightKnown=true;prefs.putFloat("height",heightM);prefs.putBool("height_known",true);}
 prefs.end();return true;
}
const char PAGE[] PROGMEM=R"HTML(<!doctype html><meta name="viewport" content="width=device-width"><title>C3 ToF Anchor</title><style>body{font:18px system-ui;max-width:44rem;margin:2rem auto;padding:1rem}input,button{font:inherit;margin:.4rem;padding:.5rem}pre{white-space:pre-wrap}</style><h1>C3 ToF anchor</h1><p>Connect all anchors to the same 2.4 GHz Wi-Fi network as the rover. Each board keeps its own identity. Enter height above the floor for each anchor.</p><form id="f"><input name="ssid" placeholder="Wi-Fi name" required><input name="password" type="password" placeholder="Wi-Fi password"><input name="target_host" placeholder="Receiver host (optional)"><input name="height_m" type="number" step=".001" placeholder="Height in meters"><button>Save</button></form><p id="msg"></p><pre id="s"></pre><script>f.onsubmit=async e=>{e.preventDefault();let d=Object.fromEntries(new FormData(f));if(d.height_m==="")delete d.height_m;else d.height_m=Number(d.height_m);let r=await fetch("/api/config",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(d)});msg.textContent=await r.text()};setInterval(async()=>{try{s.textContent=JSON.stringify(await(await fetch("/api/status")).json(),null,2)}catch{}},2000)</script>)HTML";
void setup() {
 Serial.begin(115200);
 bootId=esp_random();
 resultQueue=xQueueCreate(4,sizeof(Result));
 prefs.begin("tof-anchor",true);
 otaPassword=prefs.getString("ota_key","");
 wifiSSID=prefs.getString("ssid","");wifiPass=prefs.getString("password","");
 targetHost=prefs.getString("target","");offsetM=prefs.getFloat("offset",0);
 lowTx=prefs.getBool("low_tx",false);
 heightM=prefs.getFloat("height",0);heightKnown=prefs.getBool("height_known",false);prefs.end();
 WiFi.persistent(false); WiFi.mode(WIFI_AP_STA); WiFi.setSleep(false);
 uint8_t mac[6];esp_wifi_get_mac(WIFI_IF_AP,mac);nodeId=macText(mac);
 char suffix[13];snprintf(suffix,sizeof(suffix),"%02X%02X%02X%02X%02X%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
 apName=String("C3-TOF-")+suffix;
 uint8_t subnet=10+(mac[5]%220);
 WiFi.softAPConfig(IPAddress(192,168,subnet,1),IPAddress(192,168,subnet,1),IPAddress(255,255,255,0));
 if(!WiFi.softAP(apName.c_str(),nullptr,1,0,4,true)){Serial.println("{\"error\":\"FTM AP failed\"}");}
 WiFi.onEvent(onWiFi);
 WiFi.setTxPower(lowTx?WIFI_POWER_8_5dBm:WIFI_POWER_19_5dBm);
 if(!wifiSSID.isEmpty())WiFi.begin(wifiSSID.c_str(),wifiPass.c_str());
 udp.begin(PORT);
 web.on("/",HTTP_GET,[]{web.send_P(200,"text/html",PAGE);});
 web.on("/api/status",HTTP_GET,[]{web.send(200,"application/json",statusJSON());});
 web.on("/api/config",HTTP_POST,[]{
  if(web.client().localIP()!=WiFi.softAPIP()){web.send(403,"application/json","{\"error\":\"configure over USB or anchor AP\"}");return;}
  DynamicJsonDocument d(1024);String err;
  if(deserializeJson(d,web.arg("plain"))||!applyConfig(d,err)){web.send(400,"application/json","{\"error\":\"invalid configuration\"}");return;}
  web.send(200,"application/json","{\"saved\":true}");
 });
 if(otaPassword.length()>=16)updater.setup(&web,"/update","admin",otaPassword.c_str());
 web.begin();
 Serial.print("{\"ready\":true,\"firmware\":\"");Serial.print(VERSION);Serial.println("\"}");
}
void loop() {
 uint32_t now=millis();
 web.handleClient();
 while(Serial.available()){
  char c=Serial.read();
  if(c=='\n'){
   if(serialLine=="status")Serial.println(statusJSON());
   else {DynamicJsonDocument d(1024);String err;
    if(!deserializeJson(d,serialLine)&&applyConfig(d,err))Serial.println("{\"saved\":true}");
    else Serial.println("{\"error\":\"invalid command\"}");
   }
   serialLine="";
  }else if(c!='\r'){if(serialLine.length()<1024)serialLine+=c;else serialLine="";}
 }
 if(provisionPending&&!radioBusy&&!scanning){provisionPending=false;WiFi.disconnect(false);WiFi.begin(wifiSSID.c_str(),wifiPass.c_str());}
 if(!wifiSSID.isEmpty()&&WiFi.status()!=WL_CONNECTED&&now-lastConnect>20000&&!radioBusy&&!scanning){lastConnect=now;WiFi.reconnect();}
 Result r;
 while(xQueueReceive(resultQueue,&r,0)==pdTRUE){
  ftmCode=r.status;
  String mac=macText(r.mac);
  if(mac!=activeMac)continue;
  int i=peerIndex(mac);
  if(i>=0){
   auto &p=peers[i];p.status=r.status==FTM_STATUS_SUCCESS?"ok":"ftm_failed";
   p.distance=r.status==FTM_STATUS_SUCCESS?float(r.cm)/100.0f-offsetM:NAN;
   p.rtt=r.rtt;p.samples=r.samples;p.measured=now;++p.reportSeq;
  }
  radioBusy=false;retiring=false;lastFTM=now+(esp_random()%1000);
 }
 if(radioBusy&&!retiring&&now-ftmStarted>8000){esp_wifi_ftm_end_session();retiring=true;}
 // Never overlap a timed-out session with a new measurement: its delayed report must retire first.
 if(!radioBusy&&!scanning&&now-lastScan>20000){
  int rc=WiFi.scanNetworks(true,false,false,150,0);scanning=rc==WIFI_SCAN_RUNNING;lastScan=now;
 }
 if(scanning){
  int n=WiFi.scanComplete();
  if(n>=0){
   for(int j=0;j<n;j++){
    String ssid=WiFi.SSID(j);if(!ssid.startsWith("C3-TOF-"))continue;
    int i=peerIndex(macText(WiFi.BSSID(j)));if(i<0)continue;
    peers[i].ssid=ssid;peers[i].channel=WiFi.channel(j);peers[i].rssi=WiFi.RSSI(j);peers[i].seen=now;
   }
   WiFi.scanDelete();scanning=false;
  }else if(n==WIFI_SCAN_FAILED){scanning=false;}
 }
 if(!radioBusy&&!scanning&&!retiring&&int32_t(now-lastFTM)>2000){
  static int cursor=0;
  for(int k=0;k<12;k++){
   int i=cursor++%12;auto &p=peers[i];if(p.mac.isEmpty()||now-p.seen>60000)continue;
   wifi_ftm_initiator_cfg_t cfg={};if(!parseMac(p.mac,cfg.resp_mac))continue;
   cfg.channel=p.channel;cfg.frm_count=16;cfg.burst_period=2;
   activeMac=p.mac;activeChannel=p.channel;ftmStarted=now;lastFTM=now;
   if(esp_wifi_ftm_initiate_session(&cfg)==ESP_OK){radioBusy=true;p.status="measuring";}
   else {p.status="request_rejected";p.distance=NAN;}
   break;
  }
 }
 // Drain network packets: this node publishes its own measurements, the coordinator fuses all nodes.
 int packet=udp.parsePacket();if(packet){char discard[256];while(udp.available())udp.read(discard,sizeof(discard));}
 if(now-lastPublish>2000){lastPublish=now;publish();}
 delay(2);
}
