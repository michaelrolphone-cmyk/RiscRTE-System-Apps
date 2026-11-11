#include <cassert>
#include <cstring>
#include <initializer_list>
#include "../../firmware/waveshare-relay6ch/main/FreshIntent.h"
#include "../../firmware/waveshare-relay6ch/main/CompensatedClock.h"
#include "../../firmware/waveshare-relay6ch/main/StreamRecovery.h"
int main(){
 BiomeRelay::CompensatedClock clock;
 assert(!clock.ready());
 assert(clock.synchronized(0)==0&&!clock.ready());
 assert(clock.synchronized(-1)==0&&!clock.ready());
 assert(clock.synchronized(1794356565)==1&&!clock.ready());
 assert(clock.synchronized(1794356625)==2&&clock.ready());
 assert(clock.synchronized(1794356685)==2&&clock.ready());
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
 // Inject write failure: feedback retry must retain its wire sequence and must
 // not alter the accepted output intent, deadline or applied state.
 BiomeRelay::FreshIntent feedback;
 assert(feedback.bind(a,8,e));
 assert(feedback.accept(a,8,e,7,0,10000,10500,10001,105));
 const uint64_t deadline=feedback.deadline;
 unsigned attempts=0;uint64_t firstWire=0;
 assert(feedback.acknowledge(7,true,[&](uint64_t n){++attempts;firstWire=n;return false;})==0);
 assert(attempts==1&&firstWire==16&&feedback.ackSequence==0);
 assert(feedback.sequence==7&&feedback.deadline==deadline&&feedback.desired==0);
 assert(feedback.acknowledge(7,true,[&](uint64_t n){++attempts;assert(n==firstWire);return true;})==16);
 assert(attempts==2&&feedback.ackSequence==16);
 assert(feedback.acknowledge(7,true,[&](uint64_t){++attempts;return true;})==0);
 assert(attempts==2); // Delivered feedback cannot be emitted a second time.
 assert(feedback.accept(a,8,e,8,0,10001,10501,10002,106));
 assert(feedback.acknowledge(8,true,[](uint64_t){return false;})==0);
 assert(feedback.accept(a,8,e,9,0,10002,10502,10003,107));
 assert(feedback.acknowledge(9,true,[](uint64_t){return true;})==20);
 assert(feedback.acknowledge(8,true,[](uint64_t){assert(false);return true;})==0);
 assert(feedback.ackSequence==20); // A newer state supersedes pending feedback.
 // Reproduce actual sequence68: a clock26ms behind must reject, without
 // consuming its sequence. Correcting the clock may accept that still-fresh
 // input, but must retain the original expiry and reject every replay.
 BiomeRelay::FreshIntent clocked;assert(clocked.bind(a,9,e));
 assert(!clocked.accept(a,9,e,68,0,10000,10500,9974,200));
 assert(!clocked.seen&&clocked.sequence==0);
 assert(clocked.accept(a,9,e,68,0,10000,10500,10010,300));
 assert(clocked.deadline==790);
 assert(!clocked.accept(a,9,e,68,0,10000,10500,10020,310));
 assert(!clocked.accept(a,9,e,69,0,10000,10500,10500,790));
 assert(!s.accept(a,5,e,UINT64_MAX,0,10000,10500,10001,105));
}

