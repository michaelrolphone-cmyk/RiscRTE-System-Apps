/* Host admission qualification using the selected, unmodified Runtime.
 * Provider fixtures below perform no radio I/O and are not device qualification. */
#if defined(QUICK_MANIFEST_APP)
extern "C" void quick_remote_manifest_entry();
extern "C" __attribute__((visibility("default"))) void app_main() { quick_remote_manifest_entry(); }
#elif defined(QUICK_MANIFEST_PROVIDER)
#include <RiscProviderV2.h>
#include <RiscBluetoothSensorsV1.h>
extern "C" void quick_remote_manifest_sensor_call(unsigned operation);
#if defined(QUICK_MANIFEST_SENSOR)
namespace {
bool opened=false;
bool open(void*,uint64_t* token) { quick_remote_manifest_sensor_call(0);opened=true;*token=1;return true; }
bool poll(void*,uint64_t token,uint32_t) { return opened&&token==1; }
bool status(void*,risc_ble_sensor_status_v1* out) {
 *out={};out->struct_size=sizeof(*out);out->state=opened?RISC_BLE_SENSOR_SCANNING:RISC_BLE_SENSOR_IDLE;out->cleanup_pending=opened;return true;
}
bool device(void*,uint32_t,risc_ble_sensor_device_v1*) { return false; }
bool close(void*,uint64_t token) { quick_remote_manifest_sensor_call(1);if(token!=1)return false;opened=false;return true; }
const risc_bluetooth_sensors_v1 api={1,sizeof(api),nullptr,open,poll,status,device,close};
}
#else
static const uint32_t api[]={TEST_API,2*sizeof(uint32_t)};
#endif
static bool start(const risc_provider_dependency_v1*,size_t count) { return count==0; }
static void stop() {}
static bool quiesce() { return true; }
static const risc_driver_v2 driver={2,sizeof(driver),TEST_PROVIDER_ID,TEST_CAPABILITY,TEST_API,&api,start,stop,quiesce};
extern "C" __attribute__((visibility("default"))) const risc_driver_v2* t5_driver_get(uint32_t abi) { return abi==2?&driver:nullptr; }
#else
#include <ArduinoJson.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#define private public
#include "bootstrap/Runtime.h"
#undef private
#include "RuntimeFileFixture.h"
#include "FixtureJson.h"
#include "PortableRemoteBattery.h"

