#include "BiomeProvider.h"
#include "../../../Services/biome_node/ProviderState.h"
#include "../../../Services/biome_node/NativeEncoding.h"
#include <nvs.h>
#include <esp_random.h>
#include <psa/crypto.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <new>

namespace BiomeRelay {
namespace {
constexpr size_t ResponseLimit=8192,JournalLimit=12288;
constexpr const char* Contracts[]={"relay.bank:1","device.outputs.batch:1","serial.rs485:1","sound.buzzer:1","light.rgb:1"};
void eraseBytes(void* memory,size_t count){auto p=static_cast<volatile uint8_t*>(memory);while(count--)*p++=0;}
bool hex64(const char* text){if(!text||strlen(text)!=64)return false;for(const char* p=text;*p;++p)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')))return false;return true;}
bool ident(const char* text){if(!text||!text[0]||strlen(text)>128)return false;for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='.'||*p==':'||*p=='-'))return false;return true;}
bool endpoint(const char* url){
 if(!url||strlen(url)>768||strncmp(url,"https://",8))return false;
 constexpr const char* suffix="/api/provider/call";const size_t size=strlen(url),tail=strlen(suffix);
 if(size<8+1+tail||strcmp(url+size-tail,suffix))return false;
 const char* slash=strchr(url+8,'/');if(!slash||slash==url+8)return false;
 for(const unsigned char* p=reinterpret_cast<const unsigned char*>(url);*p;++p)
  if(*p<=32||*p>=127||*p=='@'||*p=='?'||*p=='#'||*p=='\\')return false;
 return true;
}
bool parseResponse(JsonDocument& doc,const char* data,size_t size){
 doc.clear();return size&&size<=ResponseLimit&&Infra::BasicJsonGuard<20,256>(data,size).objectOnly()&&
  !deserializeJson(doc,data,size,DeserializationOption::NestingLimit(20))&&!doc.overflowed()&&doc["result"].is<JsonObjectConst>();
}
}

struct Client::Implementation {
 enum class Request {None,PairBegin,PairFinish,Offer,Poll,Fenced,Claim,Status,Result};
 nvs_handle_t nvs=0;risc_http_client_v2 http{};Infra::Hardware hardware{};
 uint64_t (*clock)(void*)=nullptr;void* clockContext=nullptr;
 ProviderState state;JsonDocument config,pair,pendingFence,response;
 char device[32]{},actor[65]{},publicKey[512]{},error[128]{};
 psa_key_id_t key=0;std::atomic<int> keyPhase{0};
 bool healthy=true,identityBound=false,enrolled=false,retained=false,done=false,wasHardwareReady=false,recoveryClaim=false;
 unsigned offerIndex=0,failures=0;
 Request request=Request::None;uint64_t httpToken=0,due=0,utc=0,utcAt=0,started=0;
 char* body=nullptr;size_t bodySize=0;risc_http_response_v2 info{};int networkOutcome=0;

