#include "DriverStageFileTransfer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using DriverManager::StageFileTransfer;
using TransferState=StageFileTransfer::State;
using namespace RuntimePackages;
static std::string manifest(const char*id,const char*kind="driver",const char*version="1.2.3"){
 return std::string("{\"schema\":1,\"kind\":\"")+kind+"\",\"id\":\""+id+"\",\"version\":\""+version+
 "\",\"artifact\":\"driver.elf\",\"architecture\":\"xtensa-esp32s3\",\"min_runtime_api\":2,\"entries\":[{\"name\":\"driver.elf\",\"size_bytes\":52,\"sha256\":\""+
 std::string(64,'a')+"\",\"executable\":true}],\"requires\":[]}";
}
static std::vector<uint8_t> zip(const std::string&json){
 uint8_t elf[52]{};RteZipSourceFile files[]={{kOrdinaryManifestName,reinterpret_cast<const uint8_t*>(json.data()),uint32_t(json.size())},{"driver.elf",elf,sizeof(elf)}};
 std::vector<uint8_t> bytes;auto write=[&](const uint8_t*p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;};
 uint32_t written=0;assert(packRteZip(files,2,write,&written)==RteZipResult::Ready);return bytes;
}
struct Fixture{
 risc_storage_volume_api_v1_download api{};
 std::vector<risc_storage_dirent_v1> entries;
 std::map<std::string,std::vector<uint8_t>> files;
 risc_storage_async_request_v1 request{};risc_storage_package_request_v1 package{};
 bool isPackage=false,active=false,missing=false,retainedCollect=false,retainedEnd=false;
 unsigned submitBusy=0,submitCalls=0;uint32_t rejectOperation=0;int32_t rejectStatus=RISC_STORAGE_ASYNC_IO;bool wrongRead=false,writer=false;
 std::vector<uint8_t> destination;std::vector<uint32_t> acceptedOperations;std::string destinationPath;
 uint64_t session=0,operation=0,nextOperation=1;
 unsigned position=0,submitted=0,collected=0,opened=0,ended=0,rangeReads=0,beginBusy=0,pending=0,endPending=0;
 static Fixture&self(void*c){return *static_cast<Fixture*>(c);}
 const risc_storage_volume_api_v1*volume()const{return reinterpret_cast<const risc_storage_volume_api_v1*>(&api);}
 static bool trap(){assert(!"synchronous filesystem callback invoked");return false;}
 Fixture(){
  auto*b=reinterpret_cast<risc_storage_volume_api_v1*>(&api);
  b->api_version=1;b->struct_size=sizeof(api);b->context=this;
  b->refresh=[](void*){return trap();};b->ready=[](void*){return trap();};
  b->label=[](void*,char*,size_t){return trap();};
  b->stat=[](void*,const char*,uint64_t*,bool*){return trap();};
  b->dir_open=[](void*,const char*)->risc_storage_dir_t{trap();return 0;};
  b->dir_next=[](void*,risc_storage_dir_t,risc_storage_dirent_v1*){return trap();};
  b->dir_close=[](void*,risc_storage_dir_t){trap();};
  b->file_open_read=[](void*,const char*,uint64_t*)->risc_storage_file_t{trap();return 0;};
  b->file_read=[](void*,risc_storage_file_t,void*,size_t)->size_t{trap();return 0;};
  b->file_open_write=[](void*,const char*)->risc_storage_file_t{trap();return 0;};
  b->file_write=[](void*,risc_storage_file_t,const void*,size_t)->size_t{trap();return 0;};
  b->file_close=[](void*,risc_storage_file_t,bool){return trap();};
  b->remove=[](void*,const char*){return trap();};b->last_error=[](void*,char*,size_t){return trap();};
  auto&fs=api.base.base.fs;
  fs.state.state_tag=RISC_STORAGE_STATE_TAG;fs.state.state_version=1;fs.state.observe=[](void*)->int32_t{trap();return 0;};
  fs.fs_tag=RISC_STORAGE_FS_TAG;fs.fs_version=1;
  fs.dir_tell=[](void*,risc_storage_dir_t,uint64_t*){return trap();};
  fs.dir_seek=[](void*,risc_storage_dir_t,uint64_t){return trap();};
  fs.metadata=[](void*,const char*,risc_storage_metadata_v1*)->int32_t{trap();return -1;};
  fs.file_truncate=[](void*,risc_storage_file_t,uint64_t){return trap();};
  fs.replace_file=[](void*,const char*,const char*){return trap();};
  fs.recover_replace=[](void*,const char*){return trap();};
  auto&a=api.base.base;a.async_tag=RISC_STORAGE_ASYNC_TAG;a.async_version=3;
  a.begin=[](void*c,uint64_t*out)->int32_t{auto&f=self(c);if(f.beginBusy){--f.beginBusy;return RISC_STORAGE_ASYNC_BUSY;}assert(!f.session);f.session=17;*out=f.session;return RISC_STORAGE_ASYNC_OK;};
  a.submit=[](void*c,uint64_t s,const risc_storage_async_request_v1*q,uint64_t*out)->int32_t{auto&f=self(c);assert(s==f.session&&!f.active);f.request=*q;f.isPackage=false;f.active=true;*out=f.operation=f.nextOperation++;++f.submitted;return RISC_STORAGE_ASYNC_OK;};
  a.collect=[](void*c,uint64_t s,uint64_t o,risc_storage_async_result_v1*out)->int32_t{
   auto&f=self(c);assert(s==f.session&&f.active&&o==f.operation);
   if(f.pending){--f.pending;return RISC_STORAGE_ASYNC_PENDING;}
   if(f.retainedCollect)return RISC_STORAGE_ASYNC_RETAINED;
   *out={};f.active=false;f.operation=0;++f.collected;
   if(f.isPackage){
    if(f.package.operation==f.rejectOperation){if(f.rejectOperation==RISC_STORAGE_PACKAGE_APPEND_AT)f.destination.push_back(0x5a);return f.rejectStatus;}
    if(f.package.operation==RISC_STORAGE_PACKAGE_APPEND_AT){
     uint64_t offset=0;std::memcpy(&offset,f.package.sha256,sizeof(offset));assert(offset==f.destination.size());
     assert(f.package.size&&f.package.size<=4096);if(f.destinationPath.empty())f.destinationPath=f.package.path;
     assert(f.destinationPath==f.package.path);
     f.destination.insert(f.destination.end(),f.package.data,f.package.data+f.package.size);
     return RISC_STORAGE_ASYNC_OK;
    }
    if(f.package.operation==RISC_STORAGE_PACKAGE_OPEN_WRITE){assert(!f.writer&&f.destination.empty());f.writer=true;f.destinationPath=f.package.path;return RISC_STORAGE_ASYNC_OK;}
    if(f.package.operation==RISC_STORAGE_PACKAGE_CLOSE_WRITE){assert(f.writer);f.writer=false;return RISC_STORAGE_ASYNC_OK;}
    if(f.package.operation==RISC_STORAGE_PACKAGE_VALIDATE_DATA||f.package.operation==RISC_STORAGE_PACKAGE_VALIDATE_DRIVER){
     assert(f.destinationPath==f.package.path&&f.package.size==f.destination.size());
     for(unsigned i=0;i<32;++i)assert(f.package.sha256[i]==0xaa);
     return RISC_STORAGE_ASYNC_OK;
    }
    assert(f.package.operation==RISC_STORAGE_PACKAGE_READ_RANGE);++f.rangeReads;
    auto i=f.files.find(f.package.path);if(i==f.files.end())return RISC_STORAGE_ASYNC_NOT_FOUND;
    uint64_t offset=0;std::memcpy(&offset,f.package.sha256,sizeof(offset));
    if(offset>i->second.size()||f.package.size>i->second.size()-offset)return RISC_STORAGE_ASYNC_IO;
    out->size=f.package.size-(f.wrongRead?1:0);std::memcpy(out->data,i->second.data()+offset,out->size);return RISC_STORAGE_ASYNC_OK;
   }
   switch(f.request.operation){
    case RISC_STORAGE_ASYNC_REFRESH:return RISC_STORAGE_ASYNC_OK;
    case RISC_STORAGE_ASYNC_OPEN_DIRECTORY:assert(!std::strcmp(f.request.path,"/Packages/Inbox"));f.position=0;++f.opened;return f.missing?RISC_STORAGE_ASYNC_NOT_FOUND:RISC_STORAGE_ASYNC_OK;
    case RISC_STORAGE_ASYNC_NEXT_ENTRY:if(f.position==f.entries.size())return RISC_STORAGE_ASYNC_END;out->entry=f.entries[f.position++];return RISC_STORAGE_ASYNC_OK;
    case RISC_STORAGE_ASYNC_CLOSE_DIRECTORY:return RISC_STORAGE_ASYNC_OK;
    case RISC_STORAGE_ASYNC_READ_SMALL_FILE:{auto i=f.files.find(f.request.path);if(i==f.files.end())return RISC_STORAGE_ASYNC_NOT_FOUND;if(i->second.size()>4096)return RISC_STORAGE_ASYNC_IO;out->size=uint32_t(i->second.size());std::memcpy(out->data,i->second.data(),out->size);return RISC_STORAGE_ASYNC_OK;}
    default:assert(false);return RISC_STORAGE_ASYNC_INVALID;
   }
  };
  a.end=[](void*c,uint64_t s)->int32_t{auto&f=self(c);assert(s==f.session);++f.ended;if(f.endPending){--f.endPending;return RISC_STORAGE_ASYNC_PENDING;}if(f.retainedEnd)return RISC_STORAGE_ASYNC_RETAINED;f.active=false;f.operation=f.session=0;return RISC_STORAGE_ASYNC_OK;};
  api.base.package_tag=RISC_STORAGE_PACKAGE_TAG;api.base.package_version=1;
  api.base.submit_package=[](void*c,uint64_t s,const risc_storage_package_request_v1*q,uint64_t*out)->int32_t{auto&f=self(c);assert(s==f.session&&!f.active);++f.submitCalls;if(f.submitBusy){--f.submitBusy;return RISC_STORAGE_ASYNC_BUSY;}f.acceptedOperations.push_back(q->operation);f.package=*q;f.isPackage=true;f.active=true;*out=f.operation=f.nextOperation++;++f.submitted;return RISC_STORAGE_ASYNC_OK;};
  api.download_tag=RISC_STORAGE_DOWNLOAD_TAG;api.download_version=3;
 }
 void add(const char*name,bool directory,const std::vector<uint8_t>&content={}){
  risc_storage_dirent_v1 e{};assert(std::strlen(name)<sizeof(e.name));std::strcpy(e.name,name);e.is_directory=directory;e.size=content.size();entries.push_back(e);
  if(!content.empty())files[std::string("/Packages/Inbox/")+name]=content;
 }
 void directory(const char*id,const char*kind="driver"){
  add(id,true);const auto j=manifest(id,kind);files[std::string("/Packages/Inbox/")+id+"/.package.json"]=std::vector<uint8_t>(j.begin(),j.end());
 }
};

