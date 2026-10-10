#include "WebDavSharing.h"
#include "WebDavFixture.h"
#include "RiscProviderV2.h"
#include "../../Services/tcp_listener/native/RiscTcpListenerV1.h"
#include <openssl/sha.h>
#include <algorithm>
#include <cstdio>
using namespace RiscWebDav;using namespace WebDavTest;
extern "C" const risc_driver_v2* t5_driver_get(uint32_t);
namespace {
std::string input,output;size_t offset=0;unsigned nativeCalls=0;bool accepted=false,closeFailure=false;
std::vector<uint64_t> closed;
std::string hash(const std::string&s){unsigned char bytes[32];SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),bytes);char out[65]{};for(unsigned i=0;i<32;++i)std::snprintf(out+2*i,3,"%02x",bytes[i]);return out;}
std::string authorization(const char*method,const char*target,const std::string&nonce,const char*count,const char*body,bool integrity){
 const char* qop=integrity?"auth-int":"auth";std::string ha1=hash("files:Saved files:test-password");std::string ha2=hash(std::string(method)+":"+target+(integrity?":"+hash(body):""));
 std::string response=hash(ha1+":"+nonce+":"+count+":client:"+qop+":"+ha2);
 return "Authorization: Digest username=\"files\", realm=\"Saved files\", nonce=\""+nonce+"\", uri=\""+target+"\", algorithm=SHA-256, qop="+qop+", nc="+count+", cnonce=\"client\", response=\""+response+"\"\r\n";
}
const risc_tcp_listener_v1 native{1,sizeof(native),reinterpret_cast<void*>(1),
 [](void*,const risc_tcp_listen_v1*,uint64_t*out)->int32_t{++nativeCalls;*out=10;return 0;},
 [](void*,uint64_t h,uint64_t*out)->int32_t{++nativeCalls;assert(h==10);*out=0;if(accepted)return 1;accepted=true;*out=20;return 0;},
 [](void*,uint64_t h,void*b,uint32_t cap,uint32_t*n)->int32_t{++nativeCalls;assert(h==20);*n=std::min<size_t>({cap,13,input.size()-offset});if(!*n)return 1;std::memcpy(b,input.data()+offset,*n);offset+=*n;return 0;},
 [](void*,uint64_t h,const void*b,uint32_t cap,uint32_t*n)->int32_t{++nativeCalls;assert(h==20);*n=std::min<uint32_t>(cap,19);output.append(static_cast<const char*>(b),*n);return 0;},
 [](void*,uint64_t h)->int32_t{++nativeCalls;closed.push_back(h);return closeFailure?-6:0;}};
