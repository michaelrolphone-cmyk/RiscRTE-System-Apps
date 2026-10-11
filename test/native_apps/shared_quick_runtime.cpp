#include "bootstrap/Runtime.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
extern "C" {
void policy_fixture_setup(const char*);
void policy_fixture_verify(bool);
bool reference_health(risc_runtime_health_v1*);
void policy_fixture_delay(uint32_t);
bool policy_fixture_log(const char*);
bool policy_fixture_safe();
int32_t reference_kv_get(uint32_t,const char*,void*,uint32_t,uint32_t*);
int32_t reference_kv_put(uint32_t,const char*,const void*,uint32_t);
}
namespace {
RiscBoot::Runtime* runtime=nullptr;
RiscBoot::Runtime* volatile retainedRuntime=nullptr;
bool owner(){return true;}
int32_t get(void*,uint32_t ns,const char*key,void*out,uint32_t cap,uint32_t*used){return reference_kv_get(ns,key,out,cap,used);}
int32_t put(void*,uint32_t ns,const char*key,const void*bytes,uint32_t size){return reference_kv_put(ns,key,bytes,size);}
RiscBoot::KeyValueBackend backend{nullptr,get,put};
void save(const std::string&root,const std::string&name,const JsonDocument&doc){std::string text;serializeJson(doc,text);std::ofstream(root+"/"+name)<<text;}
void setup(const std::string&root){
 const char*caps[]={"display.output","input.touch.raw","input.navigation","board.battery","rtc.clock","alarm.service","net.wifi","bluetooth.hci","storage.volume","software.update.apps","software.update.firmware","usb.device.msc"};
 const unsigned versions[]={1,1,1,1,2,2,1,1,1,1,1,1},instances[]={3,4,5,10,7,8,15,16,9,20,21,22};
 JsonDocument board;board["schema"]="riscrte.board-hardware";board["schema_version"]=1;board["board_id"]="test";board["revision"]="unspecified";board["buses"].to<JsonArray>();auto devices=board["devices"].to<JsonArray>();
 JsonDocument boot;boot["board"]="board.json";boot["default_app"]="host.elf";boot["provider_activation"]="demand";
 auto drivers=boot["drivers"].to<JsonArray>();
 for(unsigned i=0;i<12;++i){
  const std::string name="provider-"+std::to_string(i+1);auto selected=drivers.add<JsonObject>();selected["manifest"]=name+".json";selected["instance_id"]=instances[i];
  auto device=devices.add<JsonObject>();device["instance_id"]=instances[i];device["chip"]["vendor"]="test";device["chip"]["model"]="policy";device["chip"]["revision"]="unspecified";device["compatible"]="test,policy";device["config_type"]="gpio.bank";device["config_version"]=1;
  auto config=device["config"].to<JsonObject>();config["pins"].to<JsonArray>().add(10+i);config["active_high"]=true;config["pull_up"]=false;config["debounce_us"]=0;config["long_press_us"]=0;config["click_min_us"]=0;
  JsonDocument provider;provider["type"]="driver";provider["id"]=caps[i];provider["version"]="1.0.0";provider["driver_abi"]=2;provider["architecture"]="xtensa-esp32s3";provider["file_name"]=name+".elf";
  auto req=provider["requires"].to<JsonArray>().add<JsonObject>();req["capability"]="hardware.device";req["api"]=1;
  auto provides=provider["provides"].to<JsonArray>().add<JsonObject>();provides["capability"]=caps[i];provides["api"]=versions[i];
  auto compat=provider["hardware_compatibility"].to<JsonArray>().add<JsonObject>();compat["compatible"]="test,policy";compat["revisions"].to<JsonArray>().add("unspecified");compat["config_type"]="gpio.bank";compat["config_version"]=1;
  save(root,name+".json",provider);
 }
 auto resident=boot["resident_shell"].to<JsonObject>();resident["api"]=1;resident["host"]="host.elf";auto foreground=resident["foreground"].to<JsonArray>();foreground.add("client.elf");foreground.add("receiver.elf");foreground.add("usb_sd_transfer.elf");
 auto policies=boot["app_capabilities"].to<JsonArray>();
 for(const char*name:{"host","client","receiver","usb_sd_transfer"}){
  JsonDocument app;app["type"]="application";app["id"]=name;app["version"]="1.0.0";app["architecture"]="xtensa-esp32s3";app["file_name"]=std::string(name)+".elf";app["entry"]="app_main";
  auto reqs=app["requires"].to<JsonArray>();auto policy=policies.add<JsonObject>();policy["manifest"]=std::string(name)+".json";auto grants=policy["grants"].to<JsonArray>();
  auto add=[&](const char*cap,unsigned version,unsigned instance){if(!(std::string(cap)=="storage.key-value"&&instance==6)){auto req=reqs.add<JsonObject>();req["capability"]=cap;req["api"]=version;}auto grant=grants.add<JsonObject>();grant["capability"]=cap;grant["api"]=version;grant["instance_id"]=instance;};
  for(unsigned i=0;i<12;++i)add(caps[i],versions[i],instances[i]);
  add("storage.key-value",1,1);add("storage.key-value",1,6);add("file.open",1,0);if(std::string(name)=="receiver")app["supported_file_types"].to<JsonArray>().add(".txt");save(root,std::string(name)+".json",app);
 }
 save(root,"board.json",board);save(root,"boot.json",boot);
}
}
extern "C" bool reference_runtime_retained(){return runtime&&runtime->retained();}
extern "C" unsigned policy_runtime_role(){
 if(!runtime)return 0;
 risc_resident_client_v1 client{};client.struct_size=sizeof(client);
 return runtime->residentClient(&client)?client.role:0;
}
extern "C" bool policy_runtime_reset_safe(){return runtime&&runtime->residentResetSafe();}
int main(int argc,char**argv){
 assert(argc==3);setup(argv[1]);policy_fixture_setup(argv[2]);
 runtime=new RiscBoot::Runtime({owner,reference_health,policy_fixture_delay,policy_fixture_log,nullptr,&backend,policy_fixture_safe});
 if(!runtime->prepare(argv[1])){std::fprintf(stderr,"System integration admission: %s\n",runtime->error());return 1;}
 const bool ran=runtime->run();
 if(!ran&&!runtime->retained()){std::fprintf(stderr,"System integration execution: %s\n",runtime->error());return 2;}
 policy_fixture_verify(runtime->retained());
 if(runtime->retained())retainedRuntime=runtime;else delete runtime;
}
