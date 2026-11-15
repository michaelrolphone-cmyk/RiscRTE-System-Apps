#include "DriverInboxScan.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using DriverManager::InboxScan;
using ScanState=InboxScan::State;
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
    assert(f.package.operation==RISC_STORAGE_PACKAGE_READ_RANGE);++f.rangeReads;
    auto i=f.files.find(f.package.path);if(i==f.files.end())return RISC_STORAGE_ASYNC_NOT_FOUND;
    uint64_t offset=0;std::memcpy(&offset,f.package.sha256,sizeof(offset));
    if(offset>i->second.size()||f.package.size>i->second.size()-offset)return RISC_STORAGE_ASYNC_IO;
    out->size=f.package.size;std::memcpy(out->data,i->second.data()+offset,out->size);return RISC_STORAGE_ASYNC_OK;
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
  api.base.submit_package=[](void*c,uint64_t s,const risc_storage_package_request_v1*q,uint64_t*out)->int32_t{auto&f=self(c);assert(s==f.session&&!f.active);f.package=*q;f.isPackage=true;f.active=true;*out=f.operation=f.nextOperation++;++f.submitted;return RISC_STORAGE_ASYNC_OK;};
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
static bool eligible(void*,const OrdinaryPackagePlan&){return true;}
static ScanState complete(InboxScan&s){for(unsigned i=0;i<100000;++i){auto state=s.step();if(state!=ScanState::Working)return state;}assert(false);return ScanState::Failed;}
int main(){
 {Fixture f;InboxScan s;f.directory("driver-one");f.add("zip.rte.zip",false,zip(manifest("zip-driver")));
  assert(s.begin(f.volume(),eligible,nullptr));assert(complete(s)==ScanState::Complete);assert(s.count()==2&&f.rangeReads>0&&!f.session);
  assert(!std::strcmp(s.get(0)->row.id,"driver-one")&&!s.get(0)->archive);
  assert(!std::strcmp(s.get(1)->row.id,"zip-driver")&&s.get(1)->archive);
  assert(s.get(0)->row.flags&DRIVER_MANAGER_CAN_INSTALL);assert(!s.get(2));
 }
 {Fixture f;InboxScan s;
  for(unsigned i=0;i<65;++i){char id[32];std::snprintf(id,sizeof(id),"app-%03u",i);f.directory(id,"application");}
  for(unsigned i=0;i<64;++i){char id[32];std::snprintf(id,sizeof(id),"driver-%03u",i);f.directory(id);}
  assert(s.begin(f.volume(),eligible,nullptr));assert(complete(s)==ScanState::Complete);assert(s.count()==64&&f.opened>=3);
  assert(!std::strcmp(s.get(63)->row.id,"driver-063"));
 }
 for(bool missing:{false,true}){Fixture f;f.missing=missing;InboxScan s;assert(s.begin(f.volume(),eligible,nullptr));assert(complete(s)==ScanState::Complete);assert(!s.count()&&!f.session);}
 {Fixture f;InboxScan s;f.add("../escape",true);f.add("bad:name",true);f.add("bad\\name",true);f.add(".",true);f.add("..",true);
  risc_storage_dirent_v1 invalid{};std::memset(invalid.name,'x',sizeof(invalid.name));invalid.is_directory=1;f.entries.push_back(invalid);
  f.directory("good");assert(s.begin(f.volume(),eligible,nullptr));assert(complete(s)==ScanState::Complete);assert(s.count()==1&&!std::strcmp(s.get(0)->row.id,"good"));
 }
 {Fixture f;InboxScan s;f.directory("driver-one");f.beginBusy=1000;
  assert(s.begin(f.volume(),eligible,nullptr));for(unsigned i=0;i<1000;++i)assert(s.step()==ScanState::Working);assert(!f.session&&!f.submitted);
  assert(s.step()==ScanState::Working);assert(s.step()==ScanState::Working);assert(f.active);
  f.pending=1000;unsigned submitted=f.submitted;
  for(unsigned i=0;i<1000;++i)assert(s.step()==ScanState::Working);
  assert(f.submitted==submitted&&f.active&&f.session);
  f.endPending=1000;for(unsigned i=0;i<1000;++i)assert(s.cancel()==ScanState::Working);
  assert(f.active&&f.session&&f.submitted==submitted);
  assert(s.cancel()==ScanState::Failed);assert(!f.session&&!f.active);
 }
 {Fixture f;InboxScan s;assert(s.begin(f.volume(),eligible,nullptr));s.step();s.step();f.retainedCollect=true;
  assert(s.step()==ScanState::Retained);unsigned calls=f.collected+f.ended+f.submitted;
  for(unsigned i=0;i<1000;++i)assert(s.cancel()==ScanState::Retained);
  assert(calls==f.collected+f.ended+f.submitted&&f.session&&f.active);
  assert(!s.begin(f.volume(),eligible,nullptr));
 }
 {Fixture f;InboxScan s;assert(s.begin(f.volume(),eligible,nullptr));s.step();f.retainedEnd=true;assert(s.cancel()==ScanState::Retained);assert(f.session);}
 {Fixture f;InboxScan s;f.directory("driver-one");const auto old=manifest("driver-one","driver","1.0.0");f.files["/Drivers/driver-one/.package.json"]=std::vector<uint8_t>(old.begin(),old.end());
  assert(s.begin(f.volume(),eligible,nullptr));assert(complete(s)==ScanState::Complete);assert(s.get(0)->row.flags&DRIVER_MANAGER_CAN_UPDATE);assert(s.get(0)->row.flags&DRIVER_MANAGER_CAN_UNINSTALL);
 }
 return 0;
}
