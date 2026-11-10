#include <cassert>
#include <cstring>
#include <initializer_list>
#include "../../firmware/waveshare-relay6ch/main/FreshIntent.h"
#include "../../firmware/waveshare-relay6ch/main/StreamRecovery.h"
int main(){
 BiomeRelay::FreshIntent s;const char* a="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
 const char* e="eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
 assert(s.bind(a,4,e));assert(s.output(0)==0);
 assert(s.accept(a,4,e,1,1,9900,10400,10000,100));
 assert(s.output(499)==1);assert(s.output(500)==0);
 assert(!s.accept(a,4,e,1,2,10000,10500,10001,101));
 assert(!s.accept(a,3,e,2,2,10000,10500,10001,101));
 assert(!s.accept(a,4,"old",2,2,10000,10500,10001,101));
 assert(!s.accept(a,4,e,2,2,9000,9500,10000,101));
 assert(s.accept(a,4,e,2,1,10000,10500,10001,101));
 assert(s.output(101)==1);s.detach();assert(s.output(102)==0);
 assert(!s.accept(a,4,e,2,1,10000,10500,10001,102));
 assert(s.accept(a,4,e,3,1,10000,10500,10001,103));
 s.ackSequence=19;
 s.detach();assert(s.bind(a,4,e));assert(s.output(104)==0);
 assert(s.ackSequence==19);assert(!s.accept(a,4,e,3,1,10000,10500,10001,104));
 assert(s.accept(a,4,e,4,0,10000,10500,10001,104));
 s.revoke();assert(s.output(104)==0);
 assert(!s.accept(a,4,e,4,1,10000,10500,10001,104));
 assert(s.bind(a,5,e));assert(s.ackSequence==0);assert(!s.seen);
 for(int code:{403,404,409,410})assert(BiomeRelay::bindingInvalidated(code,true,false));
 for(int code:{404,409,410})assert(BiomeRelay::bindingInvalidated(code,false,true));
 for(int code:{200,401,429,500,502,503})assert(!BiomeRelay::bindingInvalidated(code,true,true));
 assert(!BiomeRelay::bindingInvalidated(404,false,false));
 BiomeRelay::FreshIntent firstBoot,secondBoot;
 assert(firstBoot.acknowledge(100,false)==201);
 assert(firstBoot.acknowledge(100,true)==202);
 assert(firstBoot.acknowledge(100,false)==0);
 assert(secondBoot.acknowledge(101,false)>firstBoot.ackSequence);
 assert(secondBoot.acknowledge(100,true)==0);
 assert(secondBoot.acknowledge(101,true)==204);
 assert(!s.accept(a,5,e,UINT64_MAX,0,10000,10500,10001,105));
}

