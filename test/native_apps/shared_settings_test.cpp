#include "installed_apps_fixture.h"
#include "runtime/storage/ScalableSettings.h"
using SharedValues=RiscStorage::ScalableSettings;
namespace shared_fixture {
struct Bank {SharedValues::State values;std::map<std::string,std::string> originals,deltas;std::string anchor;};
std::map<uint32_t,Bank> banks;
std::vector<uint32_t> restored;
uint32_t slotNamespace(uint32_t slot){assert(slot==1||slot==129||slot==130);return slot==1?3000:slot==129?1:6;}
const SharedValues::Backend backend={nullptr,
 [](void*,uint32_t ns,void*p,uint32_t cap,uint32_t*n)->int32_t{assert(!fixture::executing);auto&s=banks[ns].anchor;if(s.empty())return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=s.size());*n=s.size();memcpy(p,s.data(),*n);return 0;},
 [](void*,uint32_t ns,const void*p,uint32_t n)->int32_t{assert(!fixture::executing);banks[ns].anchor.assign(static_cast<const char*>(p),n);return 0;},
 [](void*,uint32_t ns,bool delta,const char*after,char key[16],void*p,uint32_t cap,uint32_t*n)->int32_t{assert(!fixture::executing);auto&m=delta?banks[ns].deltas:banks[ns].originals;auto i=m.upper_bound(after);if(i==m.end())return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=i->second.size());strcpy(key,i->first.c_str());*n=i->second.size();memcpy(p,i->second.data(),*n);return 0;},
 [](void*,uint32_t ns,const char*k,const void*p,uint32_t n)->int32_t{assert(!fixture::executing);banks[ns].deltas[k].assign(static_cast<const char*>(p),n);return 0;},
 [](void*,uint32_t ns,const char*k)->int32_t{assert(!fixture::executing);banks[ns].deltas.erase(k);return 0;}};
