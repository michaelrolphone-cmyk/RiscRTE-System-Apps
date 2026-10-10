#include "WebDavFixture.h"
using namespace WebDavTest;
namespace {
void parser(){
 struct Case {const char* wire;unsigned status;};
 const Case cases[]={
 {"GET / HTTP/1.1\r\nHost: a\r\n\r\n",0},
 {"OPTIONS * HTTP/1.1\r\nHost: a\r\n\r\n",0},
 {"GET /points/%64ata HTTP/1.1\r\nHost: a\r\n\r\n",0},
 {"GET /%2e%2e/secret HTTP/1.1\r\nHost: a\r\n\r\n",400},
 {"GET /a//b HTTP/1.1\r\nHost: a\r\n\r\n",400},
 {"GET /%00 HTTP/1.1\r\nHost: a\r\n\r\n",400},
 {"GET /%0a HTTP/1.1\r\nHost: a\r\n\r\n",400},
 {"GET /%5c HTTP/1.1\r\nHost: a\r\n\r\n",400},
 {"GET / HTTP/1.1\r\n\r\n",400},
 {"GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n",400},
 {"GET / HTTP/1.1\r\nHost: a\r\n Content-Length: 0\r\n\r\n",400},
 {"PUT /a HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",400},
 {"PUT /a HTTP/1.1\r\nHost: a\r\nContent-Length: 99999999999999\r\n\r\n",400},
 {"PUT /a HTTP/1.1\r\nHost: a\r\nContent-Length: 65537\r\n\r\n",413},
 {"PUT /a HTTP/1.1\r\nHost: a\r\n\r\n",400},
 {"GET / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n",415},
 {"GET / HTTP/1.1\r\nHost: a\r\nIf-Match: bad\r\n\r\n",400},
 {"GET / HTTP/1.1\r\nHost: a\r\nIf-Match: \"comma,inside\", \"another\"\r\n\r\n",0},
 {"GET / HTTP/1.1\r\nHost: a\r\nIf-Match: \"a\",\r\n\r\n",400},
 {"GET / HTTP/1.1\r\nHost: a\r\nExpect: anything\r\n\r\n",417},
 {"PROPFIND / HTTP/1.1\r\nHost: a\r\nDepth: 3\r\n\r\n",400},
 {"GET / HTTP/1.0\r\nHost: a\r\n\r\n",400}};
 for(const auto& c:cases){Request q;assert(parseRequest(c.wire,std::strlen(c.wire),q)==c.status);}
 Request q;std::string huge(HeaderMax+1,'a');assert(parseRequest(huge.data(),huge.size(),q)==431);
}
void basics(){
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));run(tx,f);assert(tx.status()==200&&body(tx)=="old"&&f.readCalls==1);assert(std::strstr(tx.header(),"ETag: \"risc-0000000000000007\""));assert(tx.finish()&&f.allocations.empty());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("HEAD"),nullptr,0));run(tx,f);assert(tx.status()==200&&tx.bodySize()==0&&f.readCalls==0&&std::strstr(tx.header(),"Content-Length: 3\r\n"));assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("GET","/outside"),nullptr,0));run(tx,f);assert(tx.status()==404&&f.statCalls==0);assert(tx.finish());}
 {Fixture f;f.authorized=false;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));assert(tx.status()==401&&f.calls==0&&std::strstr(tx.header(),"WWW-Authenticate: Bearer realm=\"Saved files\""));assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("OPTIONS","/"),nullptr,0));run(tx,f);assert(tx.status()==200&&std::strstr(tx.header(),"Allow: OPTIONS, GET, HEAD, PROPFIND, PUT"));assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("DELETE"),nullptr,0));run(tx,f);assert(tx.status()==405&&f.statCalls==0);assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("GET","/points/data","If-None-Match: W/\"risc-0000000000000007\"\r\n"),nullptr,0));run(tx,f);assert(tx.status()==304&&tx.bodySize()==0&&f.readCalls==0&&!std::strstr(tx.header(),"Content-Length:"));assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("GET","/points/data","If-Match: W/\"risc-0000000000000007\"\r\n"),nullptr,0));run(tx,f);assert(tx.status()==412&&f.readCalls==0);assert(tx.finish());}
}
void writes(){
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/points/data","If-Match: \"risc-0000000000000007\"\r\nIf-None-Match: W/\"risc-0000000000000007\"\r\n",3),"new",3));run(tx,f);assert(tx.status()==412&&f.writeCalls==0&&f.files["/points/data"].bytes=="old");assert(tx.finish());}

 const char* value="new";
 for(const char* condition:{"","If-Match: *\r\n"}){Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/points/data",condition,3),value,3));run(tx,f);assert(tx.status()==428&&f.writeCalls==0&&f.files["/points/data"].bytes=="old");assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/points/data","If-Match: \"risc-0000000000000007\"\r\n",3),value,3));run(tx,f);assert(tx.status()==204&&f.writeCalls==1&&f.files["/points/data"].bytes=="new"&&!std::strstr(tx.header(),"Content-Length:"));assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/points/new","If-None-Match: *\r\n",3),value,3));run(tx,f);assert(tx.status()==201&&f.files["/points/new"].bytes=="new");assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/points/new","",3),value,3));run(tx,f);assert(tx.status()==428&&f.writeCalls==0);assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/readonly/info","If-Match: *\r\n",3),value,3));run(tx,f);assert(tx.status()==403&&f.statCalls==0&&f.writeCalls==0);assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/points/data","If-Match: \"risc-0000000000000007\"\r\n",3),value,3));while(!f.statCalls)tx.step();++f.files["/points/data"].revision;run(tx,f);assert(tx.status()==412&&f.writeCalls==1&&f.files["/points/data"].bytes=="old");tx.step();assert(f.writeCalls==1);assert(tx.finish());}
 for(auto rc:{RISC_APP_DATA_NO_SPACE,RISC_APP_DATA_COMMIT_UNKNOWN,RISC_APP_DATA_UNAVAILABLE}){Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PUT","/points/data","If-Match: \"risc-0000000000000007\"\r\n",3),value,3));while(!f.statCalls)tx.step();f.nextResult=rc;run(tx,f);assert(tx.status()==(rc==RISC_APP_DATA_NO_SPACE?507u:rc==RISC_APP_DATA_COMMIT_UNKNOWN?500u:503u)&&f.writeCalls==1);tx.step();assert(f.writeCalls==1);assert(tx.finish());}
}
void failures(){
 {Fixture f;Transaction tx;auto h=f.hooks();h.deallocate=[](void*p){auto* owner=Fixture::active;owner->hooks().deallocate(p);owner->owned=false;};assert(tx.begin(&f.api,h,request(),nullptr,0));run(tx,f);assert(!tx.finish()&&tx.state()==State::Retained&&f.freeCalls==1);assert(!tx.finish()&&f.freeCalls==1);}

 for(auto rc:{RISC_APP_DATA_RETAINED,RISC_APP_DATA_CONTEXT}){Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));while(!f.statCalls)tx.step();f.nextResult=rc;run(tx,f);assert(tx.state()==State::Retained&&!tx.header()&&!tx.body()&&!tx.finish());auto count=f.calls;tx.step();assert(f.calls==count&&f.freeCalls==0);}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));while(!f.statCalls)tx.step();f.loseAfterOperation=true;run(tx,f);assert(tx.state()==State::Retained&&!tx.finish()&&f.freeCalls==0);}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));while(!f.statCalls)tx.step();f.files["/points/data"].revision++;run(tx,f);assert(tx.status()==412&&body(tx).find("reload")!=std::string::npos&&f.readCalls==1);assert(tx.finish());}
 {Fixture f;f.allocationFailure=true;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));run(tx,f);assert(tx.status()==503&&f.readCalls==0);assert(tx.finish());}
 {Fixture f;Transaction tx;auto q=request();std::memset(q.path,'x',sizeof(q.path));assert(!tx.begin(&f.api,f.hooks(),q,nullptr,0)&&f.calls==0);}
 {Fixture f;Transaction tx;auto h=f.hooks();h.authenticationChallenge="Bearer\r\nInjected: yes";assert(!tx.begin(&f.api,h,request(),nullptr,0)&&f.calls==0);}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));assert(tx.finish()&&f.calls==0);}
}
void properties(){
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PROPFIND","/","Depth: 1\r\n"),nullptr,0));run(tx,f);auto s=body(tx);assert(tx.status()==207&&s.find("/points/")!=std::string::npos&&s.find("/readonly/")!=std::string::npos&&f.statCalls==0);assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PROPFIND","/points","Depth: 1\r\n"),nullptr,0));run(tx,f);auto s=body(tx);assert(tx.status()==207&&s.find("/points/data")!=std::string::npos&&s.find("/points/new")==std::string::npos&&f.statCalls==2);assert(tx.finish());}
 {Fixture f;Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PROPFIND","/points"),nullptr,0));run(tx,f);assert(tx.status()==403&&f.statCalls==0);assert(tx.finish());}
 struct XMLCase {const char* input;unsigned status;const char* present;const char* absent;};
 const XMLCase cases[]={
 {"<propfind xmlns='DAV:'><prop><xml:lang/></prop></propfind>",207,"<xml:lang/>","xmlns:U=\"http://www.w3.org/XML/1998/namespace\""},
 {"<D:propfind xmlns:D='DAV:'><D:prop><D:getetag/><Z:other xmlns:Z='urn:other'/><noNamespace/></D:prop></D:propfind>",207,"HTTP/1.1 404 Not Found","<D:getcontentlength>"},
 {"<propfind xmlns='DAV:'><propname/></propfind>",207,"<D:getetag></D:getetag>","risc-000000"},
 {"<propfind xmlns='DAV:'><allprop/><include><Z:unknown xmlns:Z='urn:a&amp;b'/></include></propfind>",207,"xmlns:U=\"urn:a&amp;b\"","secret"},
 {"<propfind xmlns='DAV:'><prop/></propfind>",207,"<D:prop/>","<D:getetag>"},
 {"<!DOCTYPE x><propfind xmlns='DAV:'><allprop/></propfind>",400,"Invalid or unsupported",nullptr},
 {"<propfind xmlns='DAV:'><allprop/><propname/></propfind>",400,"Invalid or unsupported",nullptr}};
 for(const auto& c:cases){Fixture f;Transaction tx;auto q=request("PROPFIND","/points/data","Depth: 0\r\nContent-Type: application/xml; charset=utf-8\r\n",std::strlen(c.input));assert(tx.begin(&f.api,f.hooks(),q,c.input,std::strlen(c.input)));run(tx,f);auto b=body(tx);assert(tx.status()==c.status&&b.find(c.present)!=std::string::npos&&(!c.absent||b.find(c.absent)==std::string::npos));if(c.status!=207)assert(f.statCalls==0);assert(tx.finish());}
 {Fixture f;const char* xml="<propfind xmlns='DAV:'><prop><getetag/></prop></propfind>";Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PROPFIND","/points","Depth: 0\r\n",std::strlen(xml)),xml,std::strlen(xml)));run(tx,f);assert(tx.status()==207&&body(tx).find("HTTP/1.1 404 Not Found")!=std::string::npos&&f.statCalls==0);assert(tx.finish());}
 {Fixture f;const char* xml="<propfind xmlns='DAV:'><allprop/></propfind>";Transaction tx;assert(tx.begin(&f.api,f.hooks(),request("PROPFIND","/points","Depth: 0\r\nContent-Type: text/html\r\n",std::strlen(xml)),xml,std::strlen(xml)));run(tx,f);assert(tx.status()==415&&f.statCalls==0);assert(tx.finish());}
 {Fixture f;f.files["/points/data"].bytes.clear();Transaction tx;assert(tx.begin(&f.api,f.hooks(),request(),nullptr,0));run(tx,f);assert(tx.status()==200&&tx.bodySize()==0&&f.readCalls==1);assert(tx.finish());}

}
}
int main(){parser();basics();writes();failures();properties();std::cout<<"WebDAV actual core: parsing, scope, preconditions, snapshots, retention and cleanup PASS\n";}
