#include "bootstrap/Runtime.h"
#include <cassert>
#include <cstdio>
using namespace RiscBoot;
static int32_t get(void*,uint32_t,const char*,void*,uint32_t,uint32_t*){assert(false);return 0;}
static int32_t put(void*,uint32_t,const char*,const void*,uint32_t){assert(false);return 0;}
int main(int argc,char **argv){assert(argc==2);KeyValueBackend kv{nullptr,get,put};
 Runtime runtime({[](){return true;},[](risc_runtime_health_v1*){return true;},[](uint32_t){assert(false);},[](const char*){return true;},nullptr,&kv});
 if(!runtime.prepare(argv[1])){fprintf(stderr,"prepare failed: %s\n",runtime.error());return 1;}
 puts("Actual Runtime::prepare accepted generated app manifest and explicit grants; no run/device/network calls");
}
