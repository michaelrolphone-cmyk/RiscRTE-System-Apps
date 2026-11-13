#pragma once
#include "runtime/packages/PackageOrdinaryManifest.h"
#include <RiscStoragePackageAsyncV1.h>
#include <cstdio>
#include <cstring>
namespace DriverManager {
/* Exact inventory + hash verification inside an already owned copied session.
 * Only canonical ordinary driver roots are constructible. No mutations.
 * Coordinator retains the actual native gate during transaction verification.
 * A failure does not end/cancel/release the borrowed session. */
  class DirectoryVerify {
    public: enum class Root{
      Installed,Stage,Previous,Removing
    };
    enum class State{
      Idle,Working,Ready,Missing,Failed,Retained
    };
    bool begin(const risc_storage_volume_api_v1*v,uint64_t session,Root root, const char*id,const char*version=nullptr,bool nativeValidation=false){
      if((state_!=State::Idle&&state_!=State::Ready&&state_!=State::Missing)||operation_|| !session||!risc_storage_volume_range(v)||!RuntimePackages::safeId(id)|| (version&&!RuntimePackages::safeVersion(version)))return false;
      const char*suffix=root==Root::Stage?".pkg-stage":root==Root::Previous?".pkg-previous": root==Root::Removing?".pkg-removing":root==Root::Installed?"":nullptr;
      if(!suffix)return false;
      int n=std::snprintf(root_,sizeof(root_),"/Drivers/%s%s%s",root==Root::Installed?"":".",id,suffix);
      if(n<=0||size_t(n)>=sizeof(root_))return false;
      std::strcpy(id_,id);
      version_[0]=0;
      if(version)std::strcpy(version_,version);
      v_=v;
      a_=risc_storage_volume_async(v);
      p_=risc_storage_volume_packages(v);
      session_=session;
      native_=nativeValidation;
      operation_=0;
      directoryCount_=1;
      directory_=file_=0;
      std::memset(directories_,0,sizeof(directories_));
      std::memset(seenFiles_,0,sizeof(seenFiles_));
      std::memset(seenDirectories_,0,sizeof(seenDirectories_));
      manifestSeen_=false;
      manifest_[0]=0;
      RuntimePackages::clearOrdinaryManifestPlan(plan_);
      phase_=Phase::Manifest;
      state_=State::Working;
      return true;
    }
    State state()const{
      return state_;
    }
    const RuntimePackages::OrdinaryPackagePlan&plan()const{
      return plan_;
    }
    const char*manifest()const{
      return manifest_;
    }
    const char*root()const{
      return root_;
    }
    State step(){
      if(state_!=State::Working)return state_;
      if(!operation_){
        request_={
        };
        simple_={
        };
        bool package=false;
        switch(phase_){
          case Phase::Manifest:simple_.operation=RISC_STORAGE_ASYNC_READ_SMALL_FILE;
          if(!path(simple_.path,sizeof(simple_.path),".package.json"))return fail(false);
          break;
          case Phase::Open:simple_.operation=RISC_STORAGE_ASYNC_OPEN_DIRECTORY;
          if(!path(simple_.path,sizeof(simple_.path),directories_[directory_]))return fail(false);
          break;
          case Phase::Next:simple_.operation=RISC_STORAGE_ASYNC_NEXT_ENTRY;
          break;
          case Phase::Close:simple_.operation=RISC_STORAGE_ASYNC_CLOSE_DIRECTORY;
          break;
          case Phase::Hash:case Phase::Native:{
            package=true;
            const auto&e=plan_.entries[file_];
            request_.operation=phase_==Phase::Hash?RISC_STORAGE_PACKAGE_VALIDATE_DATA:RISC_STORAGE_PACKAGE_VALIDATE_DRIVER;
            if(!path(request_.path,sizeof(request_.path),e.name)||!digest(e.sha256,request_.sha256))return fail(false);
            request_.size=uint32_t(e.sizeBytes);
            break;
          }
        }
        int rc=package?p_->submit_package(v_->context,session_,&request_,&operation_): a_->submit(v_->context,session_,&simple_,&operation_);
        if(rc==RISC_STORAGE_ASYNC_BUSY)return state_;
        if(rc!=RISC_STORAGE_ASYNC_OK)return fail(rc==RISC_STORAGE_ASYNC_RETAINED);
        return state_;
      }
      int rc=a_->collect(v_->context,session_,operation_,&result_);
      if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return state_;
      if(rc==RISC_STORAGE_ASYNC_RETAINED)return fail(true);
      operation_=0;
      if(phase_==Phase::Manifest&&rc==RISC_STORAGE_ASYNC_NOT_FOUND){
        state_=State::Missing;
        return state_;
      }
      if(phase_==Phase::Next&&rc==RISC_STORAGE_ASYNC_END){
        phase_=Phase::Close;
        return state_;
      }
      if(rc!=RISC_STORAGE_ASYNC_OK)return fail(false);
      switch(phase_){
        case Phase::Manifest: if(!result_.size||result_.size>4096||std::memchr(result_.data,0,result_.size))return fail(false);
        std::memcpy(manifest_,result_.data,result_.size);
        manifest_[result_.size]=0;
        if(!RuntimePackages::parseOrdinaryManifest(manifest_,result_.size,plan_)|| plan_.identity.kind!=RuntimePackages::Kind::Driver||std::strcmp(plan_.identity.id,id_)|| (version_[0]&&std::strcmp(plan_.identity.version,version_))||!buildDirectories())return fail(false);
        phase_=Phase::Open;
        break;
        case Phase::Open:phase_=Phase::Next;
        break;
        case Phase::Next:if(!inventory(result_.entry))return fail(false);
        break;
        case Phase::Close: if(++directory_<directoryCount_)phase_=Phase::Open;
        else{
          if(!manifestSeen_)return fail(false);
          for(size_t i=0;i<plan_.entryCount;++i)if(!seenFiles_[i])return fail(false);
          for(uint32_t i=1;i<directoryCount_;++i)if(!seenDirectories_[i])return fail(false);
          file_=0;
          phase_=Phase::Hash;
        }
        break;
        case Phase::Hash: if(native_&&plan_.entries[file_].executable)phase_=Phase::Native;
        else nextFile();
        break;
        case Phase::Native:nextFile();
        break;
      }
      return state_;
    }
    private: static constexpr uint32_t MaxDirectories=1+RuntimePackages::kMaxPackageEntries*(RuntimePackages::kPackageResourceDepth-1);
    enum class Phase{
      Manifest,Open,Next,Close,Hash,Native
    };
    Phase phase_=Phase::Manifest;
    State state_=State::Idle;
    const risc_storage_volume_api_v1*v_=nullptr;
    const risc_storage_volume_api_v1_async*a_=nullptr;
    const risc_storage_volume_api_v1_packages*p_=nullptr;
    uint64_t session_=0,operation_=0;
    bool native_=false,manifestSeen_=false;
    uint32_t directoryCount_=0,directory_=0,file_=0;
    char id_[64]{
    },version_[32]{
    },root_[100]{
    },manifest_[4097]{
    };
    char directories_[MaxDirectories][128]{
    };
    bool seenDirectories_[MaxDirectories]{
    },seenFiles_[RuntimePackages::kMaxPackageEntries]{
    };
    RuntimePackages::OrdinaryPackagePlan plan_{
    };
    risc_storage_async_request_v1 simple_{
    };
    risc_storage_package_request_v1 request_{
    };
    risc_storage_async_result_v1 result_{
    };
    State fail(bool retained){
      state_=retained?State::Retained:State::Failed;
      return state_;
    }
    void nextFile(){
      if(++file_==plan_.entryCount)state_=State::Ready;
      else phase_=Phase::Hash;
    }
    bool path(char*out,size_t capacity,const char*relative)const{
      int n=relative[0]?std::snprintf(out,capacity,"%s/%s",root_,relative):std::snprintf(out,capacity,"%s",root_);
      return n>0&&size_t(n)<capacity;
    }
    bool buildDirectories(){
      for(size_t i=0;i<plan_.entryCount;++i){
        const char*name=plan_.entries[i].name;
        for(const char*s=name;*s;++s)if(*s=='/'){
          const size_t length=size_t(s-name);
          bool found=false;
          for(uint32_t j=0;j<directoryCount_;++j)if(std::strlen(directories_[j])==length&&!std::memcmp(directories_[j],name,length)){
            found=true;
            break;
          }
          if(found)continue;
          if(directoryCount_==MaxDirectories)return false;
          std::memcpy(directories_[directoryCount_],name,length);
          directories_[directoryCount_++][length]=0;
        }
      }
      return true;
    }
    bool inventory(const risc_storage_dirent_v1&e){
      if(!std::memchr(e.name,0,sizeof(e.name))||!e.name[0])return false;
      if(!std::strcmp(e.name,".")||!std::strcmp(e.name,".."))return e.is_directory!=0;
      if(std::strchr(e.name,'/')||std::strchr(e.name,'\\'))return false;
      char relative[128];
      int n=directories_[directory_][0]? std::snprintf(relative,sizeof(relative),"%s/%s",directories_[directory_],e.name): std::snprintf(relative,sizeof(relative),"%s",e.name);
      if(n<=0||size_t(n)>=sizeof(relative))return false;
      if(e.is_directory){
        for(uint32_t i=1;i<directoryCount_;++i)if(!std::strcmp(relative,directories_[i])){
          if(seenDirectories_[i])return false;
          seenDirectories_[i]=true;
          return true;
        }
        return false;
      }
      if(!std::strcmp(relative,".package.json")){
        if(manifestSeen_||e.size!=std::strlen(manifest_))return false;
        manifestSeen_=true;
        return true;
      }
      for(size_t i=0;i<plan_.entryCount;++i)if(!std::strcmp(relative,plan_.entries[i].name)){
        if(seenFiles_[i]||e.size!=plan_.entries[i].sizeBytes)return false;
        seenFiles_[i]=true;
        return true;
      }
      return false;
    }
    static bool digest(const char*s,uint8_t*out){
      if(!std::memchr(s,0,65)||std::strlen(s)!=64)return false;
      for(unsigned i=0;i<32;++i){
        unsigned v=0;
        for(unsigned j=0;j<2;++j){
          char c=s[2*i+j];
          int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;
          if(n<0)return false;
          v=(v<<4)|unsigned(n);
        }
        out[i]=uint8_t(v);
      }
      return true;
    }
  };
}
