/* Actual Runtime pre-execution parser, linked from a supplied read-only checkout.
 * prepare() validates metadata only: no ELF, provider, credential or device I/O. */
#include "bootstrap/Runtime.h"
#include <cstdio>
#if __has_include("RuntimeFileFixture.h")
#include "RuntimeFileFixture.h"
#define WIFI_RUNTIME_FILE_SOURCES
#endif
using namespace RiscBoot;
static bool owner(){return true;}
static bool health(risc_runtime_health_v1*){return true;}
static void delay(uint32_t){}
static bool log(const char*){return true;}
static int32_t get(void*,uint32_t,const char*,void*,uint32_t,uint32_t*){return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t put(void*,uint32_t,const char*,const void*,uint32_t){return RISC_KEY_VALUE_IO;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 const KeyValueBackend kv{nullptr,get,put};
#ifdef WIFI_RUNTIME_FILE_SOURCES
 RiscTest::FileSources files({std::string(argv[1])});
#endif
 Runtime runtime({owner,health,delay,log,nullptr,&kv});
#ifdef WIFI_RUNTIME_FILE_SOURCES
 if(!files.bind(runtime))return 1;
#endif
 if(!runtime.prepare(argv[1])){fprintf(stderr,"%s\n",runtime.error());return 1;}
 puts("Production Runtime metadata admission passed; no modules executed");return 0;
}