 uint64_t now()const{return clock?clock(clockContext):0;}
 bool fail(const char* why,bool permanent=false){Infra::copy(error,sizeof(error),why);if(permanent)healthy=false;return false;}
 bool saveJson(const char* name,JsonObjectConst value,size_t limit){
  const size_t size=measureJson(value);if(!size||size>limit)return false;
  char* bytes=static_cast<char*>(malloc(size+1));if(!bytes)return false;
  const bool encoded=serializeJson(value,bytes,size+1)==size;
  bool result=encoded&&nvs_set_blob(nvs,name,bytes,size)==ESP_OK&&nvs_commit(nvs)==ESP_OK;
  eraseBytes(bytes,size+1);free(bytes);return result;
 }
 int loadJson(const char* name,JsonDocument& value,size_t limit){
  size_t size=0;esp_err_t rc=nvs_get_blob(nvs,name,nullptr,&size);
  if(rc==ESP_ERR_NVS_NOT_FOUND)return 0;if(rc!=ESP_OK||!size||size>limit)return -1;
  char* bytes=static_cast<char*>(malloc(size+1));if(!bytes)return -1;
  rc=nvs_get_blob(nvs,name,bytes,&size);bytes[size]=0;
  const bool parsed=rc==ESP_OK&&Infra::parseJournal(value,bytes,size);
  eraseBytes(bytes,size+1);free(bytes);return parsed?1:-1;
 }
 static bool saveState(void* context,JsonObjectConst value){
  return static_cast<Implementation*>(context)->saveJson("state",value,JournalLimit);
 }
 bool normalizeConfig(JsonObjectConst input,JsonDocument& out){
  if(!Infra::keys(input,{"url"},{"site","realm","resource"})||measureJson(input)>2048||
     !endpoint(input["url"]|"")||(!input["realm"].isNull()&&input["realm"]!="site"))return false;
  const char* site=input["site"]|"";if(site[0]&&!ident(site))return false;
  const char* resource=input["resource"]|device;
  if(!ident(resource))return false;
  out.clear();out["url"]=input["url"];out["site"]=site;out["realm"]="site";out["resource"]=resource;
  return !out.overflowed();
 }
 bool configured()const{return endpoint(config["url"]|"");}
 bool physicalReady()const{return hardware.ready&&hardware.ready(hardware.context);}
 bool ready()const{return keyPhase.load(std::memory_order_acquire)==2&&healthy&&state.healthy&&identityBound&&enrolled&&offerIndex==5&&physicalReady();}
 bool busy()const{return httpToken||retained||keyPhase.load(std::memory_order_acquire)==1;}

