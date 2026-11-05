#pragma once
#include "WebDavCore.h"
#include "vendor/tinydtls_sha2/sha2.h"
#include <RiscStorageVolumeStateV1.h>
/* Ordinary capability consumer. No Runtime, board or native filesystem calls. */
namespace RiscWebDav {
class VolumeTransaction {
 public:
 bool begin(const risc_storage_volume_api_v1*,const Hooks&,const Request&,const void*,size_t,bool=false);
 State step();
 bool finish();
 bool nextBody();
 bool uploadReady()const{return state_==State::Prepare&&phase_==Phase::WriteData&&writer_&&upload_;}
 bool feedUpload(const void*,size_t);
 bool endUpload();
 bool bodyComplete()const{return !streaming_;}
 State state()const{return state_;}
 unsigned status()const{return status_;}
 const char* header()const{return header_;}
 size_t headerSize()const{return headerSize_;}
 const void* body()const{return output_;}
 size_t bodySize()const{return head_?0:outputSize_;}
 private:
 const risc_storage_volume_api_v1* api_=nullptr;
 const risc_storage_volume_api_v1_ext* ext_=nullptr;
 const risc_storage_volume_api_v1_state* observation_=nullptr;
 Hooks hooks_{};Request request_{};const void* input_=nullptr;size_t inputSize_=0;
 State state_=State::Idle;unsigned status_=0;
 enum class Phase { Stat,Destination,RemoveOriginal,ListOpen,ListNext,ListClose,ReadOpen,WriteOpen,WriteData,WriteSync,WriteClose,Commit,Remove,Mkdir,Move,CopyOpen,CopyRead,CopyWrite,WalkOpen,WalkSeek,WalkNext,WalkTell,WalkClose,WalkEntry,WalkRemove,WalkMkdir } phase_=Phase::Stat;
 char header_[768]{},temporary_[RISC_STORAGE_VOLUME_PATH_MAX+1]{};
 unsigned char* output_=nullptr;size_t outputSize_=0,outputCapacity_=0,headerSize_=0,offset_=0;
 unsigned char chunk_[RISC_STORAGE_VOLUME_IO_MAX]{};size_t chunkSize_=0;
 uint64_t size_=0,remaining_=0;risc_storage_file_t file_=0,writer_=0;risc_storage_dir_t directory_=0;
 struct WalkFrame {char source[RISC_STORAGE_VOLUME_PATH_MAX+1],destination[RISC_STORAGE_VOLUME_PATH_MAX+1];uint64_t position;};
 WalkFrame* walk_=nullptr;unsigned depth_=0;bool walkDelete_=false,walkEof_=false,walkDirectory_=false,walkInspect_=false,inspectDelete_=false;
 char workSource_[RISC_STORAGE_VOLUME_PATH_MAX+1]{},workDestination_[RISC_STORAGE_VOLUME_PATH_MAX+1]{};
 bool startWalk(bool);void popWalk();const char* sourcePath()const{return walk_?workSource_:request_.path;}const char* destinationPath()const{return walk_?workDestination_:request_.destination;}
 dtls_sha256_ctx entity_{};bool upload_=false;
 PropertyRequest properties_{};bool destinationExists_=false;
 bool head_=false,streaming_=false,exists_=false,isDirectory_=false,copy_=false,moveCopy_=false,temporaryOwned_=false;
 bool owner();int observed();bool safe();bool after();bool closeFile(risc_storage_file_t&,bool);bool closeDirectory();
 void response(unsigned,size_t=0,const char* ="application/octet-stream");void fail(unsigned);
 bool reserve(size_t);bool append(const char*);bool escaped(const char*,bool=false);bool row(const char*,bool,uint64_t);
 bool writablePath(const char*)const;bool supports(const char*)const;
};
}
