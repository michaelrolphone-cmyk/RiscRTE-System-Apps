#include "WebDavSession.h"
#include <cstring>
#include "WebDavBounds.h"
#include <cstdio>
#include <algorithm>
namespace RiscWebDav {
namespace {
// Shared native/ordinary TCP result contract; no native capability dependency.
constexpr int32_t Ok=0,Again=1,Eof=2,Context=-2,Retained=-6;
constexpr uint32_t IoMax=2048;
constexpr char ContinueReply[]="HTTP/1.1 100 Continue\r\n\r\n";
}
bool Session::valid(){
 if(state_==SessionState::Retained)return false;
 if(!hooks_.owner||!hooks_.owner(hooks_.context)){state_=SessionState::Retained;return false;}
 return true;
}
bool Session::begin(const risc_app_data_export_v1* files,const Hooks& hooks,const Connection& connection,uint64_t now,uint64_t deadline){
 if(state_!=SessionState::Idle&&state_!=SessionState::Done)return false;
 if(!files||!risc_app_data_export_catalog(&files->volume.terminal.power.volume.base)||!hooks.owner||!hooks.authorize||!hooks.authenticationChallenge||!hooks.allocate||!hooks.deallocate||!connection.handle||!connection.read||!connection.write||!connection.close||deadline<=now)return false;
 const size_t n=detail::boundedLength(hooks.authenticationChallenge,256);
 if(!n||n==256)return false;
 for(size_t i=0;i<n;++i)if((unsigned char)hooks.authenticationChallenge[i]<32||(unsigned char)hooks.authenticationChallenge[i]>=127)return false;
 std::memcpy(challenge_,hooks.authenticationChallenge,n+1);
 files_=files;hooks_=hooks;hooks_.authenticationChallenge=challenge_;connection_=connection;deadline_=deadline;
 headerSize_=bodySize_=sent_=replySize_=0;body_=nullptr;status_=0;transactionStarted_=false;
 request_={};state_=SessionState::Header;
 return valid();
}
bool Session::received(int32_t rc,uint32_t count,uint32_t capacity,bool reading){
 if(!valid())return false;
 if(rc==Context||rc==Retained){state_=SessionState::Retained;return false;}
 if(count>capacity||(rc!=Ok&&count)||(rc==Ok&&!count)||(!reading&&rc==Eof)){state_=SessionState::Retained;return false;}
 if(rc==Again)return false;
 if(rc!=Ok){state_=SessionState::Closing;return false;}
 return true;
}
void Session::error(unsigned status,const char* reason){
 if(!valid())return;
 status_=status;
 const int n=std::snprintf(reply_,sizeof(reply_),"HTTP/1.1 %u %s\r\nContent-Length: 0\r\nConnection: close\r\nCache-Control: no-store\r\n%s%s%s\r\n",status,reason,
 status==401?"WWW-Authenticate: ":"",status==401?hooks_.authenticationChallenge:"",status==401?"\r\n":"");
 if(n<0||size_t(n)>=sizeof(reply_)){state_=SessionState::Retained;return;}
 replySize_=n;sent_=0;state_=SessionState::ResponseHeader;
}
void Session::startTransaction(){
 if(!valid())return;
 transactionStarted_=transaction_.begin(files_,hooks_,request_,body_,bodySize_);
 if(transaction_.state()==State::Retained){state_=SessionState::Retained;return;}
 if(!transactionStarted_){error(500,"Internal Server Error");return;}
 state_=SessionState::Compute;
}
SessionState Session::tick(uint64_t now){
 if(state_==SessionState::Idle||state_==SessionState::Done||state_==SessionState::Retained)return state_;
 if(!valid())return state_;
 if(now>=deadline_&&state_!=SessionState::Closing){state_=SessionState::Closing;return state_;}
 if(state_==SessionState::Header){
  const uint32_t capacity=std::min<size_t>(IoMax,HeaderMax-headerSize_);
  if(!capacity){error(431,"Request Header Fields Too Large");return state_;}
  uint32_t n=0;const int rc=connection_.read(connection_.context,connection_.handle,header_+headerSize_,capacity,&n);
  if(!received(rc,n,capacity,true))return state_;
  headerSize_+=n;header_[headerSize_]=0;
  // Length-bounded delimiter scan; embedded NUL must reach the strict parser.
  size_t end=0;for(size_t i=3;i<headerSize_;++i)if(!std::memcmp(header_+i-3,"\r\n\r\n",4)){end=i+1;break;}
  if(!end)return state_;
  const unsigned status=parseRequest(header_,end,request_);
  if(status){error(status,status==413?"Content Too Large":status==415?"Unsupported Media Type":status==417?"Expectation Failed":"Bad Request");return state_;}
  if(!hooks_.authorize(hooks_.context,request_,AuthPhase::Headers,nullptr,0)){if(valid())error(401,"Unauthorized");return state_;}
  if(!valid())return state_;
  const size_t extra=headerSize_-end;
  if(extra>request_.contentLength){error(400,"Bad Request");return state_;}
  if(request_.contentLength){
   body_=static_cast<unsigned char*>(hooks_.allocate(request_.contentLength));
   if(!valid())return state_;
   if(!body_){error(503,"Service Unavailable");return state_;}
   if(extra)std::memcpy(body_,header_+end,extra);
  }
  bodySize_=extra;
  if(bodySize_==request_.contentLength){startTransaction();return state_;}
  if(request_.expectContinue){sent_=0;state_=SessionState::Continue;}else state_=SessionState::Body;
  return state_;
 }
 if(state_==SessionState::Continue){
  const uint32_t capacity=sizeof(ContinueReply)-1-sent_;uint32_t n=0;
  const int rc=connection_.write(connection_.context,connection_.handle,ContinueReply+sent_,capacity,&n);
  if(!received(rc,n,capacity,false))return state_;
  sent_+=n;if(sent_==sizeof(ContinueReply)-1){sent_=0;state_=SessionState::Body;}return state_;
 }
 if(state_==SessionState::Body){
  const uint32_t capacity=std::min<size_t>(IoMax,request_.contentLength-bodySize_);uint32_t n=0;
  const int rc=connection_.read(connection_.context,connection_.handle,body_+bodySize_,capacity,&n);
  if(!received(rc,n,capacity,true))return state_;
  bodySize_+=n;if(bodySize_==request_.contentLength)startTransaction();return state_;
 }
 if(state_==SessionState::Compute){
  auto state=transaction_.step();if(state==State::Retained){state_=SessionState::Retained;return state_;}
  if(state==State::Ready){status_=transaction_.status();sent_=0;state_=SessionState::ResponseHeader;}return state_;
 }
 if(state_==SessionState::ResponseHeader||state_==SessionState::ResponseBody){
  const bool head=state_==SessionState::ResponseHeader;
  const void* bytes=head?(transactionStarted_?transaction_.header():reply_):transaction_.body();
  const size_t size=head?(transactionStarted_?transaction_.headerSize():replySize_):transaction_.bodySize();
  if(sent_==size){sent_=0;state_=head&&transactionStarted_&&transaction_.bodySize()?SessionState::ResponseBody:SessionState::Closing;return state_;}
  if(!bytes||sent_>size){state_=SessionState::Retained;return state_;}
  const uint32_t capacity=std::min<size_t>(IoMax,size-sent_);uint32_t n=0;
  const int rc=connection_.write(connection_.context,connection_.handle,static_cast<const unsigned char*>(bytes)+sent_,capacity,&n);
  if(!received(rc,n,capacity,false))return state_;
  sent_+=n;return state_;
 }
 if(state_==SessionState::Closing){(void)stop();return state_;}
 return state_;
}
void Session::dispose(){
 if(transactionStarted_&&!transaction_.finish()){state_=SessionState::Retained;return;}
 if(!valid())return;
 if(body_){auto* released=body_;body_=nullptr;hooks_.deallocate(released);if(!valid())return;}
 body_=nullptr;files_=nullptr;connection_={};transactionStarted_=false;state_=SessionState::Done;
}
bool Session::stop(){
 if(state_==SessionState::Idle||state_==SessionState::Done)return true;
 if(!valid())return false;
 // No transport cleanup may follow the export's terminal fence.
 if(transactionStarted_&&transaction_.state()==State::Retained){state_=SessionState::Retained;return false;}
 if(connection_.handle){const int rc=connection_.close(connection_.context,connection_.handle);if(!valid()||rc!=Ok){state_=SessionState::Retained;return false;}connection_.handle=0;}
 dispose();return state_==SessionState::Done;
}
}
