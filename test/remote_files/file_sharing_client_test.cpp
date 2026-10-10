#include "PortableFileSharing.h"
#include "RiscEntropySourceV1.h"
#include "RiscTcpConnectionV1.h"
#include "WebDavFixture.h"
#include <openssl/sha.h>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cstdio>
using namespace WebDavTest;
namespace {
Fixture *files;
std::map<void*,size_t> memory;
std::set<unsigned> grants;
std::vector<unsigned> releaseOrder;
std::vector<uint64_t> closeOrder;
unsigned calls=0,randomCalls=0,cycle=0,failAcquire=99,failRelease=99;
bool owner=true,oom=false,closeFault=false,acceptNext=false,ownerLostFill=false;
int32_t entropyResult=0;
std::string input,output;size_t inputOffset=0;
void *allocate(size_t n){if(oom)return nullptr;void*p=std::malloc(n);assert(p);memory[p]=n;return p;}
void deallocate(void*p){assert(memory.erase(p)==1);std::free(p);}
bool owns(void*){return owner;}
std::string hash(const std::string&s){unsigned char bytes[32];SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),bytes);char text[65]{};for(unsigned i=0;i<32;++i)std::snprintf(text+2*i,3,"%02x",bytes[i]);return text;}
const risc_entropy_source_v1 entropy{1,sizeof(entropy),reinterpret_cast<void*>(1),[](void*,void*out,uint32_t size)->int32_t{
 ++calls;++randomCalls;assert(size==32&&grants.count(2));if(ownerLostFill)owner=false;
 if(entropyResult)return entropyResult;
 for(unsigned i=0;i<size;++i)static_cast<unsigned char*>(out)[i]=i+cycle;
 return 0;
}};
const risc_tcp_connection_v1 tcp{1,sizeof(tcp),reinterpret_cast<void*>(2),
 [](void*,const risc_tcp_connection_listen_v1*bind,uint64_t*out)->int32_t{++calls;assert(grants.count(0)&&grants.count(1)&&!grants.count(2));assert(bind->port==8080);*out=10;return 0;},
 [](void*,uint64_t h,uint64_t*out)->int32_t{++calls;assert(h==10);*out=0;if(!acceptNext)return 1;acceptNext=false;*out=20;return 0;},
 [](void*,uint64_t h,void*out,uint32_t cap,uint32_t*size)->int32_t{++calls;assert(h==20);*size=std::min<size_t>({cap,37,input.size()-inputOffset});if(!*size)return 1;std::memcpy(out,input.data()+inputOffset,*size);inputOffset+=*size;return 0;},
 [](void*,uint64_t h,const void*in,uint32_t cap,uint32_t*size)->int32_t{++calls;assert(h==20);*size=std::min<uint32_t>(cap,43);output.append(static_cast<const char*>(in),*size);return 0;},
 [](void*,uint64_t h)->int32_t{++calls;closeOrder.push_back(h);return closeFault?RISC_TCP_CONNECTION_RETAINED:0;}};
const risc_runtime_api_v1 runtime=[] {risc_runtime_api_v1 result{};result.api_version=1;result.struct_size=sizeof(result);result.acquire=
 [](const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*out)->bool{
  ++calls;assert(version==1);unsigned slot=std::strcmp(name,RISC_APP_DATA_EXPORT_CAPABILITY)==0?0:std::strcmp(name,RISC_TCP_CONNECTION_CAPABILITY)==0?1:2;
  assert(instance==10+slot&&!grants.count(slot));if(slot==failAcquire)return false;
  grants.insert(slot);out->slot=slot;out->generation=1;out->api=slot==0?static_cast<const void*>(&files->api):slot==1?static_cast<const void*>(&tcp):static_cast<const void*>(&entropy);return true;
 };result.release=[](risc_runtime_capability_v1*grant)->bool{++calls;assert(grants.count(grant->slot));releaseOrder.push_back(grant->slot);if(grant->slot==failRelease)return false;grants.erase(grant->slot);return true;};return result;}();
