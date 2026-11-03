#pragma once
#include "runtime/packages/PackageOrdinaryManifest.h"
#include "runtime/packages/PackageRteZip.h"
#include <RiscStoragePackageAsyncV1.h>
#include <cstdio>
#include <cstring>
namespace DriverManager {
/* One declared file into an already journaled, exclusively owned stage.
 * Internal coordinator component, NOT an exported provider API.
 * Preconditions: coordinator holds the actual native driver reservation and
 * copied volume session, has verified stage ownership/inventory, and has
 * persisted intent. It retains both through this component and session drain.
 * This class never publishes, deletes, activates, grants or releases custody.
 * Failed/uncertain append is not replayed: caller reconciles retained stage. */
class StageFileTransfer {
 public:
  enum class State{Idle,Working,Ready,Failed,Retained};
  bool begin(const risc_storage_volume_api_v1*v,uint64_t session,
    const char*source,uint64_t sourceOffset,const char*id,
    const RuntimePackages::OrdinaryEntry&entry,bool archive,uint32_t crc){
   if((state_!=State::Idle&&state_!=State::Ready)||operation_||!session||
      !risc_storage_volume_range(v)||!RuntimePackages::safeId(id)||!source||
      !std::memchr(source,0,sizeof(source_))||!std::memchr(entry.name,0,sizeof(entry.name))||!std::memchr(entry.sha256,0,sizeof(entry.sha256))||entry.sizeBytes>(archive?RuntimePackages::kRteZipMaxEntryBytes:RuntimePackages::kOrdinaryMaxEntryBytes)||
      !RuntimePackages::RteZipDetail::safeRelativePath(entry.name,std::strlen(entry.name))||
      !digest(entry.sha256,expected_))return false;
   if(std::strcmp(source,"/Drivers/.driver-manager.part")&&
    (std::strncmp(source,"/Packages/Inbox/",16)||!RuntimePackages::RteZipDetail::safeRelativePath(source+16,std::strlen(source+16))))return false;
   const int n=std::snprintf(destination_,sizeof(destination_),"/Drivers/.%s.pkg-stage/%s",id,entry.name);
   if(n<=0||size_t(n)>=sizeof(destination_))return false;
   std::strcpy(source_,source);v_=v;a_=risc_storage_volume_async(v);p_=risc_storage_volume_packages(v);
   session_=session;operation_=0;offset_=0;sourceOffset_=sourceOffset;size_=entry.sizeBytes;
   archive_=archive;crcExpected_=crc;crc_=0xffffffffu;executable_=entry.executable;
   chunk_=0;state_=State::Working;phase_=size_?Phase::Read:Phase::EmptyOpen;return true;
  }
  State state()const{return state_;}
  uint64_t copied()const{return offset_;}
  State step(){
   if(state_!=State::Working)return state_;
   if(!operation_){
    request_={};
    switch(phase_){
     case Phase::Read:
      chunk_=uint32_t(size_-offset_);if(chunk_>sizeof(bytes_))chunk_=sizeof(bytes_);
      if(sourceOffset_>UINT64_MAX-offset_||!risc_storage_package_read_range(&request_,source_,sourceOffset_+offset_,chunk_))return fail(false);
      break;
     case Phase::Append:
      if(!risc_storage_package_append_at(&request_,destination_,offset_,bytes_,chunk_))return fail(false);break;
     case Phase::EmptyOpen:request_.operation=RISC_STORAGE_PACKAGE_OPEN_WRITE;std::strcpy(request_.path,destination_);break;
     case Phase::EmptyClose:request_.operation=RISC_STORAGE_PACKAGE_CLOSE_WRITE;request_.size=0;break;
     case Phase::Validate:
      request_.operation=RISC_STORAGE_PACKAGE_VALIDATE_DATA;std::strcpy(request_.path,destination_);
      request_.size=uint32_t(size_);std::memcpy(request_.sha256,expected_,32);break;
     case Phase::ValidateDriver:
      request_.operation=RISC_STORAGE_PACKAGE_VALIDATE_DRIVER;std::strcpy(request_.path,destination_);
      request_.size=uint32_t(size_);std::memcpy(request_.sha256,expected_,32);break;
    }
    const int rc=p_->submit_package(v_->context,session_,&request_,&operation_);
    if(rc==RISC_STORAGE_ASYNC_BUSY)return state_;
    if(rc!=RISC_STORAGE_ASYNC_OK)return fail(rc==RISC_STORAGE_ASYNC_RETAINED);
    return state_;
   }
   const int rc=a_->collect(v_->context,session_,operation_,&result_);
   if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return state_;
   if(rc!=RISC_STORAGE_ASYNC_OK)return fail(rc==RISC_STORAGE_ASYNC_RETAINED);
   operation_=0;
   switch(phase_){
    case Phase::Read:
     if(result_.size!=chunk_)return fail(false);
     std::memcpy(bytes_,result_.data,chunk_);
     if(archive_)crc_=RuntimePackages::RteZipInstallDetail::updateCrc(crc_,bytes_,chunk_);
     phase_=Phase::Append;break;
    case Phase::Append:
     offset_+=chunk_;
     if(offset_<size_)phase_=Phase::Read;
     else {if(archive_&&(crc_^0xffffffffu)!=crcExpected_)return fail(false);phase_=Phase::Validate;}
     break;
    case Phase::EmptyOpen:phase_=Phase::EmptyClose;break;
    case Phase::EmptyClose:
     if(archive_&&crcExpected_!=0)return fail(false);
     phase_=Phase::Validate;break;
    case Phase::Validate:
     if(executable_)phase_=Phase::ValidateDriver;else state_=State::Ready;break;
    case Phase::ValidateDriver:state_=State::Ready;break;
   }
   return state_;
  }
  /* No local cancel: coordinator calls native end(session) and must collect
   * it. Abandoning this object alone never proves custody ended. */
 private:
  enum class Phase{Read,Append,EmptyOpen,EmptyClose,Validate,ValidateDriver};
  State state_=State::Idle;Phase phase_=Phase::Read;
  const risc_storage_volume_api_v1*v_=nullptr;
  const risc_storage_volume_api_v1_async*a_=nullptr;
  const risc_storage_volume_api_v1_packages*p_=nullptr;
  uint64_t session_=0,operation_=0,offset_=0,sourceOffset_=0,size_=0;
  bool archive_=false,executable_=false;uint32_t crc_=0,crcExpected_=0,chunk_=0;
  char source_[205]{},destination_[205]{};uint8_t expected_[32]{},bytes_[4096]{};
  risc_storage_package_request_v1 request_{};risc_storage_async_result_v1 result_{};
  State fail(bool retained){state_=retained?State::Retained:State::Failed;return state_;}
  static bool digest(const char*s,uint8_t*out){
   if(!s||std::strlen(s)!=64)return false;
   for(unsigned i=0;i<32;++i){unsigned value=0;for(unsigned j=0;j<2;++j){
    const char c=s[2*i+j];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;
    if(n<0)return false;value=(value<<4)|unsigned(n);
   }out[i]=uint8_t(value);}return true;
  }
};
}
