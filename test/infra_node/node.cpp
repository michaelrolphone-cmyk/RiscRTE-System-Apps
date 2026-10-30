#include "Node.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
using namespace Infra;
static uint64_t tick=0;static std::string policyBytes,statePath,responseBytes,pendingBytes;static uint64_t revision=1;static unsigned responseAt=0,statusCode=200;static int outcome=0;static bool online=true;static uint64_t utc=0;
std::string file(const std::string& path){std::ifstream s(path,std::ios::binary);return {std::istreambuf_iterator<char>(s),{}};}
int stat(void*,const char* name,uint32_t* size,uint64_t* rev){*size=std::string(name)=="node-policy.json"?policyBytes.size():file(statePath).size();*rev=revision;return 0;}
int read(void*,const char* name,uint64_t expected,void* out,uint32_t cap,uint32_t* size,uint64_t* rev){if(expected!=revision)return RISC_APP_DATA_STALE;const auto v=std::string(name)=="node-policy.json"?policyBytes:file(statePath);assert(v.size()<=cap);memcpy(out,v.data(),v.size());*size=v.size();*rev=revision;return 0;}
int begin(void*,const char*,uint64_t expected,const void* bytes,uint32_t size,uint64_t* token){assert(expected==revision);pendingBytes.assign(static_cast<const char*>(bytes),size);*token=9;return 0;}
int collect(void*,uint64_t token,int32_t* result){assert(token==9);std::ofstream stream(statePath+".new",std::ios::binary);stream.write(pendingBytes.data(),pendingBytes.size());stream.close();assert(!rename((statePath+".new").c_str(),statePath.c_str()));++revision;*result=0;return 0;}
risc_bound_app_data_async_v1 storage{{1,sizeof(storage),nullptr,stat,read,nullptr},RISC_BOUND_APP_DATA_ASYNC_TAG,1,begin,collect};
int httpOpen(void*,const risc_http_request_v2* r,uint64_t* token){
 JsonDocument call;call["url"]=r->url;call["method"]=r->method;call["now"]=tick;call["pid"]=getpid();for(unsigned i=0;i<r->header_count;++i)call["headers"][r->headers[i].name]=r->headers[i].value;
 if(r->body_bytes){JsonDocument body;assert(!deserializeJson(body,static_cast<const char*>(r->body),r->body_bytes));call["body"]=body;}
 std::string encoded;serializeJson(call,encoded);std::cout<<encoded<<std::endl;std::string line;if(!std::getline(std::cin,line))exit(0);JsonDocument reply;assert(!deserializeJson(reply,line));
 if(reply["done"]==true)exit(0);responseBytes.clear();serializeJson(reply["body"],responseBytes);responseAt=0;statusCode=reply["status"]|200;outcome=reply["outcome"]|0;utc=reply["utc"]|0ULL;*token=1;return 0;
}
int httpAdvance(void*,uint64_t){return outcome?outcome:responseAt==responseBytes.size()?RISC_HTTP_EOF:RISC_HTTP_AGAIN;}
int httpRead(void*,uint64_t,void* out,uint32_t cap,uint32_t* count){if(outcome){*count=0;return outcome;}*count=std::min<size_t>(cap,responseBytes.size()-responseAt);memcpy(out,responseBytes.data()+responseAt,*count);responseAt+=*count;return *count?0:RISC_HTTP_EOF;}
int httpInfo(void*,uint64_t,risc_http_response_v2* r){r->status_code=statusCode;r->sent_bytes=100;r->utc_seconds=utc;strcpy(r->etag,"\"source-v1\"");return 0;}
int httpClose(void*,uint64_t){return 0;}
int connect(void*,const char*,const char*){return online?0:-1;}int poll(void*){return 0;}int closeStation(void*){return 0;}uint64_t clockNow(void*){return tick;}int entropy(void*,void* out,uint32_t n){static unsigned nonce=0;for(unsigned i=0;i<n;++i)static_cast<unsigned char*>(out)[i]=getpid()+i+nonce++;return 0;}
int main(int argc,char**argv){assert(argc==4);policyBytes=file(argv[1]);statePath=argv[2];tick=strtoull(argv[3],nullptr,10);
 Node n;assert(n.journal.initialize(&storage.base));n.net.api={2,sizeof(n.net.api),nullptr,httpOpen,httpAdvance,httpRead,httpInfo,httpClose};n.clock={1,sizeof(n.clock),nullptr,clockNow};n.entropy={1,sizeof(n.entropy),nullptr,entropy};n.station={1,sizeof(n.station),nullptr,connect,poll,closeStation};n.memory={sizeof(n.memory),262144,131072,65536,32768,8000000,4000000,16000};
 for(unsigned i=0;i<50000;++i){int r=n.advance();if(r<0){std::cerr<<"NODE BLOCKED phase="<<unsigned(n.phase)<<" action="<<unsigned(n.action)<<" reason="<<n.status.reason<<" response="<<responseBytes<<std::endl;return 2;}tick+=10;}
 std::cerr<<"Simulation exceeded tick bound phase="<<unsigned(n.phase)<<" reason="<<n.status.reason<<" job="<<(n.journal.state["job"]["phase"]|"")<<" time="<<tick<<" "<<std::endl;return 3;
}
