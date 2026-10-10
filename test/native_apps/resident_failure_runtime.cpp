#include "bootstrap/Runtime.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
extern "C" {
void failure_setup(const char*,const char*);
void failure_verify(bool);
bool failure_health(risc_runtime_health_v1*);
void failure_delay(uint32_t);
bool failure_log(const char*);
bool failure_prior(risc_resident_failure_v1*);
int32_t failure_kv_get(uint32_t,const char*,void*,uint32_t,uint32_t*);
int32_t failure_kv_put(uint32_t,const char*,const void*,uint32_t);
}
namespace {
RiscBoot::Runtime* runtime=nullptr;
RiscBoot::Runtime* volatile retainedRuntime=nullptr;
bool owner(){return true;}
int32_t get(void*,uint32_t ns,const char*k,void*out,uint32_t cap,uint32_t*used){return failure_kv_get(ns,k,out,cap,used);}
int32_t put(void*,uint32_t ns,const char*k,const void*data,uint32_t size){return failure_kv_put(ns,k,data,size);}
RiscBoot::KeyValueBackend backend{nullptr,get,put};
void save(const std::string&root,const std::string&name,const JsonDocument&doc){std::string text;serializeJson(doc,text);std::ofstream(root+"/"+name)<<text;}
void setup(const std::string&root){
 const char*caps[]={"display.output","input.touch.raw","input.navigation","board.battery","rtc.clock","alarm.service"};
 const unsigned versions[]={1,1,1,1,2,1};
 JsonDocument board;board["schema"]="riscrte.board-hardware";board["schema_version"]=1;board["board_id"]="test";board["revision"]="unspecified";board["buses"].to<JsonArray>();auto devices=board["devices"].to<JsonArray>();
 JsonDocument boot;boot["board"]="board.json";boot["default_app"]="host.elf";boot["provider_activation"]="demand";auto drivers=boot["drivers"].to<JsonArray>();
 for(unsigned i=0;i<6;++i){
  const std::string name="provider-"+std::to_string(i+1);auto selected=drivers.add<JsonObject>();selected["manifest"]=name+".json";selected["instance_id"]=i+2;
  auto device=devices.add<JsonObject>();device["instance_id"]=i+2;device["chip"]["vendor"]="test";device["chip"]["model"]="failure";device["chip"]["revision"]="unspecified";device["compatible"]="test,failure";device["config_type"]="gpio.bank";device["config_version"]=1;
  auto config=device["config"].to<JsonObject>();config["pins"].to<JsonArray>().add(10+i);config["active_high"]=true;config["pull_up"]=false;config["debounce_us"]=0;config["long_press_us"]=0;config["click_min_us"]=0;
  JsonDocument provider;provider["type"]="driver";provider["id"]=caps[i];provider["version"]="1.0.0";provider["driver_abi"]=2;provider["architecture"]="xtensa-esp32s3";provider["file_name"]=name+".elf";
  auto req=provider["requires"].to<JsonArray>().add<JsonObject>();req["capability"]="hardware.device";req["api"]=1;
  auto provides=provider["provides"].to<JsonArray>().add<JsonObject>();provides["capability"]=caps[i];provides["api"]=versions[i];
  auto compat=provider["hardware_compatibility"].to<JsonArray>().add<JsonObject>();compat["compatible"]="test,failure";compat["revisions"].to<JsonArray>().add("unspecified");compat["config_type"]="gpio.bank";compat["config_version"]=1;
  save(root,name+".json",provider);
 }
 auto resident=boot["resident_shell"].to<JsonObject>();resident["api"]=1;resident["host"]="host.elf";resident["foreground"].to<JsonArray>().add("client.elf");resident["legacy"].to<JsonArray>();
 auto policies=boot["app_capabilities"].to<JsonArray>();
 for(const char*name:{"host","client"}){
  JsonDocument app;app["type"]="application";app["id"]=name;app["version"]="1.0.0";app["architecture"]="xtensa-esp32s3";app["file_name"]=std::string(name)+".elf";app["entry"]="app_main";
  auto reqs=app["requires"].to<JsonArray>();auto policy=policies.add<JsonObject>();policy["manifest"]=std::string(name)+".json";auto grants=policy["grants"].to<JsonArray>();
  auto add=[&](const char*cap,unsigned version,unsigned instance){auto req=reqs.add<JsonObject>();req["capability"]=cap;req["api"]=version;auto grant=grants.add<JsonObject>();grant["capability"]=cap;grant["api"]=version;grant["instance_id"]=instance;};
  if(std::string(name)=="host")for(unsigned i=0;i<6;++i)add(caps[i],versions[i],i+2);
  add("storage.key-value",1,1);save(root,std::string(name)+".json",app);
 }
 save(root,"board.json",board);save(root,"boot.json",boot);
}
}
extern "C" bool failure_runtime_retained(){return runtime&&runtime->retained();}
int main(int argc,char**argv){
 assert(argc==3);failure_setup(argv[2],argv[1]);setup(argv[1]);
 RiscBoot::Port port{owner,failure_health,failure_delay,failure_log,nullptr,&backend};port.priorFailure=failure_prior;
 runtime=new RiscBoot::Runtime(port);
 if(!runtime->prepare(argv[1])){fprintf(stderr,"Failure fixture admission: %s\n",runtime->error());return 1;}
 const bool ran=runtime->run();if(!ran&&!runtime->retained()){fprintf(stderr,"Failure fixture execution: %s\n",runtime->error());return 2;}
 failure_verify(runtime->retained());if(runtime->retained())retainedRuntime=runtime;else{delete runtime;runtime=nullptr;}
}