void setup(){
 staged_fixture::setup();auto&n=fixture::installation;n.struct_size=sizeof(n);n.settings_capacity=n.namespace_capacity=128;n.settings_format=2;n.shared_settings_count=2;
 n.shared_settings_namespace=[](void*,uint32_t i)->uint32_t{assert(i<2);return i?6:1;};
 n.settings_status=[](void*,uint32_t slot)->uint32_t{return banks[slotNamespace(slot)].values.status;};
 n.settings_restore=[](void*,uint32_t slot,const char*owner,const void*p,uint32_t size,bool missing){
  uint32_t ns=slotNamespace(slot);auto&v=banks[ns].values;
  if(v.status!=SharedValues::Restoring)restored.push_back(ns);
  return SharedValues::restore(v,backend,ns,owner,p,size,missing);
 };
 n.settings_snapshot=[](void*,uint32_t slot,void*p,uint32_t cap,uint32_t*size,uint64_t*g)->int32_t{
  uint32_t ns=slotNamespace(slot);uint8_t*data=nullptr;int rc=SharedValues::snapshot(banks[ns].values,ns,&data,size,g);if(rc)return rc;
  if(cap<*size){free(data);return RISC_KEY_VALUE_BUFFER_SMALL;}memcpy(p,data,*size);free(data);return 0;
 };
 n.settings_ack=[](void*,uint32_t slot,uint64_t g){assert(staged_fixture::opened.empty());uint32_t ns=slotNamespace(slot);return SharedValues::acknowledge(banks[ns].values,backend,ns,g);};
 staged_fixture::packageVolume.base.begin=[](void*,uint64_t*t){return staged_fixture::exported?RISC_STORAGE_ASYNC_BUSY:risc_storage_async_begin(&fixture::bridge,t);};
 fixture::begin();loaded=true;requested=false;currentCount=1;current[0]={};strcpy(current[0].view.id,"app");current[0].namespaceSlot=1;current[0].settingsNeeded=true;
}
void worker(){fixture::executing=true;fixture::worker.workerStep();fixture::cacheWorker.workerStep();fixture::executing=false;}
unsigned drive(){unsigned ticks=0;for(;;){unsigned before=fixture::io;int rc=api.step(nullptr,staged_fixture::volume);assert(fixture::io==before);worker();assert(++ticks<10000);if(rc!=INSTALLED_APPS_WORKING){assert(rc==INSTALLED_APPS_OK);return ticks;}}}
void checkpointTick(RiscPackages::PagedSettingsCheckpoint&c){unsigned before=fixture::io;c.step();assert(fixture::io==before);worker();}
unsigned finish(RiscPackages::PagedSettingsCheckpoint&c){using State=RiscPackages::InstalledSettingsCheckpoint::State;unsigned ticks=0;while(c.state()!=State::Done&&c.state()!=State::Failed&&c.state()!=State::Cancelled){assert(c.state()!=State::Retained);checkpointTick(c);assert(++ticks<10000);}return ticks;}
void value(uint32_t ns,const char*key,const std::string&expected){char out[2048];uint32_t n=0;assert(SharedValues::get(banks[ns].values,key,out,sizeof(out),&n)==0);assert(std::string(out,n)==expected);}
void cold(){for(auto&item:banks)item.second.values.clear();restored.clear();}
}
int main(){using namespace shared_fixture;using State=RiscPackages::InstalledSettingsCheckpoint::State;
 setup();banks[1].originals["opaque"]="namespace-one";banks[6].originals["opaque"]="namespace-six";banks[3000].originals["opaque"]="private";
 assert(api.settings_pending(nullptr));drive();
 assert((restored==std::vector<uint32_t>{1,6,3000}));
 assert(staged_fixture::disk.count("/RTECFG/00000001/settings.index")&&staged_fixture::disk.count("/RTECFG/00000006/settings.index")&&staged_fixture::disk.count("/AppData/app/settings.index"));
 value(1,"opaque","namespace-one");value(6,"opaque","namespace-six");value(3000,"opaque","private");
 assert(!api.settings_pending(nullptr)&&!requested);auto checkpointDisk=staged_fixture::disk;
 // New dirty writes checkpoint after the requested discovery has already ended.
 assert(!SharedValues::put(banks[1].values,backend,1,"late","journal-only",12));assert(!requested&&api.settings_pending(nullptr));
 auto journal=banks[1].deltas;staged_fixture::exported=true;unsigned io=fixture::io;
 assert(api.step(nullptr,staged_fixture::volume)==INSTALLED_APPS_UNAVAILABLE&&fixture::io==io&&banks[1].deltas==journal);
 assert(api.cancel(nullptr)==INSTALLED_APPS_OK);staged_fixture::exported=false;drive();assert(banks[1].deltas.empty());value(1,"late","journal-only");
 // All native data, including originals and anchors, may be erased by flashing.
 checkpointDisk=staged_fixture::disk;for(auto&item:banks){item.second.originals.clear();item.second.deltas.clear();item.second.anchor.clear();}cold();drive();
 for(const auto&item:banks)assert(!item.second.anchor.empty());
 value(1,"opaque","namespace-one");value(6,"opaque","namespace-six");value(3000,"opaque","private");value(1,"late","journal-only");
 // Recovery repairs an interrupted atomic index before reading any pages.
 auto&disk=staged_fixture::disk;disk["/RTECFG/00000006/settings.index.risc-replace"]=disk.at("/RTECFG/00000006/settings.index");disk.erase("/RTECFG/00000006/settings.index");cold();drive();value(6,"opaque","namespace-six");
 assert(!disk.count("/RTECFG/00000006/settings.index.risc-replace"));
 // Missing media preserves a later native journal and closes copied-worker custody.
 assert(!SharedValues::put(banks[6].values,backend,6,"late","retained",8));journal=banks[6].deltas;
 fixture::present=false;int rc;do{rc=staged_fixture::tick();}while(rc==INSTALLED_APPS_WORKING);assert(rc==INSTALLED_APPS_UNAVAILABLE&&banks[6].deltas==journal&&staged_fixture::opened.empty());
 assert(api.cancel(nullptr)==INSTALLED_APPS_OK);fixture::present=true;cold();drive();value(6,"late","retained");assert(banks[6].deltas.empty());
 // A corrupt shared namespace reports failure, then the next pass advances.
 auto goodIndex=disk.at("/RTECFG/00000001/settings.index");disk["/RTECFG/00000001/settings.index"]="corrupt";cold();settingsCursor=0;
 do{rc=staged_fixture::tick();}while(rc==INSTALLED_APPS_WORKING);assert(rc==INSTALLED_APPS_UNAVAILABLE);
 for(unsigned tries=0;banks[6].values.status!=SharedValues::Clean||banks[3000].values.status!=SharedValues::Clean;++tries){assert(tries<1000);assert(staged_fixture::tick()==INSTALLED_APPS_WORKING);}
 value(6,"opaque","namespace-six");value(3000,"opaque","private");
 disk["/RTECFG/00000001/settings.index"]=goodIndex;drive();
 // A new-header consumer sees an old provider prefix without touching the suffix.
 cold();fixture::installation.struct_size=RISC_INSTALLATION_SETTINGS_FORMAT_SIZE;restored.clear();drive();
 assert((restored==std::vector<uint32_t>{3000}));assert(banks[1].values.status==SharedValues::Cold&&banks[6].values.status==SharedValues::Cold);
 fixture::installation.struct_size=sizeof(fixture::installation);drive();
 // Every copied-worker boundary in an actual shared checkpoint can be cancelled.
 assert(!SharedValues::put(banks[1].values,backend,1,"cancel","survives",8));journal=banks[1].deltas;
 auto beforeDisk=disk;auto beforeAnchor=banks[1].anchor;
 RiscPackages::PagedSettingsCheckpoint probe;assert(probe.start(&fixture::installation,staged_fixture::volume,129,"shared-00000001"));unsigned boundaries=finish(probe);assert(probe.state()==State::Done);
 for(unsigned cut=0;cut<boundaries;++cut){
  disk=beforeDisk;banks[1].anchor=beforeAnchor;banks[1].deltas=journal;banks[1].values.clear();
  RiscPackages::PagedSettingsCheckpoint c;assert(c.start(&fixture::installation,staged_fixture::volume,129,"shared-00000001"));
  for(unsigned i=0;i<cut;++i)checkpointTick(c);
  c.cancel();finish(c);assert(staged_fixture::opened.empty());
  banks[1].values.clear();RiscPackages::PagedSettingsCheckpoint recovered;assert(recovered.start(&fixture::installation,staged_fixture::volume,129,"shared-00000001"));finish(recovered);assert(recovered.state()==State::Done);value(1,"cancel","survives");assert(banks[1].deltas.empty());
 }
 // Wrong shared owner never obtains a worker session.
 RiscPackages::PagedSettingsCheckpoint wrong;assert(!wrong.start(&fixture::installation,staged_fixture::volume,129,"shared-00000006"));
 assert(t5_driver_get(2)->quiesce());
 printf("Shared 1/6: shared-first/private ordering, idle dirty work, old ABI, SD-only recovery, atomic repair, absent/exported card and %u cancellation boundaries PASS\n",boundaries);
}