 bool initializeIdentity(bool create){
  if(psa_crypto_init()!=PSA_SUCCESS)return fail("crypto_initialization_failed",true);
  psa_key_attributes_t attributes=PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_type(&attributes,PSA_KEY_TYPE_RSA_KEY_PAIR);psa_set_key_bits(&attributes,2048);
  psa_set_key_usage_flags(&attributes,PSA_KEY_USAGE_SIGN_HASH|PSA_KEY_USAGE_EXPORT);
  psa_set_key_algorithm(&attributes,PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));
  size_t size=0;esp_err_t loaded=nvs_get_blob(nvs,"rsa_private",nullptr,&size);
  psa_status_t status=PSA_ERROR_GENERIC_ERROR;
  if(loaded==ESP_ERR_NVS_NOT_FOUND){
   if(!create){psa_reset_key_attributes(&attributes);return true;}
   status=psa_generate_key(&attributes,&key);
   if(status==PSA_SUCCESS){
    uint8_t* privateBytes=static_cast<uint8_t*>(malloc(2048));size_t written=0;
    bool stored=privateBytes&&psa_export_key(key,privateBytes,2048,&written)==PSA_SUCCESS&&
     nvs_set_blob(nvs,"rsa_private",privateBytes,written)==ESP_OK&&nvs_commit(nvs)==ESP_OK;
    if(privateBytes){eraseBytes(privateBytes,2048);free(privateBytes);}
    if(!stored){psa_destroy_key(key);key=0;status=PSA_ERROR_STORAGE_FAILURE;}
   }
  }else if(loaded==ESP_OK&&size&&size<=2048){
   uint8_t* privateBytes=static_cast<uint8_t*>(malloc(size));
   if(privateBytes){
    if(nvs_get_blob(nvs,"rsa_private",privateBytes,&size)==ESP_OK)status=psa_import_key(&attributes,privateBytes,size,&key);
    eraseBytes(privateBytes,size);free(privateBytes);
   }
  }
  psa_reset_key_attributes(&attributes);
  if(status!=PSA_SUCCESS)return fail("device_key_unavailable",true);
  uint8_t rsa[512],spki[544];size_t rsaSize=0;
  if(psa_export_public_key(key,rsa,sizeof(rsa),&rsaSize)!=PSA_SUCCESS)return fail("public_key_export_failed",true);
  const size_t spkiSize=rsaSpki(rsa,rsaSize,spki,sizeof(spki));
  if(!spkiSize||!publicPem(spki,spkiSize,publicKey,sizeof(publicKey)))return fail("public_key_encoding_failed",true);
  bytesDigest(spki,spkiSize,actor);return true;
 }
 static void identityTask(void* context){
  auto self=static_cast<Implementation*>(context);
  const bool ok=self->initializeIdentity(true);
  self->keyPhase.store(ok?2:-1,std::memory_order_release);vTaskDelete(nullptr);
 }
 bool bindIdentity(){
  if(identityBound)return true;
  if(!key||!hex64(actor)||!state.identity(actor,config["resource"]|device))return fail("identity_binding_failed",true);
  if(!pair.isNull()&&!pair["key"].isNull()&&pair["key"]!=actor)return fail("pairing_identity_mismatch",true);
  identityBound=true;enrolled=pair["enrolled"]==true;return true;
 }
 bool signature(JsonVariantConst value,char out[345]){
  uint8_t digest[32],signedBytes[256];size_t size=0;
  if(!nativeDigest(value,digest)||psa_sign_hash(key,PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256),
      digest,sizeof(digest),signedBytes,sizeof(signedBytes),&size)!=PSA_SUCCESS||size!=256)
   return fail("native_signature_failed",true);
  return base64(signedBytes,size,out,345)!=0;
 }
 bool signedCall(const char* operation,JsonObjectConst arguments,JsonDocument& requestBody){
  if(!ident(config["site"]|""))return fail("site_not_paired",true);
  char id[33];uint8_t random[16];esp_fill_random(random,sizeof(random));const char* hex="0123456789abcdef";
  for(unsigned i=0;i<16;++i){id[2*i]=hex[random[i]>>4];id[2*i+1]=hex[random[i]&15];}id[32]=0;
  requestBody["site"]=config["site"];requestBody["realm"]="site";requestBody["id"]=id;
  requestBody["operation"]=operation;requestBody["arguments"]=arguments;
  char proof[345];if(!signature(requestBody.as<JsonVariantConst>(),proof))return false;
  requestBody["auth"]["actor"]=actor;requestBody["auth"]["signature"]=proof;return !requestBody.overflowed();
 }
 void backoff(const char* why){
  fail(why);due=now()+Infra::retryDelay(failures<10?++failures:10,esp_random());
 }
 bool open(Request kind,const char* url,JsonObjectConst payload){
  if(httpToken||retained)return false;
  const size_t size=measureJson(payload);if(!size||size>ResponseLimit)return fail("native_request_too_large",true);
  char* encoded=static_cast<char*>(malloc(size+1));body=static_cast<char*>(malloc(ResponseLimit+1));
  if(!encoded||!body){free(encoded);free(body);body=nullptr;backoff("native_http_memory_unavailable");return false;}
  serializeJson(payload,encoded,size+1);body[0]=0;bodySize=0;done=false;networkOutcome=0;
  info={};info.struct_size=sizeof(info);const uint64_t current=now(),wall=utc?utc+(current-utcAt)/1000:0;
  const risc_http_header_v2 header{"Content-Type","application/json"};
  const risc_http_request_v2 wire{sizeof(wire),RISC_HTTP_POST_PUBLIC,url,&header,1,encoded,
    static_cast<uint32_t>(size),ResponseLimit,Infra::ServiceRequestTimeoutMs,wall,wall?nullptr:"pool.ntp.org",0};
  int rc=http.open(http.context,&wire,&httpToken);eraseBytes(encoded,size+1);free(encoded);
  if(rc==RISC_HTTP_RETAINED){retained=true;fail("native_http_cleanup_retained",true);return false;}
  if(rc<0||rc==RISC_HTTP_BUSY){httpToken=0;free(body);body=nullptr;if(rc!=RISC_HTTP_BUSY)backoff("native_http_open_failed");return false;}
  request=kind;started=current;return true;
 }
 bool call(Request kind,const char* operation,JsonObjectConst args){
  JsonDocument payload;if(!signedCall(operation,args,payload))return false;
  return open(kind,config["url"]|"",payload.as<JsonObjectConst>());
 }
 bool pairRequest(bool finish){
  char url[800];Infra::copy(url,sizeof(url),config["url"]|"");
  const char* suffix=finish?"pair/finish":"pair/begin";char* tail=strrchr(url,'/');
  if(!tail)return fail("provider_endpoint_invalid",true);
  const size_t prefix=tail-url+1;if(prefix+strlen(suffix)>=sizeof(url))return fail("pair_endpoint_too_long",true);
  strcpy(url+prefix,suffix);JsonDocument payload;
  if(finish){
   payload["challenge"]=pair["challenge"];payload["site"]=pair["site"];payload["key"]=pair["key"];
   char proof[345];if(!signature(payload.as<JsonVariantConst>(),proof))return false;payload["signature"]=proof;
  }else{
   payload["public_key"]=publicKey;char label[64];snprintf(label,sizeof(label),"Biome Relay %s",device);payload["label"]=label;
  }
  return open(finish?Request::PairFinish:Request::PairBegin,url,payload.as<JsonObjectConst>());
 }
 bool normalizedFence(JsonObjectConst source,bool action,JsonDocument& out){
  if(source["provider"]!=actor||source["resource"]!=config["resource"])return false;
  out.clear();out["resource"]=source["resource"];out["guard"]=source["guard"];
  out["lease"]=source[action?"lease":"id"];out["generation"]=source["generation"];
  return Infra::text(out["lease"],128)&&Infra::text(out["guard"],512)&&Infra::integer(out["generation"],1,9007199254740991ULL);
 }
 void received(Request kind,JsonObjectConst result){
  switch(kind){
  case Request::PairBegin:{
   if(!hex64(result["challenge"]|"")||!ident(result["site"]|"")||result["key"]!=actor||
      (strlen(config["site"]|"")&&config["site"]!=result["site"])){fail("pairing_response_mismatch",true);return;}
   pair.clear();pair["challenge"]=result["challenge"];pair["site"]=result["site"];pair["key"]=actor;pair["enrolled"]=false;pair["url"]=config["url"];pair["resource"]=config["resource"];
   if(!saveJson("pair",pair.as<JsonObjectConst>(),2048)){fail("pairing_storage_failed",true);return;}
   config["site"]=result["site"];if(!saveJson("config",config.as<JsonObjectConst>(),2048))fail("configuration_storage_failed",true);
   break;}
  case Request::PairFinish:
   if(result["site"]!=pair["site"]||result["credential"]!=actor){fail("pairing_completion_mismatch",true);return;}
   pair["enrolled"]=true;
   if(!saveJson("pair",pair.as<JsonObjectConst>(),2048)){fail("pairing_storage_failed",true);return;}
   enrolled=true;break;
  case Request::Offer:
   if(result["provider"]!=actor||result["resource"]!=config["resource"]||result["contract"]!=Contracts[offerIndex]){
    backoff("capability_offer_mismatch");return;
   }
   ++offerIndex;break;
  case Request::Fenced:{
   JsonDocument normalized;
   if(result["state"]!="bound"||!normalizedFence(result,false,normalized)||
      normalized.as<JsonObjectConst>()!=pendingFence.as<JsonObjectConst>()){backoff("fence_acknowledgement_mismatch");return;}
   pendingFence.clear();break;}
  case Request::Poll:{
   if(!result["fences"].is<JsonArrayConst>()||!result["actions"].is<JsonArrayConst>()){backoff("provider_poll_invalid");return;}
   for(JsonObjectConst fence:result["fences"].as<JsonArrayConst>()){
    JsonDocument normalized;if(!normalizedFence(fence,false,normalized)){backoff("provider_fence_invalid");return;}
    pendingFence.set(normalized);return;
   }
   for(JsonObjectConst action:result["actions"].as<JsonArrayConst>()){
    if(state.pending()&&state.journal["command"]["id"]!=action["id"])continue;
    if(!state.bound(action)){
     JsonDocument normalized;if(!normalizedFence(action,true,normalized)){backoff("provider_action_binding_invalid");return;}
     pendingFence.set(normalized);return;
    }
    if(!state.stage(action)){backoff(state.error);return;}break;
   }
   if(!state.needsClaim()&&!state.outcome())due=now()+1000;
   break;}
  case Request::Claim:
   if(!state.claimed(result)){backoff(state.error);return;}recoveryClaim=false;break;
  case Request::Status:
   if(!state.matches(result)){backoff("status_identity_mismatch");return;}
   if(result["state"]=="queued"){
    // A reset between durable intent and claim never grants a new execution
    // intent. Claim only enables publishing the recovered uncertain receipt.
    // Defer the actual request to tick, preserving shared HTTP ownership.
    recoveryClaim=true;
   }else if(!state.claimed(result)){backoff(state.error);return;}
   break;
  case Request::Result:
   if(!state.reportAccepted(result)){backoff(state.error);return;}break;
  default:break;
  }
 }
 void advanceHttp(){
  if(retained||!httpToken)return;
  if(!done){
   int rc=http.advance(http.context,httpToken);char chunk[512];uint32_t count=0;
   int read=http.read(http.context,httpToken,chunk,sizeof(chunk),&count);
   int queried=http.info(http.context,httpToken,&info);
   if(rc==RISC_HTTP_RETAINED||read==RISC_HTTP_RETAINED||queried==RISC_HTTP_RETAINED||info.cleanup_retained){
    retained=true;fail("native_http_cleanup_retained",true);return;
   }
   if(count>sizeof(chunk)||bodySize+count>ResponseLimit){networkOutcome=RISC_HTTP_SIZE;done=true;}
   else if(count){memcpy(body+bodySize,chunk,count);bodySize+=count;body[bodySize]=0;}
   if(info.utc_seconds&&!utc){utc=info.utc_seconds;utcAt=now();}
   if(rc<0||read<0||queried<0){networkOutcome=rc<0?rc:read<0?read:queried;done=true;}
   else if(rc==RISC_HTTP_EOF||read==RISC_HTTP_EOF)done=true;
   if(now()-started>=Infra::ServiceRequestTimeoutMs+1000){networkOutcome=RISC_HTTP_TIMEOUT;done=true;}
  }
  if(!done)return;
  int closed=http.close(http.context,httpToken);
  if(closed==RISC_HTTP_AGAIN)return;
  if(closed==RISC_HTTP_RETAINED||closed<0){retained=true;fail("native_http_cleanup_retained",true);return;}
  httpToken=0;Request completed=request;request=Request::None;
  const bool valid=networkOutcome==0&&info.status_code>=200&&info.status_code<300&&parseResponse(response,body,bodySize);
  eraseBytes(body,ResponseLimit+1);free(body);body=nullptr;
  if(!valid){
   // All provider operations are keyed by durable native action/binding IDs.
   // A lost claim or result response cannot cause the board effect to replay.
   backoff(completed==Request::PairFinish&&info.status_code==403?"pairing_awaiting_approval":"native_request_not_acknowledged");
   return;
  }
  failures=0;due=now();error[0]=0;received(completed,response["result"].as<JsonObjectConst>());response.clear();
 }
 void tick(bool connected,bool mayOpen){
  state.tick();
  if(httpToken){advanceHttp();return;}
  int phase=keyPhase.load(std::memory_order_acquire);
  if(phase==1||retained||!healthy||!state.healthy)return;
  if(phase==2&&!bindIdentity())return;
  if(!connected||!mayOpen||!configured()||now()<due)return;
  if(phase==0){
   keyPhase.store(1,std::memory_order_release);
   if(xTaskCreate(identityTask,"biome-key",8192,this,tskIDLE_PRIORITY+1,nullptr)!=pdPASS){keyPhase.store(0);backoff("device_key_task_unavailable");}
   return;
  }
  if(phase<0||!identityBound)return;
  if(pair["challenge"].isNull()){pairRequest(false);return;}
  if(!enrolled){pairRequest(true);return;}
  const bool hwReady=physicalReady();if(hwReady!=wasHardwareReady){wasHardwareReady=hwReady;offerIndex=0;}
  if(offerIndex<5){
   JsonDocument args;args["provider"]=actor;args["contract"]=Contracts[offerIndex];args["resource"]=config["resource"];
   args["state"]=hwReady?"ready":"offline";args["capacity"]=1;
   call(Request::Offer,"cap.offer",args.as<JsonObjectConst>());return;
  }
  if(!pendingFence.isNull()){
   const int fenced=state.fence(pendingFence.as<JsonObjectConst>());
   if(fenced<0){backoff(state.error);pendingFence.clear();return;}if(fenced>0)return;
   JsonDocument args;args["provider"]=actor;args["lease"]=pendingFence["lease"];args["generation"]=pendingFence["generation"];
   call(Request::Fenced,"cap.fenced",args.as<JsonObjectConst>());return;
  }
  if(state.outcome()||state.needsClaim()){
   auto command=state.journal["command"].as<JsonObjectConst>();
   JsonDocument args;args["provider"]=actor;args["operation"]=command["id"];args["generation"]=command["generation"];
   if(state.needsClaim()||recoveryClaim){call(Request::Claim,"provider.claim",args.as<JsonObjectConst>());return;}
   if(command["claimed"]!=true){args.clear();args["operation"]=command["id"];call(Request::Status,"cap.status",args.as<JsonObjectConst>());return;}
   args["state"]=command["state"];args["result"]=command["result"];
   call(Request::Result,"provider.result",args.as<JsonObjectConst>());return;
  }
  JsonDocument args;args["provider"]=actor;args["limit"]=1;call(Request::Poll,"provider.poll",args.as<JsonObjectConst>());
 }
};

