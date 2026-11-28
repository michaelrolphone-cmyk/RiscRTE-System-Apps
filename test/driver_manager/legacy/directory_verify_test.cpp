#include "DriverDirectoryVerify.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using DriverManager::DirectoryVerify;
using VerifyState=DirectoryVerify::State;
using namespace RuntimePackages;
static std::string manifest(const char*id,const char*kind="driver",const char*version="1.2.3"){
 return std::string("{\"schema\":1,\"kind\":\"")+kind+"\",\"id\":\""+id+"\",\"version\":\""+version+
 "\",\"artifact\":\"driver.elf\",\"architecture\":\"xtensa-esp32s3\",\"min_runtime_api\":2,\"entries\":[{\"name\":\"driver.elf\",\"size_bytes\":52,\"sha256\":\""+
 std::string(64,'a')+"\",\"executable\":true}],\"requires\":[]}";
}
struct Fixture{
 risc_storage_volume_api_v1_download api{};
 std::vector<risc_storage_dirent_v1> entries;
 std::map<std::string,std::vector<risc_storage_dirent_v1>> directories;std::string currentDirectory;
 unsigned hashes=0,nativeChecks=0;std::string failShaPath;bool failNative=false;std::vector<uint32_t> validations;
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
    assert(f.package.operation==RISC_STORAGE_PACKAGE_VALIDATE_DATA||f.package.operation==RISC_STORAGE_PACKAGE_VALIDATE_DRIVER);
    f.validations.push_back(f.package.operation);
    auto i=f.files.find(f.package.path);if(i==f.files.end())return RISC_STORAGE_ASYNC_NOT_FOUND;
    if(f.package.size!=i->second.size())return RISC_STORAGE_ASYNC_IO;
    if(f.package.operation==RISC_STORAGE_PACKAGE_VALIDATE_DATA){
     ++f.hashes;if(f.failShaPath==f.package.path)return RISC_STORAGE_ASYNC_IO;
     uint8_t expected=std::strstr(f.package.path,"readme.txt")?0xbb:0xaa;
     for(unsigned j=0;j<32;++j)assert(f.package.sha256[j]==expected);
    }else {++f.nativeChecks;if(f.failNative)return RISC_STORAGE_ASYNC_IO;}
    return RISC_STORAGE_ASYNC_OK;
   }
   switch(f.request.operation){
    case RISC_STORAGE_ASYNC_REFRESH:return RISC_STORAGE_ASYNC_OK;
    case RISC_STORAGE_ASYNC_OPEN_DIRECTORY:f.currentDirectory=f.request.path;f.position=0;++f.opened;return f.directories.count(f.currentDirectory)?RISC_STORAGE_ASYNC_OK:RISC_STORAGE_ASYNC_NOT_FOUND;
    case RISC_STORAGE_ASYNC_NEXT_ENTRY:if(f.position==f.directories.at(f.currentDirectory).size())return RISC_STORAGE_ASYNC_END;out->entry=f.directories.at(f.currentDirectory)[f.position++];return RISC_STORAGE_ASYNC_OK;
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

static risc_storage_dirent_v1 dirent(const char*name,uint64_t size=0,bool directory=false){
 risc_storage_dirent_v1 e{};assert(std::strlen(name)<sizeof(e.name));std::strcpy(e.name,name);e.size=size;e.is_directory=directory;return e;
}
static void populate(Fixture&f,const std::string&root,bool nested=false){
 std::string json=manifest("driver-one");
 if(nested){
  const auto schema=json.find("\"schema\":1");assert(schema!=std::string::npos);json.replace(schema,10,"\"schema\":2");
  const auto where=json.find("],\"requires\"");assert(where!=std::string::npos);
  const std::string resource=",{\"name\":\"assets/readme.txt\",\"size_bytes\":4,\"sha256\":\""+std::string(64,'b')+"\",\"executable\":false}";
  json.insert(where,resource);
 }
 f.files[root+"/.package.json"]=std::vector<uint8_t>(json.begin(),json.end());
 f.files[root+"/driver.elf"]=std::vector<uint8_t>(52);
 f.directories[root]={dirent(".package.json",json.size()),dirent("driver.elf",52)};
 if(nested){f.directories[root].push_back(dirent("assets",0,true));f.directories[root+"/assets"]={dirent("readme.txt",4)};f.files[root+"/assets/readme.txt"]={1,2,3,4};}
}
static VerifyState complete(DirectoryVerify&v){for(unsigned i=0;i<1000;++i){const auto s=v.step();if(s!=VerifyState::Working)return s;}assert(false);return VerifyState::Failed;}
static void terminal(Fixture&f,DirectoryVerify&v,VerifyState expected){
 const auto calls=f.submitted+f.collected+f.ended;
 for(unsigned i=0;i<1000;++i)assert(v.step()==expected);
 assert(calls==f.submitted+f.collected+f.ended&&f.session==17&&!f.ended);
}
int main(){
 const std::string root="/Drivers/.driver-one.pkg-stage";
 {Fixture f;f.session=17;populate(f,root,true);DirectoryVerify v;
  assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one","1.2.3",true));
  assert(!std::strcmp(v.root(),root.c_str()));assert(complete(v)==VerifyState::Ready);
  assert(f.hashes==2&&f.nativeChecks==1&&f.opened==2);
  assert(f.validations==std::vector<uint32_t>({113,107,113}));assert(v.plan().entryCount==2);terminal(f,v,VerifyState::Ready);
 }
 {Fixture f;f.session=17;populate(f,root);DirectoryVerify v;assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));
  assert(complete(v)==VerifyState::Ready);assert(f.hashes==1&&f.nativeChecks==0);}
 // Every extra/duplicate/case-alias/size mismatch is a failed exact inventory.
 for(unsigned variant=0;variant<10;++variant){
  Fixture f;f.session=17;populate(f,root,true);auto&rows=f.directories[root];
  if(variant==0)rows.push_back(dirent("unexpected.bin",1));
  if(variant==1)rows.push_back(rows[1]);
  if(variant==2)rows.push_back(rows[2]);
  if(variant==3)rows.push_back(dirent("DRIVER.ELF",52));
  if(variant==4)++rows[1].size;
  if(variant==5)rows.erase(rows.begin()+1);
  if(variant==6)rows.erase(rows.begin()+2);
  if(variant==7)f.directories[root+"/assets"].clear();
  if(variant==8)f.directories.erase(root+"/assets");
  if(variant==9)rows.push_back(rows[0]);
  DirectoryVerify v;assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));
  assert(complete(v)==VerifyState::Failed);assert(!f.hashes);terminal(f,v,VerifyState::Failed);
 }
 {Fixture f;f.session=17;populate(f,root);f.directories[root].push_back(dirent("bad/name",1));DirectoryVerify v;
  assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));assert(complete(v)==VerifyState::Failed);}
 {Fixture f;f.session=17;populate(f,root);risc_storage_dirent_v1 bad{};std::memset(bad.name,'x',sizeof(bad.name));f.directories[root].push_back(bad);DirectoryVerify v;
  assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));assert(complete(v)==VerifyState::Failed);}
 // Failure to read the initial manifest means Missing, not an empty directory.
 {Fixture f;f.session=17;f.directories[root]={dirent("unknown-user-file",99)};DirectoryVerify v;
  assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));assert(complete(v)==VerifyState::Missing);assert(!f.opened&&f.directories[root].size()==1);}
 for(unsigned variant=0;variant<4;++variant){
  Fixture f;f.session=17;populate(f,root);auto&bytes=f.files[root+"/.package.json"];
  if(variant==0)bytes={'{','b','a','d','}'};
  if(variant==1){const auto j=manifest("different-driver");bytes=std::vector<uint8_t>(j.begin(),j.end());}
  if(variant==2){const auto j=manifest("driver-one","application");bytes=std::vector<uint8_t>(j.begin(),j.end());}
  DirectoryVerify v;assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one",variant==3?"2.0.0":nullptr));
  assert(complete(v)==VerifyState::Failed);assert(!f.opened&&!f.hashes);
 }
 for(bool nativeFailure:{false,true}){
  Fixture f;f.session=17;populate(f,root);if(nativeFailure)f.failNative=true;else f.failShaPath=root+"/driver.elf";
  DirectoryVerify v;assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one",nullptr,true));
  assert(complete(v)==VerifyState::Failed);assert(f.hashes==1&&f.nativeChecks==(nativeFailure?1u:0u));terminal(f,v,VerifyState::Failed);
 }
 {Fixture f;f.session=17;populate(f,root);DirectoryVerify v;assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));
  assert(v.step()==VerifyState::Working);f.pending=1000;const auto submitted=f.submitted;
  for(unsigned i=0;i<1000;++i)assert(v.step()==VerifyState::Working);
  assert(f.submitted==submitted&&f.active&&f.session==17);
  // The outer owner cancels/drains directly; the component must not run again.
  f.endPending=1000;for(unsigned i=0;i<1000;++i)assert(f.api.base.base.end(&f,17)==RISC_STORAGE_ASYNC_PENDING);
  assert(f.session==17&&f.active&&f.submitted==submitted);
  assert(f.api.base.base.end(&f,17)==RISC_STORAGE_ASYNC_OK);assert(!f.session&&!f.active);
 }
 {Fixture f;f.session=17;populate(f,root);DirectoryVerify v;assert(v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));
  v.step();f.retainedCollect=true;assert(v.step()==VerifyState::Retained);terminal(f,v,VerifyState::Retained);
  assert(!v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one"));}
 // Source root names are derived only from enum and validated identity.
 for(auto kind:{DirectoryVerify::Root::Installed,DirectoryVerify::Root::Previous,DirectoryVerify::Root::Removing}){
  Fixture f;f.session=17;DirectoryVerify v;assert(v.begin(f.volume(),17,kind,"driver-one"));
  const char*expected=kind==DirectoryVerify::Root::Installed?"/Drivers/driver-one":kind==DirectoryVerify::Root::Previous?"/Drivers/.driver-one.pkg-previous":"/Drivers/.driver-one.pkg-removing";
  assert(!std::strcmp(v.root(),expected));
 }
 {Fixture f;f.session=17;DirectoryVerify v;
  assert(!v.begin(f.volume(),17,static_cast<DirectoryVerify::Root>(99),"driver-one"));
  assert(!v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"../driver"));
  assert(!v.begin(f.volume(),17,DirectoryVerify::Root::Stage,"driver-one","not-a-version"));}
 return 0;
}
