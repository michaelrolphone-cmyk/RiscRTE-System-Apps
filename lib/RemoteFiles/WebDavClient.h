#pragma once
#include <RiscTcpConnectionV1.h>
#include <RiscBluetoothSessionSetupV1.h>
#include <cstddef>
#include <cstdint>
namespace RiscWebDav {
/* App-owned asynchronous transport. One bounded socket operation per tick;
 * no worker, nested input loop, credential persistence or automatic write retry. */
class Client final {
public:
 enum class State { Off, ConnectWait, Connect, Connecting, Header, UploadRead, UploadSend, Reply, Body, Closing, Ready, Error, Retained };
 enum Capability : uint32_t { List=1u<<0, Read=1u<<1, Head=1u<<2, Put=1u<<3, Delete=1u<<4, Mkcol=1u<<5, Move=1u<<6, Copy=1u<<7 };
 enum Error { None, Invalid, Transport, Authentication, Expired, Cancelled, Protocol, Storage, Uncertain };
 enum class PutMode { CreateOnly, ConfirmedReplacement };
 struct Io { void* context=nullptr; bool (*owner)(void*)=nullptr;
  bool (*source)(void*,void*,size_t,size_t*)=nullptr;
  bool (*sink)(void*,const void*,size_t)=nullptr; size_t responseChunk=0; };
 bool begin(const risc_tcp_connection_v1*,const risc_bluetooth_session_descriptor_v1&,
            const uint8_t entropy[16],const Io&,uint64_t now);
 bool request(const char* method,const char* path,const char* destination,uint64_t size,const Io&,uint64_t now,const char* match=nullptr,size_t readLimit=0,PutMode putMode=PutMode::CreateOnly);
 const char* etag()const{return etag_;}
 State tick(uint64_t now);
 void cancel();
 void cancelRequest(); // Close this request without discarding the authenticated session.
 uint32_t capabilities()const{return capabilities_;}
 bool end(); // false while close is pending or custody retained
 State state()const{return state_;} Error error()const{return error_;}
 bool ready()const{return state_==State::Ready&&authenticated_;}
 unsigned status()const{return status_;}
 uint64_t transferred()const{return received_;} uint64_t total()const{return responseSize_;}
 uint64_t sent()const{return sent_;} uint64_t expires()const{return expiry_;}
 bool uncertain()const{return uncertain_;}
 static bool encodePath(const char*,char*,size_t);
private:
 State state_=State::Off; Error error_=None; Io io_{}; risc_tcp_connection_v1 tcp_{};
 risc_tcp_connection_listen_v1 endpoint_{};uint64_t handle_=0,expiry_=0,lastNow_=0,deadline_=0,retryAt_=0;
 uint64_t uploadSize_=0,sent_=0,responseSize_=0,received_=0;uint32_t nonceCount_=0;
 uint32_t capabilities_=0,pendingCapabilities_=0;size_t readLimit_=0;
 unsigned status_=0;bool handshake_=false,authenticated_=false,mutating_=false,uncertain_=false,ending_=false,retryConnect_=false;
 char host_[64]{},username_[65]{},password_[65]{},realm_[65]{},nonce_[65]{},cnonce_[33]{},ha1_[65]{};
 char etag_[96]{},match_[96]{};
 PutMode putMode_=PutMode::CreateOnly;
 char method_[12]{},path_[1537]{},destination_[1537]{},header_[4096]{},reply_[4096]{};
 uint8_t buffer_[2048]{};size_t headerSize_=0,headerAt_=0,replySize_=0,bufferSize_=0,bufferAt_=0;
 bool live(uint64_t); void fail(Error); bool result(int32_t); bool buildHeader(); bool parseReply();
 bool challenge(const char*); bool consume(const void*,size_t); void wipe();
};
}