using RiscBoot::Runtime;
namespace {
std::string root;
std::function<void()> entry;
unsigned entries=0,cases=0,opens=0,closes=0,backendCalls=0;
const RiscBoot::KeyValueBackend kv{nullptr,
 [](void*,uint32_t,const char*,void*,uint32_t,uint32_t*)->int32_t{++backendCalls;return RISC_KEY_VALUE_NOT_FOUND;},
 [](void*,uint32_t,const char*,const void*,uint32_t)->int32_t{++backendCalls;return RISC_KEY_VALUE_OK;},
 RISC_KEY_VALUE_V2_BLOB_MAX};
RiscBoot::Port port() {
 return {[](){return true;},[](risc_runtime_health_v1*){return true;},
         [](uint32_t){},[](const char*){return true;},[](Runtime&){return true;},&kv};
}
void write(const char* path,const std::string& bytes) {
 std::ofstream file(root+"/"+path,std::ios::binary);file<<bytes;file.close();assert(file);
}
void jsonWrite(const char* path,const JsonDocument& document) {
 std::string bytes;serializeJson(document,bytes);write(path,bytes);
}
void check(const char* name,bool present,bool granted,bool prepared,
           unsigned grantApi=1,unsigned grantInstance=0,unsigned mutation=0) {
 JsonDocument manifest;
 assert(RiscTest::readFixtureJson((root+"/template.json").c_str(),manifest));
 if(mutation==1)manifest["optional"].as<JsonArray>().add(manifest["optional"][0]);
 if(mutation==2)manifest["requires"].as<JsonArray>().add(manifest["optional"][0]);
 if(mutation==3)manifest.remove("optional");
 if(mutation==4){manifest["requires"].as<JsonArray>().add(manifest["optional"][0]);manifest.remove("optional");}
 jsonWrite("app.json",manifest);
 JsonDocument boot;
 boot["board"]="board.json";boot["default_app"]="default.elf";
 JsonArray drivers=boot["drivers"].to<JsonArray>();
 JsonObject policy=boot["app_capabilities"].to<JsonArray>().add<JsonObject>();
 policy["manifest"]="app.json";JsonArray grants=policy["grants"].to<JsonArray>();
 unsigned index=0;
 for(JsonObjectConst required:manifest["requires"].as<JsonArrayConst>()) {
  const char* capability=required["capability"];
  if(!strcmp(capability,"bluetooth.sensors"))continue;
  auto grant=grants.add<JsonObject>();grant["capability"]=capability;grant["api"]=required["api"];
  const bool keyValue=!strcmp(capability,RISC_KEY_VALUE_CAPABILITY);
  grant["instance_id"]=keyValue?1:0;
  if(!keyValue)drivers.add<JsonObject>()["manifest"]="provider-"+std::to_string(index)+".json";
  ++index;
 }
 const unsigned mandatoryGrants=grants.size();
 if(present)drivers.add<JsonObject>()["manifest"]="sensors.json";
 if(granted){auto grant=grants.add<JsonObject>();grant["capability"]="bluetooth.sensors";grant["api"]=grantApi;grant["instance_id"]=grantInstance;}
 jsonWrite("boot.json",boot);
 RiscTest::FileSources files({root.c_str()});auto runtime=std::make_unique<Runtime>(port());assert(files.bind(*runtime));
 const bool actual=runtime->prepare(root.c_str());
 if(actual!=prepared){fprintf(stderr,"%s: prepare expected %d, got %d (%s)\n",name,int(prepared),int(actual),runtime->error());abort();}
 if(prepared) {
  assert(runtime->policies_[0].count==mandatoryGrants+unsigned(granted));
  const unsigned before=entries,beforeOpens=opens,beforeCloses=closes;
  entry=[&](){
   const auto* rt=risc_runtime_get_api(1);assert(rt);
   pqb_session session{};assert(pqb_init(&session));
   assert(pqb_start(&session,rt,true)==granted);
   if(granted){assert(session.active&&session.grant_owned);assert(pqb_poll(&session));}
   else {assert(session.status==PQB_UNAVAILABLE);assert(!session.active&&!session.grant_owned);}
   assert(pqb_close(&session));assert(!session.retained&&!session.grant_owned&&!session.token);
   assert(runtime->policies_[0].count==mandatoryGrants+unsigned(granted));
  };
  if(!runtime->run()){fprintf(stderr,"%s: run failed: %s\n",name,runtime->error());abort();}
  entry={};assert(entries==before+1);
  assert(opens==beforeOpens+unsigned(granted));assert(closes==beforeCloses+unsigned(granted));
 }
 assert(!backendCalls);++cases;printf("%s: PASS\n",name);
}
}
extern "C" void quick_remote_manifest_entry() { ++entries;assert(entry);entry(); }
extern "C" void quick_remote_manifest_sensor_call(unsigned operation) { if(operation==0)++opens;else if(operation==1)++closes;else abort(); }
int main(int argc,char** argv) {
 assert(argc==2);root=argv[1];
 write("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 check("omitted provider and owner grant keep Home admitted",false,false,true);
 check("selected provider does not manufacture owner grant",true,false,true);
 check("selected provider and explicit owner grant admit remote battery",true,true,true);
 check("owner grant to absent provider is rejected",false,true,false);
 check("wrong optional grant API is rejected",true,true,false,2);
 check("wrong optional grant instance is rejected",true,true,false,1,9);
 check("duplicate optional declaration is rejected",false,false,false,1,0,1);
 check("required/optional duplicate is rejected",true,true,false,1,0,2);
 check("default manifest remains admitted without optional field",false,false,true,1,0,3);
 check("undeclared sensor owner grant is rejected",true,true,false,1,0,3);
 check("mandatory sensor would block omitted provider",false,false,false,1,0,4);
 printf("Remote battery manifest: %u real Runtime admission checks, %u real app entries, no automatic grants, no RF I/O PASS\n",cases,entries);
}
#endif
