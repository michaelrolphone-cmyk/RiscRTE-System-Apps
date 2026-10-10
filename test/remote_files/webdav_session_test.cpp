#include "WebDavSession.h"
#include <algorithm>
#include "WebDavFixture.h"
using namespace RiscWebDav;
using namespace WebDavTest;
namespace {
struct Socket {
 Fixture& files;std::string input,output;size_t offset=0,limit=17;
 unsigned calls=0,closes=0;int32_t readResult=0,writeResult=0,closeResult=0;
 bool loseAfter=false;bool eof=false;
 Connection api(){return {this,1,[](void*c,uint64_t,void*b,uint32_t cap,uint32_t*n)->int32_t{
  auto&s=*static_cast<Socket*>(c);++s.calls;*n=0;if(s.loseAfter)s.files.owned=false;
  if(s.readResult)return s.readResult;
  if(s.offset==s.input.size())return s.eof?2:1;
  *n=std::min<size_t>({cap,s.limit,s.input.size()-s.offset});std::memcpy(b,s.input.data()+s.offset,*n);s.offset+=*n;return 0;},
 [](void*c,uint64_t,const void*b,uint32_t cap,uint32_t*n)->int32_t{
  auto&s=*static_cast<Socket*>(c);++s.calls;*n=0;if(s.loseAfter)s.files.owned=false;
  if(s.writeResult)return s.writeResult;
  *n=std::min<size_t>(cap,s.limit);s.output.append(static_cast<const char*>(b),*n);return 0;},
 [](void*c,uint64_t)->int32_t{auto&s=*static_cast<Socket*>(c);++s.calls;++s.closes;if(s.loseAfter)s.files.owned=false;return s.closeResult;}};}
};
std::string wire(const char*method="GET",const char*path="/points/data",const char*headers="",const char*body=""){
 return std::string(method)+" "+path+" HTTP/1.1\r\nHost: watch.local\r\n"+headers+"Content-Length: "+std::to_string(std::strlen(body))+"\r\n\r\n"+body;
}
void tick(Session&s,Fixture&f,Socket&t,uint64_t now=1){const auto before=f.calls+t.calls;s.tick(now);assert(f.calls+t.calls-before<=1);}
void run(Session&s,Fixture&f,Socket&t){for(unsigned i=0;i<10000&&s.state()!=SessionState::Done&&s.state()!=SessionState::Retained;++i)tick(s,f,t);assert(s.state()==SessionState::Done||s.state()==SessionState::Retained);}
void readWrite(){
 for(size_t limit:{1,17,2048}){Fixture f;Socket t{f,wire(),""};t.limit=limit;Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.state()==SessionState::Done&&s.status()==200&&t.output.find("\r\n\r\nold")!=std::string::npos&&t.closes==1&&f.allocations.empty());assert(s.stop()&&t.closes==1);}
 {Fixture f;Socket t{f,wire("PUT","/points/data","If-Match: \"risc-0000000000000007\"\r\n","new"),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.status()==204&&f.files["/points/data"].bytes=="new"&&t.closes==1&&f.allocations.empty());}
 {Fixture f;std::string all=wire("PUT","/points/data","If-Match: \"risc-0000000000000007\"\r\nExpect: 100-continue\r\n","new");Socket t{f,all.substr(0,all.size()-3),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));for(unsigned i=0;i<10000&&s.state()!=SessionState::Body;++i)tick(s,f,t);assert(t.output=="HTTP/1.1 100 Continue\r\n\r\n"&&f.calls==0);t.input+="new";run(s,f,t);assert(s.status()==204&&f.files["/points/data"].bytes=="new");}
 {Fixture f;Socket t{f,wire("HEAD"),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.status()==200&&t.output.find("Content-Length: 3")!=std::string::npos&&t.output.find("old")==std::string::npos&&f.readCalls==0);}
}
void rejection(){
 {Fixture f;Socket t{f,wire("PUT","/points/new","If-None-Match: *\r\n","new"),""};Session s;auto h=f.hooks();h.authorize=[](void*,const Request&q,AuthPhase phase,const void*b,size_t n){assert(!std::strcmp(q.requestTarget,"/points/new"));if(phase==AuthPhase::Headers){assert(!b&&!n);return true;}assert(n==3&&!std::memcmp(b,"new",3));return false;};assert(s.begin(&f.api,h,t.api(),0,100));run(s,f,t);assert(s.status()==401&&f.calls==0&&f.writeCalls==0&&f.allocations.empty());}

 {Fixture f;f.authorized=false;Socket t{f,wire("PUT","/points/data","Expect: 100-continue\r\n","new"),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.status()==401&&t.output.find("100 Continue")==std::string::npos&&f.calls==0&&f.allocations.empty());}
 {Fixture f;Socket t{f,"GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n",""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.status()==400&&f.calls==0&&t.closes==1);}
 {Fixture f;Socket t{f,"GET / HTTP/1.1\r\nHost: a\r\nLong: "+std::string(HeaderMax,'x'),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.status()==431&&f.calls==0&&t.closes==1);}
 {Fixture f;Socket t{f,wire("PUT","/points/data","","new")+"unexpected",""};t.limit=2048;Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.status()==400&&f.calls==0&&f.writeCalls==0);}
 {Fixture f;f.allocationFailure=true;Socket t{f,wire("PUT","/points/new","If-None-Match: *\r\n","new"),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.status()==503&&f.calls==0&&f.allocations.empty());}
}
void cancellation(){
 {Fixture f;Socket t{f,wire("PUT","/points/new","If-None-Match: *\r\n","new"),""};Session s;auto h=f.hooks();h.deallocate=[](void*p){auto* owner=Fixture::active;owner->hooks().deallocate(p);owner->owned=false;};assert(s.begin(&f.api,h,t.api(),0,100));run(s,f,t);assert(s.state()==SessionState::Retained&&t.closes==1&&f.freeCalls==1&&!s.stop());}

 for(bool eof:{false,true}){Fixture f;Socket t{f,"GET /points/data HTTP/1.1\r\n",""};t.eof=eof;Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,10));for(unsigned i=0;i<5;++i)tick(s,f,t);if(!eof)tick(s,f,t,10);run(s,f,t);assert(s.state()==SessionState::Done&&t.closes==1&&f.calls==0&&f.allocations.empty());}
 {Fixture f;Socket t{f,wire(),""};t.readResult=1;Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));tick(s,f,t);assert(s.state()==SessionState::Header&&t.calls==1);assert(s.stop()&&t.closes==1);}
 {Fixture f;Socket t{f,wire(),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));while(s.state()!=SessionState::ResponseHeader)tick(s,f,t);t.writeResult=1;tick(s,f,t);assert(s.state()==SessionState::ResponseHeader&&t.output.empty());assert(s.stop()&&f.allocations.empty());}
 for(auto rc:{-2,-6}){Fixture f;Socket t{f,wire(),""};t.readResult=rc;Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);auto before=t.calls+f.calls;tick(s,f,t);assert(!s.stop()&&s.state()==SessionState::Retained&&t.calls+f.calls==before&&t.closes==0&&f.freeCalls==0);}
 {Fixture f;Socket t{f,wire(),""};t.loseAfter=true;Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.state()==SessionState::Retained&&t.closes==0&&!s.stop());}
 {Fixture f;Socket t{f,wire(),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));while(!f.statCalls)tick(s,f,t);f.nextResult=RISC_APP_DATA_RETAINED;run(s,f,t);assert(s.state()==SessionState::Retained&&t.closes==0&&f.freeCalls==0&&!s.stop());}
 {Fixture f;Socket t{f,wire(),""};t.closeResult=-6;Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(s.state()==SessionState::Retained&&t.closes==1&&f.freeCalls==0&&!s.stop()&&t.closes==1);}
 {Fixture f;Socket t{f,wire(),""};Session s;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);t.input=wire();t.output.clear();t.offset=0;assert(s.begin(&f.api,f.hooks(),t.api(),0,100));run(s,f,t);assert(t.closes==2&&s.status()==200&&f.allocations.empty());}
}
}
int main(){readWrite();rejection();cancellation();std::cout<<"WebDAV actual session: fragmentation, partial writes, admission, timeout, cancellation and terminal retention PASS\n";}
