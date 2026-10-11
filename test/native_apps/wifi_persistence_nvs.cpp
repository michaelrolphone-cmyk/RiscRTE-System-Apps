/* Production RiscNvs adapter over a disk-backed synthetic NVS boundary.
 * Not an ESP flash emulator. Disk image lets separate processes model cold
 * startup without retaining app globals, grants, provider state or handles. */
#include "ports/esp32s3/NvsKeyValue.h"
#include <cassert>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using Bytes=std::vector<uint8_t>;
static std::map<std::string,Bytes> committed,pending;
static std::map<nvs_handle_t,nvs_open_mode_t> handles;
static std::string path;
static unsigned writes;
static int fault;
namespace RiscCpu {bool nativeRadioResourceTry(){return true;}void nativeRadioResourceEnd(){}}
namespace RiscNvs {esp_err_t initializationStatus(){return ESP_OK;}}
static void save(){
 std::ofstream f(path,std::ios::binary|std::ios::trunc);assert(f);
 uint32_t count=committed.size();f.write((char*)&count,4);
 for(auto &v:committed){uint32_t n=v.first.size(),s=v.second.size();f.write((char*)&n,4);f.write(v.first.data(),n);f.write((char*)&s,4);f.write((char*)v.second.data(),s);}f.flush();assert(f);
}
extern "C" void persistence_open(const char*p){
 path=p;std::ifstream f(path,std::ios::binary);if(!f)return;
 uint32_t count=0;f.read((char*)&count,4);assert(count<1000);
 while(count--){uint32_t n=0,s=0;f.read((char*)&n,4);assert(n<16);std::string key(n,' ');f.read(&key[0],n);f.read((char*)&s,4);assert(s<=64);Bytes b(s);f.read((char*)b.data(),s);assert(f);committed[key]=b;}
}
extern "C" void persistence_fault(int m){fault=m;}
extern "C" unsigned persistence_writes(){return writes;}
extern "C" int32_t persistence_get(void*,const char*k,void*b,uint32_t c,uint32_t*s){return RiscNvs::get(nullptr,6,k,b,c,s);}
extern "C" int32_t persistence_put(void*,const char*k,const void*b,uint32_t s){assert(s<=64);return RiscNvs::put(nullptr,6,k,b,s);}
esp_err_t nvs_open(const char*n,nvs_open_mode_t mode,nvs_handle_t*out){assert(!strcmp(n,"rte00000006"));static uint32_t id=0;*out=++id;handles[*out]=mode;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(handles.erase(h)==1);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char*k,void*b,size_t*s){assert(handles.count(h));auto it=committed.find(k);if(it==committed.end())return ESP_ERR_NVS_NOT_FOUND;if(!b){*s=it->second.size();return ESP_OK;}if(*s<it->second.size())return ESP_ERR_NVS_INVALID_LENGTH;memcpy(b,it->second.data(),it->second.size());*s=it->second.size();return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char*k,const void*b,size_t s){assert(handles.at(h)==NVS_READWRITE);++writes;if(fault)return ESP_FAIL;pending[k]=Bytes((const uint8_t*)b,(const uint8_t*)b+s);return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(handles.at(h)==NVS_READWRITE);for(auto&v:pending)committed[v.first]=v.second;pending.clear();save();return ESP_OK;}
