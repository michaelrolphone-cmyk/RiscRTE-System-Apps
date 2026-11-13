#include "runtime/drivers/NativeProviderPolicyValidationV1.h"
#include "runtime/packages/ElfAdmission.h"
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
#include "private/elf_types.h"
#include "RiscBankStoreV1.h"
extern "C" bool esp_elf_validate_file(const uint8_t*,size_t);
namespace CohortElf {
static uint32_t ticks=0;
static bool safe=true;static const char* missing=nullptr;
static bool operationSafe(){return safe;}
static uint32_t millis(){return ticks;}
static void vTaskDelay(unsigned n){ticks+=n;}
static uintptr_t elf_find_sym_default(const char* name){
 if(missing && !strcmp(missing,name))return 0;
 static const char* exports[]={"__divdf3","__errno","__fixunsdfsi","__floatunsidf","__getreent","__gtdf2","__ltdf2","_ctype_","calloc","clock_gettime","free","getopt_long","isalpha","longjmp","malloc","memchr","memcmp","memcpy","memmove","memset","optarg","opterr","optind","optopt","qsort","realloc","risc_runtime_get_api","setjmp","snprintf","strcasecmp","strcat","strchr","strcmp","strcpy","strcspn","strerror","strftime","strlen","strncasecmp","strncat","strncmp","strncpy","strnlen","strrchr","strstr","strtod","strtof","strtol","strtoul","tolower","vsnprintf"};
 for(const char* symbol:exports)if(!strcmp(name,symbol))return 1;return 0;
}
bool allowedImport(const char*name){return RiscPackages::ordinaryImport(name);}
enum class ElfRole {Application,Driver,Either};
bool admitElf(const uint8_t*bytes,size_t n,ElfRole role=ElfRole::Application,
              const RuntimeProviders::NativeProviderPolicyV1*policy=nullptr){
 if(!operationSafe()||n>RISC_BANK_APP_MAX)return false;
 if(role!=ElfRole::Driver)policy=nullptr;
 if(policy&&!RuntimeProviders::nativeProviderImageValid(*policy,bytes,n))return false;
 return RiscPackages::admitElf(bytes,n,role,policy!=nullptr,{esp_elf_validate_file,
   [](const char*name){return allowedImport(name)&&elf_find_sym_default(name);},
   operationSafe,[](){vTaskDelay(1);},[]()->uint32_t{return millis();}});
}
#ifdef STORE_ADMISSION_ENCODED_IMAGES
struct ReadContext {
 RiscBoot::Runtime* candidate;
 unsigned count=0,encodedCount=0;
 uint32_t started=ticks,last=ticks;
 size_t work=0;
 RiscBoot::RuntimeFileImage retained;
};
static bool file(void* raw,const char* path,bool provider) {
 auto& c=*static_cast<ReadContext*>(raw);
 if(!operationSafe()||c.retained.retained)return false;
 auto source=c.candidate->imageSource();
 RiscBoot::RuntimeFileImage image;
 const bool read=image.read(source,path,{
  [](size_t n)->void*{return std::malloc(n);},[](void*p){std::free(p);},
  [](void*p)->bool{auto&c=*static_cast<ReadContext*>(p);return operationSafe()&&!c.candidate->imageSource().retained();},
  [](void*p,size_t n)->bool{
   auto&c=*static_cast<ReadContext*>(p);if(uint32_t(ticks-c.started)>=30000u)return false;
   c.work+=n;if(c.work>=32768||uint32_t(ticks-c.last)>=2){vTaskDelay(1);c.work=0;c.last=ticks;}
   return uint32_t(ticks-c.started)<30000u;
  },&c},0,RISC_BANK_APP_MAX,!provider&&c.candidate->ordinaryImageEnvelopeAllowed(path));
 if(image.retained){c.retained=image;return false;}
 if(!read)return false;
 const bool ok=admitElf(image.bytes,image.size,provider?ElfRole::Driver:ElfRole::Application);
 if(!operationSafe()||source.retained()){image.retained=true;c.retained=image;return false;}
 if(ok){++c.count;if(image.encoded)++c.encodedCount;}
 std::free(image.bytes);return ok;
}
#else
static bool file(void* context,const char* path,bool provider){
 std::ifstream input(path,std::ios::binary);if(!input)return false;
 std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input),{});
 if(!admitElf(bytes.data(),bytes.size(),provider?ElfRole::Driver:ElfRole::Application))return false;
 ++*static_cast<unsigned*>(context);return true;
}
#endif
}
