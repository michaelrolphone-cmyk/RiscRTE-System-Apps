#include "installed_apps_fixture.h"
int main(){using namespace fixture;using namespace staged_fixture;staged_fixture::setup();
 for(unsigned cut=0;cut<160;++cut){
  saved.clear();revision=0;disk.clear();registered.clear();present=true;digest.assign(64,'7');
  JsonDocument d;assert(!deserializeJson(d,envelope("1.0.0")));d["url"]="https://example.invalid/app-image.elf";serializeJson(d,disk[descriptorPath]);
  graphBegin();assert(api.refresh(nullptr)==0&&drive()==0&&api.staged_count(nullptr)==1);
  assert(api.install(nullptr,"new-after-build","1.0.0")==0);
  unsigned opens=httpOpens;
  for(unsigned i=0;i<cut;++i){int rc=tick();if(rc==INSTALLED_APPS_NEEDS_NETWORK){assert(saved.find("provision")!=std::string::npos);assert(api.network_ready(nullptr,1800000000)==0);continue;}assert(rc==0||rc==INSTALLED_APPS_WORKING);if(!rc)break;}
  auto interruptedCache=saved;auto interruptedDisk=disk;
  for(unsigned i=0;;++i){assert(i<100);int rc=api.cancel(nullptr);if(!rc)break;assert(rc==INSTALLED_APPS_WORKING);executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;}
  assert(!httpOpen&&opened.empty());assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
  disk=interruptedDisk;saved=interruptedCache;revision=saved.empty()?0:revision+1;registered.clear();
  // Recovery uses the protected descriptor/URL, not a mutable public plan.
  if(saved.find("provision")!=std::string::npos)disk.erase(descriptorPath);
  graphBegin();unsigned before=io;assert(api.refresh(nullptr)==0&&before==io);
  if(saved.find("provision")!=std::string::npos&&cut%13==0){
   present=false;const auto journal=saved;unsigned network=httpOpens;
   assert(drive()==INSTALLED_APPS_UNAVAILABLE&&saved==journal&&httpOpens==network);
   assert(api.cancel(nullptr)==0);present=true;assert(api.refresh(nullptr)==0);
  }
  installed_app_v1 row{};
  if(!installId[0]&&(!api.get(nullptr,0,&row)||strcmp(row.version,"1.0.0"))){assert(drive()==0&&api.install(nullptr,"new-after-build","1.0.0")==0);}
  for(unsigned i=0;;++i){assert(i<500);int rc=tick();if(rc==INSTALLED_APPS_NEEDS_NETWORK){assert(api.network_ready(nullptr,1800000000)==0);continue;}if(!rc)break;assert(rc==INSTALLED_APPS_WORKING);}
  assert(api.get(nullptr,0,&row)&&!strcmp(row.version,"1.0.0"));
  assert(api.refresh(nullptr)==0&&drive()==0&&api.get(nullptr,0,&row)&&row.available);
  assert(disk.at("/Apps/new-after-build/image-"+digest+".elf")=="IMAGE"&&httpOpens>opens);
  assert(saved.find("provision")==std::string::npos&&saved.find("activation")==std::string::npos&&!httpOpen&&opened.empty());
  assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 }
 puts("Application HTTPS provisioning: 160 interruption/restart cuts, durable selected URL, descriptor removal, immutable image validation, cache-first replay and refreshed catalog PASS");
}