void tick(Sharing&sharing,Fixture&f,uint64_t now=1){unsigned before=nativeCalls+f.calls;sharing.tick(now);assert(nativeCalls+f.calls-before<=1);}
}
int main(int argc,char**argv){assert(argc==2);const std::string mode=argv[1];Fixture files;DigestSession auth;
 char password[]="test-password";unsigned char nonceBytes[32];for(unsigned i=0;i<32;++i)nonceBytes[i]=i;
 assert(auth.begin("files",password,sizeof(password)-1,"Saved files",nonceBytes,sizeof(nonceBytes),0,mode=="auth-expire"?100:100000));
 for(unsigned i=0;i<sizeof(password)-1;++i)assert(!password[i]);
 char challenge[256];assert(auth.challenge(challenge,sizeof(challenge),0));std::string nonce=challenge;auto start=nonce.find("nonce=\"");assert(start!=std::string::npos);nonce=nonce.substr(start+7);nonce=nonce.substr(0,nonce.find('"'));
 const char*method=(mode=="write"||mode=="tamper"||mode=="auth-expire"||mode=="auth-cleared")?"PUT":"GET";const char*target="/points/data";const char*body=std::strcmp(method,"PUT")?"":"new";
 std::string headers=mode=="unauthorized"?"":authorization(method,target,nonce.c_str(),"00000001",body,mode=="write"||mode=="tamper"||mode=="auth-expire"||mode=="auth-cleared");
 if(!std::strcmp(method,"PUT"))headers+="If-Match: \"risc-0000000000000007\"\r\n";
 input=std::string(method)+" "+target+" HTTP/1.1\r\nHost: watch.local\r\n"+headers+"Content-Length: "+std::to_string(std::strlen(body))+"\r\n\r\n"+(mode=="tamper"?"bad":body);
 const auto* driver=t5_driver_get(2);assert(driver);risc_provider_dependency_v1 dependency{RISC_TCP_LISTENER_CAPABILITY,1,&native};assert(driver->start(&dependency,1));
 const auto* tcp=static_cast<const risc_tcp_connection_v1*>(driver->capability);
 Sharing sharing;SharingHooks hooks{&files,[](void*c){return static_cast<Fixture*>(c)->owned;},files.hooks().allocate,files.hooks().deallocate};
 const risc_tcp_connection_listen_v1 bind{sizeof(bind),{192,0,2,1},8080,0};
 assert(sharing.begin(&files.api,tcp,&auth,hooks,bind,0,100000));assert(nativeCalls==0&&sharing.activeWork());
 if(mode=="cancel")sharing.requestStop();
 if(mode=="close-fault")closeFailure=true;
 for(unsigned i=0;i<10000&&sharing.status().state!=SharingState::Retained&&sharing.status().state!=SharingState::Off&&!sharing.status().completedRequests;++i){
  if(mode=="storage-retained"&&files.statCalls)files.nextResult=RISC_APP_DATA_RETAINED;
  if(mode=="auth-cleared"&&files.statCalls)auth.clear();
  tick(sharing,files,mode=="auth-expire"&&files.statCalls?100:1);
 }
 if(mode=="close-fault"||mode=="storage-retained"){
  assert(sharing.status().state==SharingState::Retained&&sharing.status().transportOpen&&sharing.activeWork());unsigned before=nativeCalls+files.calls;sharing.requestStop();tick(sharing,files);assert(nativeCalls+files.calls==before);if(mode=="close-fault")assert(!driver->quiesce());
  std::cout<<"WebDAV actual Digest + sharing + TCP facade: "<<mode<<" terminal fence PASS\n";return 0;
 }
 if(mode=="auth-expire"||mode=="auth-cleared"){assert(!files.writeCalls&&files.files["/points/data"].bytes=="old");assert(!sharing.activeWork()&&!auth.active()&&!sharing.status().transportOpen&&closed.size()==2&&closed[0]==20&&closed[1]==10&&files.allocations.empty());}
 else if(mode=="cancel")assert(closed.empty()&&!sharing.activeWork()&&!auth.active());
 else {
  auto status=sharing.status();assert(status.state==SharingState::Listening&&status.completedRequests==1&&closed.size()==1&&closed[0]==20&&sharing.activeWork());
  if(mode=="unauthorized"||mode=="tamper")assert(status.lastHttpStatus==401&&files.calls==0&&files.writeCalls==0);
  else if(mode=="write")assert(status.lastHttpStatus==204&&files.files["/points/data"].bytes=="new"&&files.writeCalls==1);
  else assert(status.lastHttpStatus==200&&output.find("\r\n\r\nold")!=std::string::npos);
  if(mode=="expire")tick(sharing,files,100000);else sharing.requestStop();
  for(unsigned i=0;i<10&&sharing.status().state!=SharingState::Off;++i)tick(sharing,files,mode=="expire"?100000:1);
  assert(!sharing.activeWork()&&!auth.active()&&!sharing.status().transportOpen&&closed.size()==2&&closed[1]==10&&files.allocations.empty());
 }
 assert(driver->quiesce());driver->stop();std::cout<<"WebDAV actual Digest + sharing + TCP facade: "<<mode<<" PASS\n";
}
