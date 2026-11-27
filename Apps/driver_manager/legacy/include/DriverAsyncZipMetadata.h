#pragma once
#include "runtime/packages/PackageRteZip.h"
#include <cstring>
/* Incremental metadata-only adapter for the exact legacy stored-ZIP policy.
 * It performs no I/O. Each next() asks for one copied READ_RANGE operation.
 * accept() copies the completed result. Replaying the bounded pure parser over
 * cached header ranges preserves its validation logic without calling storage
 * from the UI task or retaining caller buffers. Payload CRC/SHA and actual
 * transaction verification are still REQUIRED; MetadataReady is not install
 * authority. One instance belongs to one immutable selected archive attempt. */
namespace DriverManager {
class AsyncZipMetadata {
 public:
  enum class State {Idle,NeedRead,MetadataReady,Invalid};
  struct Request {uint64_t offset;uint32_t size;uint64_t attempt;};
  bool begin(uint64_t size){
   reset();if(attempt_==UINT64_MAX){state_=State::Invalid;return false;}++attempt_;if(size<RuntimePackages::kRteZipEocdBytes||
    size>RuntimePackages::kRteZipMaxTotalBytes+65536u){state_=State::Invalid;return false;}
   length_=size;state_=State::NeedRead;return true;
  }
  void reset(){state_=State::Idle;length_=0;count_=used_=0;waiting_=false;request_={};zip_={};plan_={};policy_=RuntimePackages::RteZipResult::InvalidInput;}
  State state()const{return state_;}
  const RuntimePackages::RteZipView&zip()const{return zip_;}
  const RuntimePackages::OrdinaryPackagePlan&plan()const{return plan_;}
  RuntimePackages::RteZipResult policyResult()const{return policy_;}
  /* Repeated next while a request is outstanding returns the SAME request;
   * callers must keep their operation ID and must not resubmit it. */
  State next(Request*out){
   if(state_!=State::NeedRead)return state_;
   if(waiting_){if(out)*out=request_;return state_;}
   auto read=[this](uint64_t offset,uint8_t*data,size_t size){return readCached(offset,data,size);};
   policy_=RuntimePackages::inspectRteZip(read,length_,zip_);
   if(policy_==RuntimePackages::RteZipResult::Ready)
    policy_=RuntimePackages::planRteZip(read,zip_,manifest_,sizeof(manifest_),plan_);
   if(waiting_){if(out)*out=request_;return state_;}
   state_=policy_==RuntimePackages::RteZipResult::Ready?State::MetadataReady:State::Invalid;
   return state_;
  }
  bool accept(const Request&request,const uint8_t*data,size_t size,bool success){
   if(state_!=State::NeedRead||!waiting_||request.offset!=request_.offset||request.size!=request_.size||request.attempt!=request_.attempt)return false;
   if(!success||!data||size!=request_.size||count_>=MaxRanges||size>PoolBytes-used_){
    state_=State::Invalid;waiting_=false;return false;
   }
   ranges_[count_++]={request_.offset,request_.size,used_};
   std::memcpy(pool_+used_,data,size);used_+=uint32_t(size);waiting_=false;return true;
  }
 private:
  /* inspect: EOCD + 4 reads for each of at most17 entries; plan: manifest.
   * Pool bounds deliberately cover even distinct maximum-size name reads. */
  static constexpr uint32_t MaxRanges=2+4*RuntimePackages::kRteZipMaxEntries;
  static constexpr uint32_t PoolBytes=12288;
  struct Range{uint64_t offset;uint32_t size,at;};
  Range ranges_[MaxRanges]{};uint8_t pool_[PoolBytes]{},manifest_[4097]{};
  uint64_t length_=0,attempt_=0;uint32_t count_=0,used_=0;bool waiting_=false;
  Request request_{};State state_=State::Idle;
  RuntimePackages::RteZipView zip_{};
  RuntimePackages::OrdinaryPackagePlan plan_{};
  RuntimePackages::RteZipResult policy_=RuntimePackages::RteZipResult::InvalidInput;
  bool readCached(uint64_t offset,uint8_t*out,size_t size){
   if(!out||!size||size>4096||offset>length_||size>length_-offset)return false;
   for(uint32_t i=0;i<count_;++i)if(ranges_[i].offset==offset&&ranges_[i].size==size){
    std::memcpy(out,pool_+ranges_[i].at,size);return true;
   }
   if(waiting_||count_>=MaxRanges||size>PoolBytes-used_)return false;
   request_={offset,uint32_t(size),attempt_};waiting_=true;return false;
  }
};
}