portable_file_sharing_status status(portable_file_sharing*p){portable_file_sharing_status s;portable_file_sharing_get_status(p,&s);return s;}
void tick(portable_file_sharing*p,uint64_t now=1){auto before=calls+files->calls;portable_file_sharing_tick(p,now);assert(calls+files->calls-before<=1);}
void ready(portable_file_sharing*p){for(unsigned i=0;i<20&&status(p).state==PORTABLE_FILE_SHARING_PREPARING;++i)tick(p);assert(status(p).state==PORTABLE_FILE_SHARING_LISTENING);}
void finished(portable_file_sharing*p,unsigned count){for(unsigned i=0;i<10000&&status(p).completed_requests<count;++i)tick(p);assert(status(p).completed_requests==count);}
void terminal(portable_file_sharing*p){assert(status(p).state==PORTABLE_FILE_SHARING_RETAINED&&status(p).active_work&&!status(p).password[0]);auto before=calls+files->calls;portable_file_sharing_stop(p);tick(p);assert(!portable_file_sharing_close(p,1));assert(!portable_file_sharing_destroy(&p));assert(calls+files->calls==before);}
}
int main(int argc,char**argv){
 assert(argc==2);std::string mode=argv[1];Fixture fixture;files=&fixture;
 portable_file_sharing_hooks hooks{nullptr,owns,allocate,deallocate};
 if(mode=="oom")oom=true;
 auto*p=portable_file_sharing_create(&runtime,&hooks);
 if(mode=="oom"){assert(!p&&!calls&&memory.empty());std::puts("file-sharing client OOM PASS");return 0;}
 assert(p&&!calls&&memory.size()==1);const size_t workspace=memory.begin()->second;
 portable_file_sharing_config config{10,11,12,{192,0,2,1},8080,100000,true};
 if(mode=="transport-choice"){config.authenticated_http=false;assert(!portable_file_sharing_start(p,&config,0));assert(!calls&&status(p).state==PORTABLE_FILE_SHARING_ERROR);assert(portable_file_sharing_destroy(&p)&&memory.empty());return 0;}
 assert(portable_file_sharing_start(p,&config,0)&&!calls&&status(p).active_work&&!status(p).password[0]);
 if(mode=="missing")failAcquire=1;
 if(mode=="release-fault")failRelease=2;
 if(mode=="owner-loss")ownerLostFill=true;
 if(mode=="entropy-terminal")entropyResult=RISC_ENTROPY_SOURCE_CONTEXT;
 if(mode=="cancel-prepare"){tick(p);portable_file_sharing_stop(p);assert(portable_file_sharing_close(p,1));assert(randomCalls==0&&grants.empty()&&closeOrder.empty());}
 else if(mode=="expiry-prepare"){tick(p);for(unsigned i=0;i<10&&status(p).active_work;++i)tick(p,100000);assert(!status(p).active_work&&grants.empty()&&!randomCalls);}
 else if(mode=="missing"){for(unsigned i=0;i<12&&status(p).active_work;++i)tick(p);assert(status(p).state==PORTABLE_FILE_SHARING_ERROR&&grants.empty()&&!randomCalls);}
 else if(mode=="busy"||mode=="unavailable"){
  entropyResult=mode=="busy"?RISC_ENTROPY_SOURCE_BUSY:RISC_ENTROPY_SOURCE_UNAVAILABLE;
  for(unsigned i=0;i<10;++i)tick(p);
  assert(randomCalls==7&&closeOrder.empty()&&status(p).state==PORTABLE_FILE_SHARING_PREPARING&&!status(p).password[0]);
  entropyResult=0;ready(p);assert(portable_file_sharing_close(p,1));
 }else if(mode=="release-fault"||mode=="owner-loss"||mode=="entropy-terminal"){
  for(unsigned i=0;i<10&&status(p).state!=PORTABLE_FILE_SHARING_RETAINED;++i)tick(p);
  terminal(p);std::cout<<"file-sharing client "<<mode<<" terminal PASS\n";return 0;
 }else{
  ready(p);auto initial=status(p);assert(std::string(initial.username)=="files"&&std::strlen(initial.password)==16&&releaseOrder==std::vector<unsigned>{2});
  if(mode=="read"){
   input="GET /points/data HTTP/1.1\r\nHost: device\r\nContent-Length: 0\r\n\r\n";acceptNext=true;finished(p,1);assert(status(p).last_http_status==401&&!files->calls);
   auto start=output.find("nonce=\"");assert(start!=std::string::npos);auto nonce=output.substr(start+7);nonce=nonce.substr(0,nonce.find('"'));
   auto response=hash(hash(std::string("files:RiscRTE files:")+initial.password)+":"+nonce+":00000001:client:auth:"+hash("GET:/points/data"));
   input="GET /points/data HTTP/1.1\r\nHost: device\r\nAuthorization: Digest username=\"files\", realm=\"RiscRTE files\", nonce=\""+nonce+"\", uri=\"/points/data\", algorithm=SHA-256, qop=auth, nc=00000001, cnonce=\"client\", response=\""+response+"\"\r\nContent-Length: 0\r\n\r\n";
   output.clear();inputOffset=0;acceptNext=true;finished(p,2);assert(status(p).last_http_status==200&&output.find("\r\n\r\nold")!=std::string::npos);
  }
  if(mode=="cancel-client"||mode=="close-fault"){acceptNext=true;tick(p);assert(status(p).state==PORTABLE_FILE_SHARING_SERVING);}
  if(mode=="close-fault"){closeFault=true;assert(!portable_file_sharing_close(p,1));terminal(p);std::cout<<"file-sharing client close-fault terminal PASS\n";return 0;}
  if(mode=="expiry"){for(unsigned i=0;i<10&&status(p).active_work;++i)tick(p,100000);assert(!status(p).active_work);}
  else if(mode=="clock-back"){tick(p,50);for(unsigned i=0;i<10&&status(p).active_work;++i)tick(p,1);assert(status(p).state==PORTABLE_FILE_SHARING_ERROR);}
  else assert(portable_file_sharing_close(p,1));
  assert(!status(p).active_work&&!status(p).password[0]&&grants.empty()&&closeOrder.back()==10);
  if(mode=="restart"){++cycle;assert(portable_file_sharing_start(p,&config,0));ready(p);assert(std::string(status(p).password)!=initial.password);assert(portable_file_sharing_close(p,1));}
 }
 assert(portable_file_sharing_destroy(&p)&&!p&&memory.empty());std::cout<<"file-sharing client "<<mode<<" PASS (workspace "<<workspace<<" bytes)\n";
}
