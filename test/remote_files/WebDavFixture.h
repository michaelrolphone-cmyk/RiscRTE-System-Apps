#pragma once
#include "WebDavCore.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace RiscWebDav;
namespace WebDavTest {
struct File {std::string bytes;uint64_t revision=1;bool writable=true,present=true;};
struct Fixture {
 static Fixture* active;
 std::map<std::string,File> files{{"/points/data",{"old",7,true,true}},{"/points/new",{"",0,true,false}},{"/readonly/info",{"status",8,false,true}}};
 std::vector<void*> allocations;
 risc_app_data_export_v1 api{};
 bool owned=true,authorized=true,allocationFailure=false,loseAfterOperation=false;
 unsigned calls=0,catalogCalls=0,statCalls=0,readCalls=0,writeCalls=0,freeCalls=0;
 int32_t nextResult=0;
 Fixture(){active=this;auto& base=api.volume.terminal.power.volume.base;base.api_version=1;base.struct_size=sizeof(api);base.context=this;
  api.export_tag=RISC_APP_DATA_EXPORT_TAG;api.export_version=1;
  api.entry=[](void*c,uint32_t n,risc_app_data_export_entry_v1* out)->int32_t{auto& f=*static_cast<Fixture*>(c);++f.calls;++f.catalogCalls;*out={};if(n>=f.files.size())return RISC_APP_DATA_NOT_FOUND;auto it=f.files.begin();std::advance(it,n);std::strcpy(out->path,it->first.c_str());out->writable=it->second.writable;return 0;};
  api.stat_revision=[](void*c,const char*p,uint32_t*z,uint64_t*r)->int32_t{auto& f=*static_cast<Fixture*>(c);++f.calls;++f.statCalls;*z=0;*r=0;auto rc=f.result();if(rc)return rc;auto it=f.files.find(p);assert(it!=f.files.end());if(!it->second.present)return RISC_APP_DATA_NOT_FOUND;*z=it->second.bytes.size();*r=it->second.revision;return 0;};
  api.read_revision=[](void*c,const char*p,uint64_t r,void*b,uint32_t cap,uint32_t*z,uint64_t*seen)->int32_t{auto& f=*static_cast<Fixture*>(c);++f.calls;++f.readCalls;*z=0;*seen=0;auto rc=f.result();if(rc)return rc;auto& v=f.files.at(p);if(!v.present)return RISC_APP_DATA_NOT_FOUND;if(v.revision!=r)return RISC_APP_DATA_STALE;assert(cap>=v.bytes.size());std::memcpy(b,v.bytes.data(),v.bytes.size());*z=v.bytes.size();*seen=v.revision;return 0;};
  api.replace_revision=[](void*c,const char*p,uint64_t r,const void*b,uint32_t n)->int32_t{auto& f=*static_cast<Fixture*>(c);++f.calls;++f.writeCalls;auto rc=f.result();if(rc)return rc;auto& v=f.files.at(p);assert(v.writable);if((v.present?v.revision:0)!=r)return RISC_APP_DATA_STALE;v.bytes.assign(static_cast<const char*>(b),n);v.present=true;v.revision=99;return 0;};
 }
 ~Fixture(){for(void* p:allocations)std::free(p);active=nullptr;}
 int32_t result(){if(loseAfterOperation)owned=false;auto rc=nextResult;nextResult=0;return rc;}
 Hooks hooks(){return {this,[](void*c){return static_cast<Fixture*>(c)->owned;},[](void*c,const Request&,AuthPhase,const void*,size_t){return static_cast<Fixture*>(c)->authorized;},"Bearer realm=\"Saved files\"",[](size_t n)->void*{if(active->allocationFailure)return nullptr;void*p=std::malloc(n);if(p)active->allocations.push_back(p);return p;},[](void*p){auto& f=*active;++f.freeCalls;for(auto it=f.allocations.begin();it!=f.allocations.end();++it)if(*it==p){f.allocations.erase(it);std::free(p);return;}assert(false);}};}
};
inline Fixture* Fixture::active=nullptr;
inline Request request(const char* method="GET",const char* path="/points/data",const char* headers="",size_t size=0){std::string wire=std::string(method)+" "+path+" HTTP/1.1\r\nHost: watch.local\r\n"+headers+"Content-Length: "+std::to_string(size)+"\r\n\r\n";Request q;assert(parseRequest(wire.data(),wire.size(),q)==0);return q;}
inline void run(Transaction& tx,Fixture& f){for(unsigned i=0;i<100&&tx.state()!=State::Ready&&tx.state()!=State::Retained;++i){auto before=f.calls;tx.step();assert(f.calls-before<=1);}assert(tx.state()==State::Ready||tx.state()==State::Retained);}
inline std::string body(const Transaction& tx){return {static_cast<const char*>(tx.body()),tx.bodySize()};}
}
