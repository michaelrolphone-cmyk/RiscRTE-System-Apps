#pragma once
#include "WebDavCore.h"
namespace RiscWebDav {
/* A connection supplied by an admitted ordinary transport provider. No native
 * capability pointer or app-data grant is transferred into that provider. */
struct Connection {
 void* context=nullptr;
 uint64_t handle=0;
 int32_t (*read)(void*,uint64_t,void*,uint32_t,uint32_t*)=nullptr;
 int32_t (*write)(void*,uint64_t,const void*,uint32_t,uint32_t*)=nullptr;
 int32_t (*close)(void*,uint64_t)=nullptr;
};
enum class SessionState { Idle,Header,Body,Continue,Compute,ResponseHeader,ResponseBody,Closing,Retained,Done };
/* One finite HTTP/1.1 request, Connection:close, no pipelining. tick dispatches
 * at most one transport or storage operation. The caller checks ownership and
 * polls from its normal foreground loop; there is no task or callback thread.
 * The caller supplies a monotonic absolute deadline and retains all grants.
 * Stop and successful completion close the socket before releasing memory.
 * Any context/custody uncertainty becomes terminal, with no further I/O. */
class Session final {
 public:
 bool begin(const risc_app_data_export_v1*,const Hooks&,const Connection&,uint64_t now,uint64_t deadline);
 SessionState tick(uint64_t now);
 bool stop();
 SessionState state()const{return state_;}
 unsigned status()const{return status_;}
 private:
 const risc_app_data_export_v1* files_=nullptr;
 Hooks hooks_{};Connection connection_{};Transaction transaction_{};Request request_{};
 SessionState state_=SessionState::Idle;
 uint64_t deadline_=0;
 char header_[HeaderMax+1]{},reply_[768]{},challenge_[256]{};
 size_t headerSize_=0,bodySize_=0,sent_=0,replySize_=0;
 unsigned char* body_=nullptr;
 unsigned status_=0;
 bool transactionStarted_=false;
 bool valid();
 bool received(int32_t,uint32_t,uint32_t,bool reading);
 void error(unsigned,const char*);
 void startTransaction();
 void dispose();
};
}
