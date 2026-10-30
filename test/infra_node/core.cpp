#include "Core.h"
#include <cassert>
#include <vector>
#include <string>
#include <iostream>
using namespace Infra;
int main(){
 Infra::Allocator allocator(131072);JsonDocument d(&allocator);
 for(auto text:{"{\"x\":1,\"x\":2}","{\"x\":NaN}","{\"x\":9007199254740992}","{\"x\":1.5}","{\"x\":\"\\ud800\"}"})assert(!parse(d,text,strlen(text)));
 const char* data="{\"installation_id\":\"test\",\"pending\":[],\"cursor\":\"0\"}";
 assert(parse(d,data,strlen(data)));std::vector<uint8_t> out(StateMax);
 auto n=frame(out.data(),out.size(),data,strlen(data),7);assert(n);const char* p;size_t size;uint64_t seq;
 assert(unframe(out.data(),n,p,size,seq)&&seq==7&&size==strlen(data));
 for(size_t i=0;i<n;++i){out[i]^=1;assert(!unframe(out.data(),n,p,size,seq));out[i]^=1;}
 Cadence c[2];assert(schedulable(c,2));c[0].timeout=19000;assert(!schedulable(c,2));c[0].timeout=5000;
 c[0].start(15000*10000ULL);assert(c[0].due==15000*10001ULL&&c[0].coalesced==10000);assert(!c[0].ready(c[0].lastStarted));
 assert(leaseDeadline(10000,5000,2000)==12000&&leaseDeadline(10000,5000,5000)==10000);
 assert(retryDelay(100,999999)<=300000);
 assert(!frame(out.data(),out.size(),data,StateMax,1));
 std::string huge="{\"x\":\""+std::string(4097,'a')+"\"}";assert(!parse(d,huge.data(),huge.size()));
 std::cout<<"infra core: strict JSON, corruption at every byte, capacity, coalescing, lease and overload PASS\n";
}
