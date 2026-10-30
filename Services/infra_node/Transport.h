#pragma once
#include "Core.h"
#include <RiscHttpClientV2.h>
namespace Infra {
class Transport {
 public:
 risc_http_client_v2 api{};uint64_t token=0,started=0,utc=0,utcAt=0;uint32_t size=0,limit=0;int outcome=0;bool retained=false,done=false;
 char* body=nullptr;risc_http_response_v2 info{};
 bool initialize(){body=static_cast<char*>(std::malloc(32769));return body;}
 uint64_t wall(uint64_t now)const{return utc?utc+(now-utcAt)/1000:0;}
 int begin(const char* url,unsigned method,const risc_http_header_v2* headers,unsigned count,const char* request,size_t length,unsigned max,unsigned timeout,const char* timeServer,uint64_t now){
  if(retained)return -2;if(token)return 1;size=0;done=false;outcome=0;body[0]=0;info={};info.struct_size=sizeof(info);limit=max;started=now;
  const uint64_t utcNow=wall(now);
  risc_http_request_v2 r{sizeof(r),method,url,headers,count,request,static_cast<uint32_t>(length),max,timeout,utcNow,utcNow?nullptr:timeServer,0};
  auto rc=api.open(api.context,&r,&token);if(rc==RISC_HTTP_RETAINED){retained=true;return -2;}return rc==RISC_HTTP_BUSY?1:rc<0?-1:0;
 }
 int advance(uint64_t now){
  if(retained)return -2;if(!token)return -1;
  auto r=api.advance(api.context,token);if(r==RISC_HTTP_RETAINED){retained=true;return -2;}
  uint32_t got=0;char chunk[512];auto read=api.read(api.context,token,chunk,sizeof(chunk),&got);
  if(read==RISC_HTTP_RETAINED){retained=true;return -2;}
  if(got>sizeof(chunk)||size+got>limit){outcome=RISC_HTTP_SIZE;done=true;}
  else if(got){memcpy(body+size,chunk,got);size+=got;body[size]=0;}
  auto ir=api.info(api.context,token,&info);if(ir==RISC_HTTP_RETAINED||info.cleanup_retained){retained=true;return -2;}
  if(info.utc_seconds&&!utc){utc=info.utc_seconds;utcAt=now;}
  if(r<0||read<0){outcome=r<0?r:read;done=true;}else if(r==RISC_HTTP_EOF||read==RISC_HTTP_EOF){outcome=0;done=true;}
  if(!done)return 1;return close();
 }
 int close(){if(retained)return -2;if(!token)return 0;auto r=api.close(api.context,token);if(r==RISC_HTTP_RETAINED||r<0){retained=true;return -2;}if(r==RISC_HTTP_AGAIN)return 1;token=0;return 0;}
 void dispose(){if(!retained&&!token){if(body){volatile char* p=body;for(size_t i=0;i<32769;++i)p[i]=0;}std::free(body);body=nullptr;}}
};
}
