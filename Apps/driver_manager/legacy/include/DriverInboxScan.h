#pragma once
#include "DriverManagerV1.h"
#include "DriverAsyncZipMetadata.h"
#include <RiscStoragePackageAsyncV1.h>
#include "runtime/update/Version.h"
#include <cstdio>
#include <cstring>
namespace DriverManager {
/* RAM snapshot builder. All media calls are copied native worker requests.
 * One session and operation ID are retained until checked end succeeds.
 * Metadata is observational: eligible is owner policy, never package authority.
 * Mutations must reread/revalidate the selected bytes, not reuse this preview. */
class InboxScan {
 public:
  static constexpr uint32_t Max=64;
  using Eligible=bool(*)(void*,const RuntimePackages::OrdinaryPackagePlan&);
  struct Item {driver_manager_row_v1 row;char source[128];bool archive;uint64_t bytes;};
  enum class State {Idle,Working,Complete,Failed,Retained};
  bool begin(const risc_storage_volume_api_v1*v,Eligible eligible,void*context){
   if(state_==State::Working||state_==State::Retained||session_)return false;
   a_=risc_storage_volume_async(v);v_=v;range_=risc_storage_volume_range(v)!=nullptr;
   if(!a_)return false;
   policy_=eligible;context_=context;count_=sources_=cursor_=0;seen_=skip_=0;directoryEnd_=false;operation_=session_=0;
   phase_=Phase::Begin;state_=State::Working;cancel_=false;return true;
  }
  State state()const{return state_;}
  uint32_t count()const{return count_;}
  const Item*get(uint32_t i)const{return i<count_?&items_[i]:nullptr;}
  /* Call until Complete/Failed. Retained pins the borrowed volume forever
   * until its native owner resolves custody; this class never fabricates it. */
  State cancel(){cancel_=true;return step();}
  State step(){
   if(state_!=State::Working)return state_;
   if(cancel_){phase_=Phase::End;success_=false;}
   if(phase_==Phase::Begin){
    int rc=a_->begin(v_->context,&session_);
    if(rc==RISC_STORAGE_ASYNC_BUSY)return state_;
    if(rc!=RISC_STORAGE_ASYNC_OK)return finish(rc);
    phase_=Phase::Refresh;return state_;
   }
   if(phase_==Phase::End){
    if(!session_){state_=success_?State::Complete:State::Failed;return state_;}
    int rc=a_->end(v_->context,session_);
    if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return state_;
    if(rc!=RISC_STORAGE_ASYNC_OK){state_=State::Retained;return state_;}
    session_=operation_=0;state_=success_?State::Complete:State::Failed;return state_;
   }
   if(phase_==Phase::Pick){
    if(cursor_>=sources_){if(directoryEnd_||count_==Max){success_=true;phase_=Phase::End;}else{sources_=cursor_=0;skip_=seen_;phase_=Phase::Open;}return state_;}
    auto&e=entries_[cursor_];plan_={};zip_.reset();
    if(e.is_directory){std::snprintf(path_,sizeof(path_),"/Packages/Inbox/%s/.package.json",e.name);phase_=Phase::Manifest;}
    else if(range_&&zip_.begin(e.size)){std::snprintf(path_,sizeof(path_),"/Packages/Inbox/%s",e.name);phase_=Phase::Zip;}
    else ++cursor_;
    return state_;
   }
   if(phase_==Phase::Zip){
    if(operation_){
     int rc=a_->collect(v_->context,session_,operation_,&result_);
     if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return state_;
     if(rc==RISC_STORAGE_ASYNC_RETAINED)return finish(rc);
     operation_=0;
     if(!zip_.accept(zipRequest_,result_.data,result_.size,rc==RISC_STORAGE_ASYNC_OK)){++cursor_;phase_=Phase::Pick;}
     return state_;
    }
    auto state=zip_.next(&zipRequest_);
    if(state==AsyncZipMetadata::State::MetadataReady){plan_=zip_.plan();return candidate();}
    if(state!=AsyncZipMetadata::State::NeedRead){++cursor_;phase_=Phase::Pick;return state_;}
    risc_storage_package_request_v1 q{};
    if(!risc_storage_package_read_range(&q,path_,zipRequest_.offset,zipRequest_.size)){++cursor_;phase_=Phase::Pick;return state_;}
    const auto*p=risc_storage_volume_packages(v_);int rc=p->submit_package(v_->context,session_,&q,&operation_);
    if(rc==RISC_STORAGE_ASYNC_BUSY)return state_;
    if(rc!=RISC_STORAGE_ASYNC_OK)return finish(rc);
    return state_;
   }
   risc_storage_async_request_v1 q{};
   switch(phase_){
    case Phase::Refresh:q.operation=RISC_STORAGE_ASYNC_REFRESH;break;
    case Phase::Open:q.operation=RISC_STORAGE_ASYNC_OPEN_DIRECTORY;std::strcpy(q.path,"/Packages/Inbox");break;
    case Phase::Next:q.operation=RISC_STORAGE_ASYNC_NEXT_ENTRY;break;
    case Phase::Close:q.operation=RISC_STORAGE_ASYNC_CLOSE_DIRECTORY;break;
    case Phase::Manifest:case Phase::Installed:q.operation=RISC_STORAGE_ASYNC_READ_SMALL_FILE;std::strcpy(q.path,path_);break;
    default:return finish(RISC_STORAGE_ASYNC_INVALID);
   }
   if(!operation_){
    int rc=a_->submit(v_->context,session_,&q,&operation_);
    if(rc==RISC_STORAGE_ASYNC_BUSY)return state_;
    if(rc!=RISC_STORAGE_ASYNC_OK)return finish(rc);
    return state_;
   }
   int rc=a_->collect(v_->context,session_,operation_,&result_);
   if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return state_;
   if(rc==RISC_STORAGE_ASYNC_RETAINED)return finish(rc);
   operation_=0;
   switch(phase_){
    case Phase::Refresh:if(rc!=RISC_STORAGE_ASYNC_OK)return finish(rc);phase_=Phase::Open;break;
    case Phase::Open:
     if(rc==RISC_STORAGE_ASYNC_NOT_FOUND){success_=true;phase_=Phase::End;break;}
     if(rc!=RISC_STORAGE_ASYNC_OK)return finish(rc);phase_=Phase::Next;break;
    case Phase::Next:
     if(rc==RISC_STORAGE_ASYNC_END){directoryEnd_=true;phase_=Phase::Close;break;}
     if(rc!=RISC_STORAGE_ASYNC_OK)return finish(rc);
     if(skip_){--skip_;break;}if(seen_==UINT32_MAX)return finish(RISC_STORAGE_ASYNC_INVALID);++seen_;
     if(safeName(result_.entry.name)&&(result_.entry.is_directory||zipName(result_.entry.name))){
      entries_[sources_++]=result_.entry;if(sources_==Max)phase_=Phase::Close;
     }break;
    case Phase::Close:if(rc!=RISC_STORAGE_ASYNC_OK)return finish(rc);phase_=Phase::Pick;break;
    case Phase::Manifest:
     if(rc==RISC_STORAGE_ASYNC_OK&&result_.size&&result_.size<=4096&&
       RuntimePackages::parseOrdinaryManifest(reinterpret_cast<const char*>(result_.data),result_.size,plan_)&&
       !std::strcmp(entries_[cursor_].name,plan_.identity.id))return candidate();
     ++cursor_;phase_=Phase::Pick;break;
    case Phase::Installed:publishPreview(rc);++cursor_;phase_=Phase::Pick;break;
    default:return finish(RISC_STORAGE_ASYNC_INVALID);
   }
   return state_;
  }
 private:
  enum class Phase{Begin,Refresh,Open,Next,Close,Pick,Manifest,Zip,Installed,End};
  State state_=State::Idle;Phase phase_=Phase::Begin;
  const risc_storage_volume_api_v1*v_=nullptr;const risc_storage_volume_api_v1_async*a_=nullptr;
  uint64_t session_=0,operation_=0;bool range_=false,cancel_=false,success_=false;
  Eligible policy_=nullptr;void*context_=nullptr;
  uint32_t count_=0,sources_=0,cursor_=0,seen_=0,skip_=0;bool directoryEnd_=false;
  risc_storage_dirent_v1 entries_[Max]{};Item items_[Max]{};
  risc_storage_async_result_v1 result_{};char path_[205]{};
  AsyncZipMetadata zip_;AsyncZipMetadata::Request zipRequest_{};
  RuntimePackages::OrdinaryPackagePlan plan_{},installed_{};
  static bool safeName(const char*s){
   if(!s||!std::memchr(s,0,128)||!s[0]||!std::strcmp(s,".")||!std::strcmp(s,".."))return false;
   for(const unsigned char*p=reinterpret_cast<const unsigned char*>(s);*p;++p)
    if(*p<32||*p>=127||*p=='/'||*p=='\\'||*p==':')return false;
   return true;
  }
  static bool zipName(const char*s){size_t n=std::strlen(s);return n>8&&!std::strcmp(s+n-8,".rte.zip");}
  State finish(int rc){
   if(rc==RISC_STORAGE_ASYNC_RETAINED){state_=State::Retained;return state_;}
   success_=false;phase_=Phase::End;return state_;
  }
  State candidate(){
   if(plan_.identity.kind!=RuntimePackages::Kind::Driver){++cursor_;phase_=Phase::Pick;return state_;}
   std::snprintf(path_,sizeof(path_),"/Drivers/%s/.package.json",plan_.identity.id);
   phase_=Phase::Installed;return state_;
  }
  void publishPreview(int rc){
   if(count_==Max)return;
   auto&out=items_[count_];out={};out.archive=!entries_[cursor_].is_directory;out.bytes=entries_[cursor_].size;
   std::strcpy(out.source,entries_[cursor_].name);std::strcpy(out.row.id,plan_.identity.id);
   std::strcpy(out.row.version,plan_.identity.version);std::strcpy(out.row.label,plan_.identity.id);
   /* Tokens are assigned by the owner together with the new generation. */
   bool valid=rc==RISC_STORAGE_ASYNC_NOT_FOUND;
   if(rc==RISC_STORAGE_ASYNC_OK&&result_.size&&result_.size<=4096&&
    RuntimePackages::parseOrdinaryManifest(reinterpret_cast<const char*>(result_.data),result_.size,installed_)&&
    installed_.identity.kind==RuntimePackages::Kind::Driver&&!std::strcmp(installed_.identity.id,plan_.identity.id)){
    valid=true;std::strcpy(out.row.installed_version,installed_.identity.version);
   }
   if(valid)out.row.flags=DRIVER_MANAGER_VALID;
   bool newer=true;
   if(out.row.installed_version[0]){
    uint32_t a[3],b[3];newer=RiscUpdate::parseVersion(out.row.version,a)&&RiscUpdate::parseVersion(out.row.installed_version,b)&&RiscUpdate::compareVersion(a,b)>0;
    out.row.flags|=DRIVER_MANAGER_CAN_UNINSTALL;
   }
   if(valid&&newer&&policy_&&policy_(context_,plan_))out.row.flags|=out.row.installed_version[0]?DRIVER_MANAGER_CAN_UPDATE:DRIVER_MANAGER_CAN_INSTALL;
   std::snprintf(out.row.detail,sizeof(out.row.detail),"%s",!valid?"Installed metadata needs recovery":
    (out.row.flags&(DRIVER_MANAGER_CAN_INSTALL|DRIVER_MANAGER_CAN_UPDATE))?"Metadata preview; content reverified on install":
    "Dependencies/version blocked; files retained");
   ++count_;
  }
};
}
