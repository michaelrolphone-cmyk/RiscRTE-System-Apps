#include "Node.h"
#include "Board.h"
#include "BiomeProvider.h"
#include "HttpSchedule.h"
#include "Upgrade.h"
#include "Factory.h"
#include "NativeHttp.h"
#include "NativeTcpListener.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <new>
namespace {
using namespace Infra;
BiomeRelay::Client* provider=nullptr;
JsonDocument stagedBiome;bool stagedBiomePending=false;
void stageBiome(JsonObjectConst config){
 stagedBiome.clear();stagedBiome.set(config);stagedBiomePending=!stagedBiome.overflowed();
 if(!stagedBiomePending)ESP_LOGE("relay6ch","committed native setup could not be staged");
}
void publicIdentity(JsonObject out){
 if(provider)provider->publicIdentity(out);
 out["setup_commit_pending"]=stagedBiomePending;
}
TaskHandle_t ownerTask=nullptr;nvs_handle_t store=0;uint64_t revision=1,writeGeneration=0,pendingWrite=0;int32_t writeOutcome=0;
uint8_t initialState[1024]{};size_t initialStateSize=0;bool initializedStore=false;bool wifiStarted=false,apMode=false,joining=false;uint64_t joinStarted=0;
uint64_t now(void*){return esp_timer_get_time()/1000;}
bool owner(){return xTaskGetCurrentTaskHandle()==ownerTask;}
bool connected(){esp_netif_ip_info_t ip{};auto* sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");return sta&&esp_netif_get_ip_info(sta,&ip)==ESP_OK&&ip.ip.addr;}
bool network(){return apMode||connected();}
const char* key(const char* name){return name&&!strcmp(name,"node-state.bin")?"state":name&&!strcmp(name,"node-policy.json")?"policy":nullptr;}
const char* initial(const char* name,size_t& n){if(!strcmp(name,"node-state.bin")){n=initialStateSize;return reinterpret_cast<const char*>(initialState);}n=strlen(factory_policy);return factory_policy;}
int32_t statFile(void*,const char* name,uint32_t* size,uint64_t* rev){
 if(size)*size=0;if(rev)*rev=0;if(!owner()||!size||!rev||!key(name))return RISC_APP_DATA_CONTEXT;
 size_t n=0;auto e=nvs_get_blob(store,key(name),nullptr,&n);if(e==ESP_ERR_NVS_NOT_FOUND){if(initializedStore&&!strcmp(name,"node-state.bin"))return RISC_APP_DATA_IO;initial(name,n);}else if(e!=ESP_OK)return RISC_APP_DATA_IO;
 if(n>StateMax)return RISC_APP_DATA_IO;*size=n;*rev=revision;return 0;
}
int32_t readFile(void*,const char* name,uint64_t expected,void* out,uint32_t cap,uint32_t* size,uint64_t* rev){
 auto r=statFile(nullptr,name,size,rev);if(r)return r;if(expected!=revision){*size=0;*rev=0;return RISC_APP_DATA_STALE;}
 if(cap<*size)return RISC_APP_DATA_BUFFER_SMALL;if(*size&&!out)return RISC_APP_DATA_INVALID;
 size_t n=*size;auto e=nvs_get_blob(store,key(name),out,&n);if(e==ESP_ERR_NVS_NOT_FOUND){auto* p=initial(name,n);memcpy(out,p,n);return 0;}return e==ESP_OK?0:RISC_APP_DATA_IO;
}
int32_t replaceFile(void*,const char* name,uint64_t expected,const void* bytes,uint32_t size){
 if(!owner()||!key(name)||(!bytes&&size)||size>StateMax)return RISC_APP_DATA_CONTEXT;if(expected!=revision)return RISC_APP_DATA_STALE;
 auto e=nvs_set_blob(store,key(name),bytes,size);if(e!=ESP_OK)return RISC_APP_DATA_IO;
 // NVS uses CRC-protected append/commit records. Never erase/format on a
 // failed initialization. A failed commit is ambiguous; reload the journal.
 if(!strcmp(name,"node-state.bin")&&nvs_set_u8(store,"initialized",1)!=ESP_OK){++revision;return RISC_APP_DATA_COMMIT_UNKNOWN;}e=nvs_commit(store);if(e==ESP_OK&&!strcmp(name,"node-state.bin"))initializedStore=true;++revision;return e==ESP_OK?0:RISC_APP_DATA_COMMIT_UNKNOWN;
}
int32_t beginWrite(void* c,const char* n,uint64_t r,const void* p,uint32_t s,uint64_t* token){
 if(token)*token=0;if(!owner()||!token)return RISC_APP_DATA_CONTEXT;if(pendingWrite)return RISC_APP_DATA_BUSY;
 writeOutcome=replaceFile(c,n,r,p,s);pendingWrite=++writeGeneration;*token=pendingWrite;return 0;
}
int32_t collectWrite(void*,uint64_t t,int32_t* outcome){if(!owner()||!outcome||!t||t!=pendingWrite)return RISC_APP_DATA_CONTEXT;*outcome=writeOutcome;pendingWrite=0;return 0;}
risc_bound_app_data_async_v1 files{{1,sizeof(files),nullptr,statFile,readFile,replaceFile},RISC_BOUND_APP_DATA_ASYNC_TAG,1,beginWrite,collectWrite};
int32_t stationClose(void*){if(!owner())return -1;if(wifiStarted&&esp_wifi_stop()!=ESP_OK)return -2;wifiStarted=apMode=joining=false;return 0;}
int32_t stationConnect(void*,const char* ssid,const char* password){
 if(!owner()||!ssid||!password||strlen(ssid)>32||strlen(password)>63)return -1;
 if(connected())return 0;if(joining)return now(nullptr)-joinStarted<30000?1:-1;
 if(wifiStarted)return -1;wifi_config_t c{};memcpy(c.sta.ssid,ssid,strlen(ssid));memcpy(c.sta.password,password,strlen(password));c.sta.pmf_cfg.capable=true;
 if(esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||esp_wifi_set_config(WIFI_IF_STA,&c)!=ESP_OK||esp_wifi_start()!=ESP_OK)return -1;
 wifiStarted=true;if(esp_wifi_connect()!=ESP_OK)return -1;joining=true;joinStarted=now(nullptr);return 1;
}
int32_t stationAp(void*,const char* ssid,const char* password){
 if(!owner())return -1;if(apMode)return 0;if(wifiStarted)return -1;wifi_config_t c{};strcpy((char*)c.ap.ssid,ssid);strcpy((char*)c.ap.password,password);c.ap.ssid_len=strlen(ssid);c.ap.max_connection=2;c.ap.authmode=WIFI_AUTH_WPA2_PSK;c.ap.channel=1;
 if(esp_wifi_set_mode(WIFI_MODE_AP)!=ESP_OK||esp_wifi_set_config(WIFI_IF_AP,&c)!=ESP_OK||esp_wifi_start()!=ESP_OK)return -1;wifiStarted=apMode=true;return 0;
}
const auto* tcp=RiscCpu::NativeTcpListener::backend();
risc_tcp_listener_v1 portalApi(){
 risc_tcp_listener_v1 a{};a.api_version=1;a.struct_size=sizeof(a);
 a.listen=[](void*,const risc_tcp_listen_v1* r,uint64_t* t){return tcp->listen(nullptr,1,r,t);};
 a.accept=[](void*,uint64_t l,uint64_t* c){return tcp->accept(nullptr,1,l,c);};
 a.read=[](void*,uint64_t t,void* b,uint32_t n,uint32_t* s){return tcp->read(nullptr,1,t,b,n,s);};
 a.write=[](void*,uint64_t t,const void* b,uint32_t n,uint32_t* s){return tcp->write(nullptr,1,t,b,n,s);};
 a.close=[](void*,uint64_t t){return tcp->close(nullptr,1,t);};a.datagram_tag=RISC_DATAGRAM_TAG;
 a.datagram_bind=[](void*,const risc_tcp_listen_v1* r,uint64_t* t){return tcp->datagram_bind(nullptr,1,r,t);};
 a.datagram_receive=[](void*,uint64_t t,void* b,uint32_t n,uint32_t* s,risc_tcp_listen_v1* p){return tcp->datagram_receive(nullptr,1,t,b,n,s,p);};
 a.datagram_send=[](void*,uint64_t t,const void* b,uint32_t n,const risc_tcp_listen_v1* p){return tcp->datagram_send(nullptr,1,t,b,n,p);};return a;
}
void fail(const char* why){ESP_LOGE("relay6ch","%s; outputs remain off, storage preserved",why);for(;;)vTaskDelay(pdMS_TO_TICKS(1000));}
}
extern "C" void app_main(){
 ownerTask=xTaskGetCurrentTaskHandle();
 // Safe outputs precede Wi-Fi, flash journal loading and enrollment.
 if(!RelayBoard::begin())fail("board initialization failed");
 if(nvs_flash_init()!=ESP_OK||nvs_open("infra",NVS_READWRITE,&store)!=ESP_OK)fail("NVS unavailable; no automatic erase");
 uint8_t mac[6];if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK)fail("device identity unavailable");
 char seed[900];snprintf(seed,sizeof(seed),"{\"schema_version\":1,\"installation_id\":\"relay6ch-%02x%02x%02x%02x%02x%02x\",\"credential_generation\":1,\"credentials\":{},\"participant_id\":\"\",\"cursor\":0,\"pending\":[],\"job\":null,\"checkpoints\":{},\"configuration_revision\":1}",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
 initialStateSize=Infra::frame(initialState,sizeof(initialState),seed,strlen(seed),1);uint8_t marker=0;auto markerRead=nvs_get_u8(store,"initialized",&marker);if(markerRead!=ESP_OK&&markerRead!=ESP_ERR_NVS_NOT_FOUND)fail("journal marker corrupt");initializedStore=markerRead==ESP_OK;
 if(esp_netif_init()!=ESP_OK||esp_event_loop_create_default()!=ESP_OK||!esp_netif_create_default_wifi_sta()||!esp_netif_create_default_wifi_ap())fail("network initialization failed");
 wifi_init_config_t wi=WIFI_INIT_CONFIG_DEFAULT();if(esp_wifi_init(&wi)!=ESP_OK||esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK)fail("Wi-Fi initialization failed");
 // This powered relay endpoint receives short-lived driving intent. Modem sleep
 // defers inbound packets to AP DTIM windows and can consume the intent lifetime.
 if(esp_wifi_set_ps(WIFI_PS_NONE)!=ESP_OK)fail("Real-time Wi-Fi configuration failed");
 // Explicit AP address matches the existing captive DNS/portal contract.
 auto* ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");esp_netif_dhcps_stop(ap);esp_netif_ip_info_t ip{};IP4_ADDR(&ip.ip,192,168,4,1);IP4_ADDR(&ip.gw,192,168,4,1);IP4_ADDR(&ip.netmask,255,255,255,0);if(esp_netif_set_ip_info(ap,&ip)!=ESP_OK||esp_netif_dhcps_start(ap)!=ESP_OK)fail("setup network unavailable");
 if(!RiscCpu::NativeTlsMemory::initialize())fail("internal TLS allocation policy unavailable");
 RiscCpu::NativeHttp::configure(owner,connected);RiscCpu::NativeTcpListener::configure(owner,network);
 auto* node=new(std::nothrow) Node();if(!node)fail("agent allocation failed");
 node->net.api=*RiscCpu::NativeHttp::apiV2();node->clock={1,sizeof(node->clock),nullptr,now,[](void*,uint32_t ms){vTaskDelay(pdMS_TO_TICKS(ms));}};
 node->entropy={1,sizeof(node->entropy),nullptr,[](void*,void* b,uint32_t n)->int32_t{if(!owner()||!b||!n||n>32)return RISC_ENTROPY_INVALID;if(!wifiStarted)return RISC_ENTROPY_UNAVAILABLE;esp_fill_random(b,n);return 0;}};
 node->station={1,sizeof(node->station),nullptr,stationConnect,[](void*)->int32_t{return connected()?0:1;},stationClose,stationAp};
 node->upgradeSetup=RelayBoard::upgradeSetup;
 // Physical I/O has one owner. Legacy roles keep their own identity and
 // journal, but never receive the Board hooks or advertise physical offers.
 node->externalHardware=true;node->allowBiomeSetup=true;
 node->setupCommitted=stageBiome;node->portal.api=portalApi();node->portal.publicIdentity=publicIdentity;
 provider=new(std::nothrow) BiomeRelay::Client();
 if(!provider||!provider->begin(mac,*RiscCpu::NativeHttp::apiV2(),
    {nullptr,RelayBoard::ready,RelayBoard::execute,RelayBoard::poll,RelayBoard::cancel},now))
  fail("native provider journal or identity unavailable");
 // GPIO0 is the board's BOOT button. A hold after normal startup requests
 // setup; holding it during reset retains the chip's ROM download behavior.
 gpio_config_t button{};button.pin_bit_mask=1ULL<<GPIO_NUM_0;button.mode=GPIO_MODE_INPUT;button.pull_up_en=GPIO_PULLUP_ENABLE;button.pull_down_en=GPIO_PULLDOWN_DISABLE;button.intr_type=GPIO_INTR_DISABLE;
 if(gpio_config(&button)!=ESP_OK)fail("setup button unavailable");
 if(!node->journal.initialize(&files.base))fail("journal allocation failed");
 if(esp_task_wdt_add(nullptr)!=ESP_OK)fail("watchdog registration failed");
 // Legacy endpoint failures must not interrupt independent native hardware.
 node->recovery={1,sizeof(node->recovery),nullptr,[](void*)->int32_t{return esp_task_wdt_reset()==ESP_OK?0:-1;},nullptr,[](void*){if(!connected()&&(!provider||!provider->outputsActive()))esp_restart();}};
 unsigned last=~0u;uint64_t report=0,buttonAt=0;bool buttonReleased=false,setupRequested=false;
 BiomeRelay::HttpSchedule schedule;
 for(;;){
  constexpr unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;node->memory={};auto& m=node->memory;m.struct_size=sizeof(m);m.internal_free=heap_caps_get_free_size(caps);m.internal_largest=heap_caps_get_largest_free_block(caps);m.dma_free=heap_caps_get_free_size(caps|MALLOC_CAP_DMA);m.dma_largest=heap_caps_get_largest_free_block(caps|MALLOC_CAP_DMA);m.stack_headroom=uxTaskGetStackHighWaterMark(nullptr);
  const uint64_t at=now(nullptr);
  if(gpio_get_level(GPIO_NUM_0)){buttonReleased=true;buttonAt=0;}
  else if(buttonReleased&&!setupRequested&&node->loaded&&node->phase<Phase::SetupStart){
   if(!buttonAt)buttonAt=at;
   else if(at-buttonAt>=3000){setupRequested=true;buttonReleased=false;ESP_LOGI("relay6ch","setup requested; waiting for outstanding work");}
  }
  // Wait for physical I/O and active HTTP to settle before changing the radio.
  // Keep durable pending receipts intact so broken Wi-Fi can still be repaired.
  if(setupRequested&&!provider->busy()&&!provider->outputsActive()&&!node->net.token&&
     !node->journal.writeToken&&!node->journal.reload&&node->phase!=Phase::Save&&node->phase<Phase::SetupStart){
   const int closed=stationClose(nullptr);
   if(closed==-2)node->retained();
   else if(!closed){node->setupRecovery=true;node->portal.nonce[0]=0;node->reason(Phase::SetupStart,"physical_setup_requested");setupRequested=false;}
  }
  const bool inSetup=node->phase>=Phase::SetupStart;
  const bool hadNative=provider->busy();
  provider->tick(connected(),schedule.grantNative(node->net.token!=0,node->phase==Phase::Send,inSetup));
  if(!hadNative&&provider->busy())schedule.openedNative();
  // A confirmed saved portal snapshot is retried until the native NVS journal
  // can accept it. Never apply uncommitted request-buffer contents.
  if(stagedBiomePending&&provider->acceptConfig(stagedBiome.as<JsonObjectConst>())){
   stagedBiomePending=false;stagedBiome.clear();
  }
  node->biomeTargetLocked|=provider->enrolled();
  if(!provider->busy()){
   const bool hadNode=node->net.token!=0;node->advance();
   if(!hadNode&&node->net.token)schedule.openedNode();
  }else{
   // The native state machine owns the shared HTTP client this turn. A live
   // cooperative owner still feeds the task watchdog while Node is paused.
   esp_task_wdt_reset();
  }
  const unsigned state=unsigned(node->phase);if(state!=last||now(nullptr)-report>=15000){
   ESP_LOGI("relay6ch","state=%u reason=%s free=%llu largest=%llu dma=%llu dma_largest=%llu stack=%u reset=%u",state,node->status.reason,(unsigned long long)m.internal_free,(unsigned long long)m.internal_largest,(unsigned long long)m.dma_free,(unsigned long long)m.dma_largest,m.stack_headroom,unsigned(esp_reset_reason()));
   JsonDocument identity;provider->publicIdentity(identity.to<JsonObject>());node->biomeTargetLocked|=identity["pairing"]=="enrolled";
   ESP_LOGI("relay6ch","biomerte=%s fingerprint=%s resource=%s error=%s",identity["pairing"]|"unconfigured",identity["fingerprint"]|"",identity["resource"]|"",identity["error"]|"");
   last=state;report=now(nullptr);
  }
  if(node->phase==Phase::Retained||provider->retained()){ESP_LOGE("relay6ch","retained HTTP ownership fault; watchdog recovery");for(;;)vTaskDelay(pdMS_TO_TICKS(1000));}
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
