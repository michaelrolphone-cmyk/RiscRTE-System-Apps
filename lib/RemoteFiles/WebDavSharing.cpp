#include "WebDavSharing.h"
namespace RiscWebDav {
bool Sharing::valid(){
 if(status_.state==SharingState::Retained)return false;
 if(!hooks_.owner||!hooks_.owner(hooks_.context)){retained(RISC_TCP_CONNECTION_CONTEXT);return false;}
 return true;
}
void Sharing::retained(int32_t error){status_.state=SharingState::Retained;status_.error=error;status_.transportOpen=true;}
void Sharing::fail(int32_t error){status_.error=error;status_.state=SharingState::Stopping;}
bool Sharing::begin(const risc_app_data_export_v1* files,const risc_tcp_connection_v1* tcp,DigestSession* authentication,
 const SharingHooks&hooks,const risc_tcp_connection_listen_v1&bind,uint64_t now,uint64_t expiry){
 if(status_.state!=SharingState::Off&&status_.state!=SharingState::Error)return false;
 if(listener_||client_||!files||!risc_app_data_export_catalog(&files->volume.terminal.power.volume.base)||!tcp||tcp->api_version!=1||tcp->struct_size<sizeof(*tcp)||!tcp->context||!tcp->listen||!tcp->accept||!tcp->read||!tcp->write||!tcp->close||!authentication||!authentication->active()||!hooks.owner||!hooks.allocate||!hooks.deallocate||bind.struct_size!=sizeof(bind)||!bind.port||bind.reserved||bind.address[0]>=224||expiry<=now||expiry==UINT64_MAX)return false;
 hooks_=hooks;status_={};status_.state=SharingState::Starting;
 if(!valid())return false;
 if(!authentication->challenge(challenge_,sizeof(challenge_),now)){status_.state=SharingState::Error;return false;}
 files_=files;tcp_=*tcp;authentication_=authentication;bind_=bind;now_=now;expiry_=expiry;sessionStarted_=false;return true;
}
bool Sharing::authorized(const Request&q,AuthPhase phase,const void*body,size_t size){
 if(!valid())return false;
 auto result=authentication_->verify(q.method,q.requestTarget,q.authorization,body,size,now_,phase==AuthPhase::Headers?DigestPhase::HeaderPrecheck:DigestPhase::Final);
 return result==DigestResult::Authorized||(phase==AuthPhase::Headers&&result==DigestResult::BodyRequired);
}
void Sharing::requestStop(){
 if(status_.state==SharingState::Off||status_.state==SharingState::Error||status_.state==SharingState::Retained)return;
 if(valid())status_.state=SharingState::Stopping;
}
SharingState Sharing::tick(uint64_t now){
 if(status_.state==SharingState::Off||status_.state==SharingState::Error||status_.state==SharingState::Retained)return status_.state;
 if(!valid())return status_.state;
 // Authentication can expire or be explicitly cleared before the listener
 // lifetime. Recheck before every socket/storage step, including staged writes.
 if(now<now_||now>=expiry_||!authentication_->challenge(challenge_,sizeof(challenge_),now))status_.state=SharingState::Stopping;
 now_=now;
 if(status_.state==SharingState::Starting){
  uint64_t handle=0;const int32_t rc=tcp_.listen(tcp_.context,&bind_,&handle);
  if(!valid())return status_.state;
  if(rc==RISC_TCP_CONNECTION_CONTEXT||rc==RISC_TCP_CONNECTION_RETAINED||(!rc&&!handle)||(rc&&handle)){retained(rc?rc:RISC_TCP_CONNECTION_RETAINED);return status_.state;}
  if(rc){fail(rc);return status_.state;}
  listener_=handle;status_.transportOpen=true;status_.state=SharingState::Listening;return status_.state;
 }
 if(status_.state==SharingState::Listening){
  uint64_t handle=0;const int32_t rc=tcp_.accept(tcp_.context,listener_,&handle);
  if(!valid())return status_.state;
  if(rc==RISC_TCP_CONNECTION_CONTEXT||rc==RISC_TCP_CONNECTION_RETAINED||(!rc&&!handle)||(rc&&handle)){retained(rc?rc:RISC_TCP_CONNECTION_RETAINED);return status_.state;}
  if(rc==RISC_TCP_CONNECTION_WOULD_BLOCK)return status_.state;
  if(rc){fail(rc);return status_.state;}
  client_=handle;
  Hooks requestHooks{this,[](void*c){return static_cast<Sharing*>(c)->valid();},
   [](void*c,const Request&q,AuthPhase p,const void*b,size_t n){return static_cast<Sharing*>(c)->authorized(q,p,b,n);},
   challenge_,hooks_.allocate,hooks_.deallocate};
  // The deadline is absolute and cannot overflow; each request gets at most
  // 60 seconds within the remaining explicitly chosen session lifetime.
  const uint64_t deadline=expiry_-now_>60000?now_+60000:expiry_;
  Connection connection{tcp_.context,client_,tcp_.read,tcp_.write,tcp_.close};
  sessionStarted_=session_.begin(files_,requestHooks,connection,now_,deadline);
  if(session_.state()==SessionState::Retained){retained(RISC_TCP_CONNECTION_RETAINED);return status_.state;}
  if(!sessionStarted_){fail(RISC_TCP_CONNECTION_INVALID);return status_.state;}
  status_.state=SharingState::Serving;return status_.state;
 }
 if(status_.state==SharingState::Serving){
  const auto state=session_.tick(now_);
  if(!valid())return status_.state;
  if(state==SessionState::Retained){retained(RISC_TCP_CONNECTION_RETAINED);return status_.state;}
  if(state==SessionState::Done){status_.lastHttpStatus=session_.status();++status_.completedRequests;client_=0;sessionStarted_=false;status_.state=SharingState::Listening;}
  return status_.state;
 }
 if(status_.state==SharingState::Stopping){
  if(client_){
   if(sessionStarted_){if(!session_.stop()){retained(RISC_TCP_CONNECTION_RETAINED);return status_.state;}}
   else {const int32_t rc=tcp_.close(tcp_.context,client_);if(!valid()||rc){retained(rc?rc:RISC_TCP_CONNECTION_CONTEXT);return status_.state;}}
   if(!valid())return status_.state;
   client_=0;sessionStarted_=false;return status_.state;
  }
  if(listener_){const int32_t rc=tcp_.close(tcp_.context,listener_);if(!valid()||rc){retained(rc?rc:RISC_TCP_CONNECTION_CONTEXT);return status_.state;}listener_=0;return status_.state;}
  if(authentication_)authentication_->clear();
  authentication_=nullptr;files_=nullptr;tcp_={};status_.transportOpen=false;
  status_.state=status_.error?SharingState::Error:SharingState::Off;return status_.state;
 }
 return status_.state;
}
}
