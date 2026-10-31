#pragma once
#include "Core.h"
#include <RiscBoundAppDataAsyncV1.h>
namespace Infra {
class Journal {
 public:
  Allocator allocator{393216};JsonDocument state{&allocator};
  const risc_bound_app_data_async_v1* backend=nullptr;
  uint8_t* scratch=nullptr;uint64_t seq=0,revision=0,writeToken=0,desiredSeq=0,writes=0,bytes=0;uint32_t desiredCrc=0,loadedCrc=0;bool retained=false,blocked=false,reload=false;
  bool initialize(const risc_bound_app_data_v1* b){backend=risc_bound_app_data_async(b);scratch=static_cast<uint8_t*>(std::malloc(StateMax));return backend&&scratch;}
  int load(){
    if(retained)return -2;uint32_t size=0;uint64_t fresh=0;auto r=backend->base.stat(backend->base.context,"node-state.bin",&size,&fresh);
    if(r==RISC_APP_DATA_RETAINED){retained=true;return -2;}if(r==RISC_APP_DATA_BUSY||r==RISC_APP_DATA_STALE)return 1;if(r!=0||size<32||size>StateMax){blocked=true;return -1;}
    r=backend->base.read(backend->base.context,"node-state.bin",fresh,scratch,StateMax,&size,&fresh);
    if(r==RISC_APP_DATA_RETAINED){retained=true;return -2;}if(r==RISC_APP_DATA_STALE||r==RISC_APP_DATA_BUSY)return 1;
    const char* payload=nullptr;size_t length=0;uint64_t sequence=0;
    if(r||!unframe(scratch,size,payload,length,sequence)||!parse(state,payload,length)||!valid()){blocked=true;return -1;}
    seq=sequence;loadedCrc=crc32(payload,length);revision=fresh;blocked=false;reload=false;return 0;
  }
  bool valid(){
   if(!keys(state.as<JsonObjectConst>(),{"schema_version","installation_id","credential_generation","credentials","participant_id","cursor","pending","job","checkpoints","configuration_revision"},{"bootstrap","enrollment_id","credential_sha256","rotation","configuration","target_epoch","configuration_desired_id","configuration_expires_at","setup_policy","setup_pending","relay"})||measureJson(state["relay"])>65536||state["schema_version"]!=1||!state["installation_id"].is<const char*>()||strlen(state["installation_id"]|"")>64||!state["pending"].is<JsonArray>()||state["pending"].size()>PendingMax||measureJson(state["credentials"])>4096||measureJson(state["job"])>RecordMax||measureJson(state["checkpoints"])>4096||measureJson(state["configuration"])>8192||measureJson(state["setup_policy"])>PolicyMax||(!state["setup_pending"].isNull()&&!state["setup_pending"].is<bool>()))return false;
   for(auto op:state["pending"].as<JsonArrayConst>())if(measureJson(op)>RecordMax)return false;return !state.overflowed();
  }
  int save(){
   if(retained)return -2;if(blocked||reload)return -1;if(writeToken)return 1;
   if(!valid()||seq==UINT64_MAX){blocked=true;return -1;}
   const size_t size=measureJson(state);if(size>StateMax-33){blocked=true;return -1;}
   serializeJson(state,reinterpret_cast<char*>(scratch+32),StateMax-32);
   desiredSeq=seq+1;desiredCrc=crc32(scratch+32,size);const auto total=frame(scratch,StateMax,reinterpret_cast<char*>(scratch+32),size,desiredSeq);
   uint32_t oldSize=0;uint64_t fresh=0;auto r=backend->base.stat(backend->base.context,"node-state.bin",&oldSize,&fresh);
   if(r==RISC_APP_DATA_RETAINED){retained=true;return -2;}if(r)return r==RISC_APP_DATA_BUSY?1:-1;
   // A different observed revision is a conflict, never overwrite another writer.
   if(revision&&fresh!=revision){reload=true;return 1;}
   r=backend->begin_replace(backend->base.context,"node-state.bin",fresh,scratch,total,&writeToken);
   if(r==RISC_APP_DATA_RETAINED){retained=true;return -2;}if(r==RISC_APP_DATA_STALE){reload=true;return 1;}
   if(r==RISC_APP_DATA_BUSY)return 1;if(r){blocked=true;return -1;}writes++;bytes+=total;return 1;
  }
  int collect(){
   if(retained)return -2;if(!writeToken)return reload?load():0;int32_t outcome=RISC_APP_DATA_BUSY;
   auto r=backend->collect_replace(backend->base.context,writeToken,&outcome);if(r==RISC_APP_DATA_BUSY)return 1;
   if(r==RISC_APP_DATA_RETAINED||outcome==RISC_APP_DATA_RETAINED){retained=true;return -2;}
   if(r){blocked=true;return -1;}writeToken=0;
   if(outcome==RISC_APP_DATA_OK){seq=desiredSeq;loadedCrc=desiredCrc;revision=0;return 0;}
   if(outcome==RISC_APP_DATA_COMMIT_UNKNOWN||outcome==RISC_APP_DATA_STALE){reload=true;return load();}
   blocked=true;return -1;
  }
  void dispose(){if(!retained&&!writeToken){state.clear();std::free(scratch);scratch=nullptr;}}
};
}
