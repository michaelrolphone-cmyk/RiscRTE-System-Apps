#include "installed_apps_fixture.h"
#include "runtime/storage/ScalableSettings.h"
using S=RiscStorage::ScalableSettings;
namespace scalable_fixture {
std::map<std::string,std::string> originals,deltas;std::string anchor;S::State values;
S::Backend backend={nullptr,
 [](void*,uint32_t,void*p,uint32_t cap,uint32_t*n)->int32_t{assert(!fixture::executing);if(anchor.empty())return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=anchor.size());*n=anchor.size();memcpy(p,anchor.data(),*n);return 0;},
 [](void*,uint32_t,const void*p,uint32_t n)->int32_t{assert(!fixture::executing);anchor.assign((const char*)p,n);return 0;},
 [](void*,uint32_t,bool delta,const char*after,char key[16],void*p,uint32_t cap,uint32_t*n)->int32_t{assert(!fixture::executing);auto&m=delta?deltas:originals;auto i=m.upper_bound(after);if(i==m.end())return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=i->second.size());strcpy(key,i->first.c_str());*n=i->second.size();memcpy(p,i->second.data(),*n);return 0;},
 [](void*,uint32_t,const char*k,const void*p,uint32_t n)->int32_t{assert(!fixture::executing);deltas[k].assign((const char*)p,n);return 0;},
 [](void*,uint32_t,const char*k)->int32_t{assert(!fixture::executing);deltas.erase(k);return 0;}};
void setup(){staged_fixture::setup();auto&n=fixture::installation;n.settings_capacity=128;n.settings_format=2;
 n.settings_status=[](void*,uint32_t)->uint32_t{return values.status;};
 n.settings_restore=[](void*,uint32_t,const char*owner,const void*p,uint32_t size,bool missing){return S::restore(values,backend,3000,owner,p,size,missing);};
 n.settings_snapshot=[](void*,uint32_t,void*p,uint32_t cap,uint32_t*size,uint64_t*g)->int32_t{uint8_t*data;int rc=S::snapshot(values,3000,&data,size,g);if(rc)return rc;if(cap<*size){free(data);return RISC_KEY_VALUE_BUFFER_SMALL;}memcpy(p,data,*size);free(data);return 0;};
 n.settings_ack=[](void*,uint32_t,uint64_t g){assert(staged_fixture::opened.empty());return S::acknowledge(values,backend,3000,g);};
 staged_fixture::packageVolume.base.begin=[](void*,uint64_t*t){return staged_fixture::exported?RISC_STORAGE_ASYNC_BUSY:risc_storage_async_begin(&fixture::bridge,t);};
}
void tick(RiscPackages::PagedSettingsCheckpoint&c){unsigned before=fixture::io;c.step();assert(fixture::io==before);fixture::executing=true;fixture::worker.workerStep();fixture::executing=false;}
unsigned finish(RiscPackages::PagedSettingsCheckpoint&c){unsigned n=0;while(c.state()!=RiscPackages::InstalledSettingsCheckpoint::State::Done&&c.state()!=RiscPackages::InstalledSettingsCheckpoint::State::Failed&&c.state()!=RiscPackages::InstalledSettingsCheckpoint::State::Cancelled){tick(c);assert(++n<10000);}return n;}
void check(){assert(values.count==originals.size());for(auto&row:originals){char out[2048];uint32_t n;assert(S::get(values,row.first.c_str(),out,sizeof out,&n)==0&&std::string(out,n)==row.second);}}
}
int main(){using namespace scalable_fixture;using State=RiscPackages::InstalledSettingsCheckpoint::State;
 setup();for(unsigned i=0;i<97;++i){char key[16];snprintf(key,sizeof key,"key%03u",i);originals[key]=std::string(64,char(i));}originals["large"]=std::string(2048,'L');auto original=originals;
 staged_fixture::disk["/AppData/app/kv-0000000000000000-00000000"]="torn unpublished page";
 RiscPackages::PagedSettingsCheckpoint first;assert(first.start(&fixture::installation,staged_fixture::volume,1,"app"));unsigned ticks=finish(first);assert(first.state()==State::Done&&values.status==S::Clean);check();assert(deltas.empty()&&!anchor.empty());auto disk=staged_fixture::disk;auto savedAnchor=anchor;
 // Every owner tick during migration can be cancelled/reset. Atomic index is
 // either absent or complete; originals always survive and no defaults replace them.
 for(unsigned cut=0;cut<ticks;++cut){
  staged_fixture::disk.clear();anchor.clear();deltas.clear();values.clear();
  {RiscPackages::PagedSettingsCheckpoint c;assert(c.start(&fixture::installation,staged_fixture::volume,1,"app"));for(unsigned i=0;i<cut;++i)tick(c);c.cancel();finish(c);}
  values.clear();RiscPackages::PagedSettingsCheckpoint resumed;assert(resumed.start(&fixture::installation,staged_fixture::volume,1,"app"));finish(resumed);assert(resumed.state()==State::Done);check();assert(originals==original);
 }
 staged_fixture::disk=disk;anchor=savedAnchor;values.clear();RiscPackages::PagedSettingsCheckpoint warm;assert(warm.start(&fixture::installation,staged_fixture::volume,1,"app"));finish(warm);check();
 staged_fixture::exported=true;assert(!S::put(values,backend,3000,"new","kept",4));auto pending=deltas;
 RiscPackages::PagedSettingsCheckpoint excluded;assert(!excluded.start(&fixture::installation,staged_fixture::volume,1,"app")&&deltas==pending);staged_fixture::exported=false;
 values.clear();RiscPackages::PagedSettingsCheckpoint replay;assert(replay.start(&fixture::installation,staged_fixture::volume,1,"app"));finish(replay);assert(replay.state()==State::Done&&values.count==99&&deltas.empty());char out[8];uint32_t n;assert(!S::get(values,"new",out,sizeof out,&n)&&std::string(out,n)=="kept");
 // Media absence must retain the native delta; reinsertion replays it.
 assert(!S::put(values,backend,3000,"removed","safe",4));pending=deltas;
 fixture::present=false;RiscPackages::PagedSettingsCheckpoint absent;
 assert(absent.start(&fixture::installation,staged_fixture::volume,1,"app"));finish(absent);
 assert(absent.state()==State::Failed&&deltas==pending);
 fixture::present=true;values.clear();RiscPackages::PagedSettingsCheckpoint inserted;
 assert(inserted.start(&fixture::installation,staged_fixture::volume,1,"app"));finish(inserted);
 assert(inserted.state()==State::Done&&deltas.empty());
 assert(!S::get(values,"removed",out,sizeof out,&n)&&std::string(out,n)=="safe");
 printf("Paged SD checkpoint: %u interruption boundaries; 98 existing values including 2048-byte opaque value, copied-worker I/O, cold recovery, export exclusion, native delta replay and immutable page verification PASS\n",ticks);
}
