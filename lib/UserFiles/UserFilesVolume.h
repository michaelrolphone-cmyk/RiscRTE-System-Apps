#pragma once
#include <RiscAppDataV1.h>
#include <RiscStorageVolumeFsV1.h>
#include <cstddef>
#include <cstdint>
namespace RiscUserFiles {
/* Dedicated public document tree in one explicitly bound atomic AppData object.
 * Never enumerates namespaces or native paths. 32 entries, 191-byte paths and
 * 58,992 payload bytes are advertised engineering limits of this first format.
 * Missing object is an empty tree; malformed storage is never initialized over. */
class Volume final {
public:
 static constexpr size_t Capacity=65536,Entries=32,Path=192,Record=204,Header=16+Entries*Record,Payload=Capacity-Header;
 struct Hooks{void*context;bool(*owner)(void*);void*(*allocate)(size_t);void(*deallocate)(void*);};
 bool begin(const risc_app_data_v1*,const Hooks&);
 bool end();
 const risc_storage_volume_api_v1* api()const{return &api_.state.prepared.base.sleep.terminal.power.volume.base;}
 bool retained()const{return retained_;}
private:
 risc_storage_volume_api_v1_fs api_{};risc_app_data_v1 store_{};Hooks hooks_{};
 uint8_t *image_=nullptr,*stage_=nullptr;uint32_t bytes_=Header,stageBytes_=Header;
 uintptr_t context_=0;uint64_t revision_=0;bool active_=false,ready_=false,retained_=false;
 uint32_t reader_=0,writer_=0,directory_=0,readIndex_=0,writeIndex_=0,position_=0,writePosition_=0,dirPosition_=0;
 char directoryPath_[Path]{},error_[96]{};uint32_t handleError_=0;
 static Volume*slots_[4];static uintptr_t serial_;static uint32_t handles_;
 static Volume*get(void*);static bool valid(const char*);
 bool owner();bool load();bool save(const uint8_t*,uint32_t);bool fail(const char*);bool backend(int32_t);
 static uint32_t word(const uint8_t*);static void word(uint8_t*,uint32_t);
 static uint8_t* row(uint8_t*,unsigned);static const uint8_t*row(const uint8_t*,unsigned);
 int find(const uint8_t*,const char*)const;bool parent(const uint8_t*,const char*)const;bool validate(uint32_t)const;
 bool stat(const char*,uint64_t*,bool*);uint32_t openRead(const char*,uint64_t*);uint32_t openWrite(const char*);
 size_t read(uint32_t,void*,size_t);size_t write(uint32_t,const void*,size_t);bool close(uint32_t,bool);
 uint32_t dirOpen(const char*);bool dirNext(uint32_t,risc_storage_dirent_v1*);bool dirClose(uint32_t);
 bool mkdir(const char*);bool remove(const char*);bool rename(const char*,const char*,bool);
 bool idle()const{return !reader_&&!writer_&&!directory_;}
};
}