static OrdinaryEntry entry(uint32_t bytes,bool executable=false){
 OrdinaryEntry e{};std::strcpy(e.name,executable?"driver.elf":"resource.bin");e.sizeBytes=bytes;
 std::memset(e.sha256,'a',64);e.sha256[64]=0;e.executable=executable;return e;
}
static TransferState complete(StageFileTransfer&t){for(unsigned i=0;i<1000;++i){const auto s=t.step();if(s!=TransferState::Working)return s;}assert(false);return TransferState::Failed;}
static void terminalDoesNotCall(Fixture&f,StageFileTransfer&t,TransferState expected){
 const auto calls=f.submitted+f.collected+f.ended;
 for(unsigned i=0;i<1000;++i)assert(t.step()==expected);
 assert(calls==f.submitted+f.collected+f.ended);
 assert(f.session==17&&!f.ended); // owning coordinator, not this helper, drains custody
}
int main(){
 // ZIP transport has a smaller bound than legacy ordinary directory intake.
 {Fixture f;f.session=17;auto e=entry(uint32_t(kRteZipMaxEntryBytes+1));StageFileTransfer directory,archive;
  assert(directory.begin(f.volume(),17,"/Packages/Inbox/driver-one/resource.bin",0,"driver-one",e,false,0));
  assert(!archive.begin(f.volume(),17,"/Packages/Inbox/source.rte.zip",0,"driver-one",e,true,0));
  // No step: the owning coordinator still owns the existing session.
  assert(!f.submitted&&!f.ended);
 }
 const char*source="/Packages/Inbox/source.rte.zip";
 {Fixture f;f.session=17;StageFileTransfer t;std::vector<uint8_t> payload(6000);
  for(size_t i=0;i<payload.size();++i)payload[i]=uint8_t(i);
  std::vector<uint8_t> file(17,0);file.insert(file.end(),payload.begin(),payload.end());f.files[source]=file;
  auto e=entry(uint32_t(payload.size()),true);
  assert(t.begin(f.volume(),17,source,17,"driver-one",e,true,RteZipDetail::crc32(payload.data(),payload.size())));
  f.submitBusy=1000;for(unsigned i=0;i<1000;++i)assert(t.step()==TransferState::Working);
  assert(!f.submitted&&!f.active&&t.copied()==0);
  assert(t.step()==TransferState::Working);assert(f.submitted==1&&f.active);
  f.pending=1000;for(unsigned i=0;i<1000;++i)assert(t.step()==TransferState::Working);
  assert(f.submitted==1&&f.active&&t.copied()==0);
  assert(complete(t)==TransferState::Ready);assert(t.copied()==payload.size()&&f.destination==payload);
  const std::vector<uint32_t> expected={110,111,110,111,113,107};assert(f.acceptedOperations==expected);
  assert(f.destinationPath=="/Drivers/.driver-one.pkg-stage/driver.elf");terminalDoesNotCall(f,t,TransferState::Ready);
 }
 {Fixture f;f.session=17;StageFileTransfer t;auto bytes=zip(manifest("driver-one"));
  RteZipView view{};auto read=[&](uint64_t o,uint8_t*p,size_t n){if(o>bytes.size()||n>bytes.size()-o)return false;std::memcpy(p,bytes.data()+o,n);return true;};
  assert(inspectRteZip(read,bytes.size(),view)==RteZipResult::Ready);
  f.files[source]=bytes;auto e=entry(52,true);
  assert(t.begin(f.volume(),17,source,view.entries[1].dataOffset,"driver-one",e,true,view.entries[1].crc32));
  assert(complete(t)==TransferState::Ready);assert(f.rangeReads==1);
 }
 // A mismatching ZIP CRC rejects before any SHA/ELF validation or publication.
 {Fixture f;f.session=17;f.files[source]={1,2,3};StageFileTransfer t;auto e=entry(3);
  const uint32_t wrong=RteZipDetail::crc32(f.files[source].data(),3)^1u;
  assert(t.begin(f.volume(),17,source,0,"driver-one",e,true,wrong));
  assert(complete(t)==TransferState::Failed);assert(f.acceptedOperations==std::vector<uint32_t>({110,111}));
  terminalDoesNotCall(f,t,TransferState::Failed);
 }
 // Destination SHA and executable/import validators are distinct native boundaries.
 for(uint32_t rejected:{uint32_t(RISC_STORAGE_PACKAGE_VALIDATE_DATA),uint32_t(RISC_STORAGE_PACKAGE_VALIDATE_DRIVER)}){
  Fixture f;f.session=17;f.files[source]=std::vector<uint8_t>(52);f.rejectOperation=rejected;StageFileTransfer t;auto e=entry(52,true);
  assert(t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
  assert(complete(t)==TransferState::Failed);assert(f.acceptedOperations.back()==rejected);
  terminalDoesNotCall(f,t,TransferState::Failed);
 }
 // Simulate an append that wrote a partial byte but reported failure.
 {Fixture f;f.session=17;f.files[source]={1,2,3};f.rejectOperation=RISC_STORAGE_PACKAGE_APPEND_AT;StageFileTransfer t;auto e=entry(3);
  assert(t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
  assert(complete(t)==TransferState::Failed);assert(f.destination==std::vector<uint8_t>({0x5a})&&t.copied()==0);
  terminalDoesNotCall(f,t,TransferState::Failed);assert(f.acceptedOperations==std::vector<uint32_t>({110,111}));
  assert(!t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
 }
 {Fixture f;f.session=17;f.files[source]={1,2,3};f.wrongRead=true;StageFileTransfer t;auto e=entry(3);
  assert(t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
  assert(complete(t)==TransferState::Failed);assert(f.destination.empty()&&f.acceptedOperations.size()==1);
 }
 // Retained append custody is never converted into a successful drain.
 {Fixture f;f.session=17;f.files[source]={1,2,3};StageFileTransfer t;auto e=entry(3);
  assert(t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
  assert(t.step()==TransferState::Working);assert(t.step()==TransferState::Working);assert(t.step()==TransferState::Working);
  assert(f.package.operation==RISC_STORAGE_PACKAGE_APPEND_AT);f.retainedCollect=true;
  assert(t.step()==TransferState::Retained);terminalDoesNotCall(f,t,TransferState::Retained);
  assert(!t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
 }
 {Fixture f;f.session=17;StageFileTransfer t;auto e=entry(0);
  assert(t.begin(f.volume(),17,source,0,"driver-one",e,true,0));
  assert(complete(t)==TransferState::Ready);assert(f.destination.empty()&&!f.writer);
  assert(f.acceptedOperations==std::vector<uint32_t>({104,106,113}));
  terminalDoesNotCall(f,t,TransferState::Ready);
 }
 {Fixture f;f.session=17;StageFileTransfer t;auto e=entry(0);
  assert(t.begin(f.volume(),17,source,0,"driver-one",e,true,1));
  assert(complete(t)==TransferState::Failed);assert(f.acceptedOperations==std::vector<uint32_t>({104,106}));
 }
 {Fixture f;f.session=17;StageFileTransfer t;auto e=entry(3);
  assert(!t.begin(f.volume(),0,source,0,"driver-one",e,false,0));
  assert(!t.begin(f.volume(),17,"/Settings/private",0,"driver-one",e,false,0));
  assert(!t.begin(f.volume(),17,source,0,"../bad",e,false,0));
  assert(!t.begin(f.volume(),17,"/Packages/Inbox/../escape",0,"driver-one",e,false,0));
  std::memset(e.name,'x',sizeof(e.name));assert(!t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
  e=entry(3);std::memset(e.sha256,'a',sizeof(e.sha256));assert(!t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
  std::strcpy(e.sha256,"invalid");assert(!t.begin(f.volume(),17,source,0,"driver-one",e,false,0));
 }
 return 0;
}
