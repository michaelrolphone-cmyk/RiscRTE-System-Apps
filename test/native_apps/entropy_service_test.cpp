#include "../../Services/entropy/service.cpp"
#include <cassert>
#include <cstdio>
#include <string>
namespace Fake {
unsigned calls;int32_t result;bool reenter;
risc_entropy_source_v1 copied;const void *caller;
int32_t fill(void*context,void*out,uint32_t size){
 assert(context==reinterpret_cast<void*>(7));assert(out!=caller);++calls;
 if(reenter){unsigned char b=0xaa;assert(copied.fill(copied.context,&b,1)==RISC_ENTROPY_SOURCE_BUSY&&b==0xaa);assert(!driver.quiesce());driver.stop();}
 for(unsigned i=0;i<size;++i)static_cast<unsigned char*>(out)[i]=static_cast<unsigned char>(i+1);
 return result;
}
}
int main(int argc,char**argv){assert(argc==2);const std::string mode=argv[1];
 risc_entropy_v1 native{1,sizeof(native),reinterpret_cast<void*>(7),Fake::fill};
 risc_provider_dependency_v1 dependency{RISC_ENTROPY_CAPABILITY,1,&native};
 assert(!t5_driver_get(1)&&t5_driver_get(2)==&driver);
 if(mode=="overflow")generation=UINTPTR_MAX;
 if(mode=="bad-dependency")native.struct_size=0;
 if(mode=="overflow"||mode=="bad-dependency"){assert(!driver.start(&dependency,1));assert(!Fake::calls);return 0;}
 assert(driver.start(&dependency,1)&&!Fake::calls);Fake::copied=*static_cast<const risc_entropy_source_v1*>(driver.capability);
 assert(!driver.start(&dependency,1));unsigned char out[33];std::memset(out,0xaa,sizeof(out));Fake::caller=out;
 if(mode=="unavailable")Fake::result=RISC_ENTROPY_UNAVAILABLE;
 if(mode=="invalid-native")Fake::result=RISC_ENTROPY_INVALID;
 if(mode=="context")Fake::result=RISC_ENTROPY_CONTEXT;
 if(mode=="retained")Fake::result=RISC_ENTROPY_RETAINED;
 if(mode=="unknown")Fake::result=99;
 Fake::reenter=mode=="reentry";
 assert(Fake::copied.fill(nullptr,out,1)==RISC_ENTROPY_SOURCE_CONTEXT);
 assert(Fake::copied.fill(Fake::copied.context,out,0)==RISC_ENTROPY_SOURCE_INVALID);
 assert(Fake::copied.fill(Fake::copied.context,out,33)==RISC_ENTROPY_SOURCE_INVALID);
 assert(Fake::copied.fill(Fake::copied.context,nullptr,1)==RISC_ENTROPY_SOURCE_INVALID&&!Fake::calls);
 int32_t rc=Fake::copied.fill(Fake::copied.context,out,32);assert(Fake::calls==1&&out[32]==0xaa);
 if(Fake::result){assert(rc==(Fake::result==99?RISC_ENTROPY_SOURCE_RETAINED:Fake::result));for(auto c:out)assert(c==0xaa);}
 else {assert(!rc);for(unsigned i=0;i<32;++i)assert(out[i]==i+1);}
 for(auto c:scratch)assert(!c);
 if(mode=="context"||mode=="retained"||mode=="unknown"){
  assert(!driver.quiesce());driver.stop();assert(Fake::copied.fill(Fake::copied.context,out,1)==rc&&Fake::calls==1);assert(!driver.start(&dependency,1));
 }else{
  assert(driver.quiesce());assert(Fake::copied.fill(Fake::copied.context,out,1)==RISC_ENTROPY_SOURCE_CONTEXT&&Fake::calls==1);
  assert(driver.start(&dependency,1));assert(Fake::copied.fill(Fake::copied.context,out,1)==RISC_ENTROPY_SOURCE_CONTEXT&&Fake::calls==1);
  const auto fresh=*static_cast<const risc_entropy_source_v1*>(driver.capability);assert(fresh.context!=Fake::copied.context);assert(driver.quiesce());
 }
 std::printf("Entropy actual provider: %s PASS\n",argv[1]);
}