bool Client::begin(const uint8_t mac[6],const risc_http_client_v2& http,Infra::Hardware hardware,uint64_t (*clock)(void*),void* context){
 if(impl||!mac||!clock||http.api_version!=RISC_HTTP_CLIENT_API_V2||!http.open||!http.advance||!http.read||!http.info||!http.close)return false;
 impl=new(std::nothrow) Implementation;if(!impl)return false;
 impl->http=http;impl->hardware=hardware;impl->clock=clock;impl->clockContext=context;
 snprintf(impl->device,sizeof(impl->device),"relay6ch.%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
 if(nvs_open("biome-relay",NVS_READWRITE,&impl->nvs)!=ESP_OK)return impl->fail("provider_storage_unavailable",true);
 JsonDocument saved;const int configRead=impl->loadJson("config",saved,2048);
 if(configRead<0)return impl->fail("provider_configuration_invalid",true);
 if(configRead>0){if(!impl->normalizeConfig(saved.as<JsonObjectConst>(),impl->config))return impl->fail("provider_configuration_invalid",true);}
 else {impl->config["url"]="";impl->config["site"]="";impl->config["realm"]="site";impl->config["resource"]=impl->device;}
 if(impl->loadJson("pair",impl->pair,2048)<0)return impl->fail("provider_pairing_invalid",true);
 if(!impl->pair.isNull()){
  if(!hex64(impl->pair["challenge"]|"")||!ident(impl->pair["site"]|"")||!hex64(impl->pair["key"]|"")||!impl->pair["enrolled"].is<bool>())
   return impl->fail("provider_pairing_invalid",true);

 }
 saved.clear();if(impl->loadJson("state",saved,JournalLimit)<0)return impl->fail("provider_journal_invalid",true);
 if(!impl->state.begin({impl,Implementation::saveState},hardware,saved.as<JsonObjectConst>()))return impl->fail(impl->state.error,true);
 if(!impl->pair.isNull()){
  const bool different=impl->pair["url"]!=impl->config["url"]||impl->pair["resource"]!=impl->config["resource"]||
   (strlen(impl->config["site"]|"")&&impl->config["site"]!=impl->pair["site"]);
  if(different){
   if(!impl->state.journal["fence"].isNull())return impl->fail("provider_pairing_site_mismatch",true);
   impl->pair.clear();
  }else impl->config["site"]=impl->pair["site"];
 }
 if(!impl->initializeIdentity(false))return false;
 if(impl->key){impl->keyPhase.store(2);if(!impl->bindIdentity())return false;}
 impl->wasHardwareReady=impl->physicalReady();return true;
}
void Client::tick(bool connected,bool mayOpen){if(impl)impl->tick(connected,mayOpen);}
bool Client::busy()const{return impl&&impl->busy();}
bool Client::retained()const{return impl&&impl->retained;}
bool Client::effectsBusy()const{return impl&&(impl->state.pending()||impl->state.active());}
bool Client::outputsActive()const{return impl&&impl->state.active();}
bool Client::configured()const{return impl&&impl->configured();}
bool Client::ready()const{return impl&&impl->ready();}
bool Client::enrolled()const{return impl&&impl->enrolled;}
bool Client::acceptConfig(JsonObjectConst input){
 if(!impl)return false;
 JsonDocument candidate;if(!impl->normalizeConfig(input,candidate))return impl->fail("provider_configuration_invalid");
 // Omitted site is an onboarding default, not a request to erase enrollment.
 if(!strlen(candidate["site"]|"")&&candidate["url"]==impl->config["url"])candidate["site"]=impl->config["site"];
 if(candidate.as<JsonObjectConst>()==impl->config.as<JsonObjectConst>())return true;
 if(impl->busy()||!impl->healthy||impl->state.pending())return false;
 const bool targetChanged=candidate["url"]!=impl->config["url"]||candidate["site"]!=impl->config["site"]||
  candidate["resource"]!=impl->config["resource"];
 if(targetChanged&&(impl->enrolled||!impl->state.journal["fence"].isNull()))return impl->fail("provider_target_locked_by_fence");
 if(!impl->saveJson("config",candidate.as<JsonObjectConst>(),2048))return impl->fail("provider_configuration_storage_failed",true);
 impl->config.set(candidate);impl->offerIndex=0;impl->due=0;impl->failures=0;impl->error[0]=0;
 if(targetChanged){
  // Only unbound onboarding can change target. Durable keys are never erased.
  if(nvs_erase_key(impl->nvs,"pair")!=ESP_OK&& !impl->pair.isNull())return impl->fail("provider_pairing_reset_failed",true);
  if(nvs_commit(impl->nvs)!=ESP_OK)return impl->fail("provider_pairing_reset_failed",true);
  impl->pair.clear();impl->enrolled=false;impl->identityBound=false;
 }
 return true;
}
void Client::publicIdentity(JsonObject out)const{
 if(!impl){out["error"]="provider_not_initialized";return;}
 out["device"]=impl->device;out["firmware"]="2.0.0";out["resource"]=impl->config["resource"];out["site"]=impl->config["site"];
 out["url"]=impl->config["url"];out["configured"]=impl->configured();out["ready"]=impl->ready();
 const int phase=impl->keyPhase.load(std::memory_order_acquire);
 if(phase!=1){out["public_key"]=impl->publicKey;out["fingerprint"]=impl->actor;out["error"]=!impl->state.healthy&&impl->state.error[0]?impl->state.error:impl->error;}
 out["pairing"]=!impl->configured()?"unconfigured":phase!=2?"identity_pending":impl->enrolled?"enrolled":
  impl->pair["challenge"].isNull()?"requesting_approval":"awaiting_approval";
 if(!impl->state.journal["fence"].isNull())out["generation"]=impl->state.journal["fence"]["generation"];
 if(impl->state.pending()){
  out["operation"]=impl->state.journal["command"]["id"];out["operation_state"]=impl->state.journal["command"]["state"];
 }
}
}
