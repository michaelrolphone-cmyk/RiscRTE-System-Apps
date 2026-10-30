#include "Journal.h"
#include <cassert>
#include <vector>
#include <string>
namespace {
std::vector<uint8_t> disk,pending;uint64_t revision=1;int statResult=0,collectResult=0;bool commit=true,busy=false;
int stat(void*,const char* name,uint32_t* bytes,uint64_t* rev){assert(!strcmp(name,"node-state.bin"));if(statResult)return statResult;*bytes=disk.size();*rev=revision;return 0;}
int read(void*,const char*,uint64_t expected,void* out,uint32_t cap,uint32_t* size,uint64_t* rev){if(expected!=revision)return RISC_APP_DATA_STALE;assert(cap>=disk.size());memcpy(out,disk.data(),disk.size());*size=disk.size();*rev=revision;return 0;}
int begin(void*,const char*,uint64_t expected,const void* data,uint32_t size,uint64_t* token){if(expected!=revision)return RISC_APP_DATA_STALE;pending.assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size);*token=7;return 0;}
int collect(void*,uint64_t token,int32_t* outcome){assert(token==7);if(busy)return RISC_APP_DATA_BUSY;if(commit){disk=pending;++revision;}*outcome=collectResult;return 0;}
risc_bound_app_data_async_v1 api{{1,sizeof(api),nullptr,stat,read,nullptr},RISC_BOUND_APP_DATA_ASYNC_TAG,1,begin,collect};
void initial(){std::string json=R"({"schema_version":1,"installation_id":"i","credential_generation":1,"credentials":{},"participant_id":"p","cursor":0,"pending":[],"job":null,"checkpoints":{},"configuration_revision":1})";disk.resize(49152);disk.resize(Infra::frame(disk.data(),disk.size(),json.data(),json.size(),1));revision=1;statResult=collectResult=0;commit=true;busy=false;}
}
int main(){
 initial();Infra::Journal j;assert(j.initialize(&api.base));statResult=RISC_APP_DATA_BUSY;assert(j.load()==1&&!j.blocked);statResult=0;assert(j.load()==0&&j.seq==1);
 j.state["pending"].to<JsonArray>().add<JsonObject>()["operation_id"]="one";assert(j.save()==1&&j.writeToken);busy=true;assert(j.collect()==1&&j.seq==1);busy=false;assert(j.collect()==0&&j.seq==2&&j.loadedCrc==j.desiredCrc);
 // Ambiguous commit, destination remains old: never treat intent as persisted.
 j.state["cursor"]=12;collectResult=RISC_APP_DATA_COMMIT_UNKNOWN;commit=false;assert(j.save()==1);assert(j.collect()==0&&j.seq==2&&j.state["cursor"]==0&&j.seq!=j.desiredSeq);
 // Ambiguous commit, complete new destination: reload proves intended snapshot.
 j.state["cursor"]=13;commit=true;assert(j.save()==1);assert(j.collect()==0&&j.seq==3&&j.state["cursor"]==13&&j.loadedCrc==j.desiredCrc);
 // Any intervening writer forces reload rather than overwriting newer bytes.
 assert(j.load()==0);++revision;assert(j.save()==1&&j.reload&&!j.writeToken);assert(j.collect()==0);
 // Retention fences both reads and writes permanently.
 statResult=RISC_APP_DATA_RETAINED;assert(j.load()==-2);statResult=0;assert(j.load()==-2&&j.save()==-2&&j.collect()==-2);
 initial();Infra::Journal corrupt;assert(corrupt.initialize(&api.base));disk[33]^=1;assert(corrupt.load()==-1&&corrupt.blocked);assert(corrupt.save()==-1);corrupt.dispose();
 puts("Journal: copied async writes, BUSY, both COMMIT_UNKNOWN outcomes, stale revision, corruption and retained fence PASS");
}
