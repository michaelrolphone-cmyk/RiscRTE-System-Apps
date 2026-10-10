#pragma once
#include "WebDavSession.h"
#include "WebDavDigest.h"
#include <RiscTcpConnectionV1.h>
namespace RiscWebDav {
struct SharingHooks {
 void* context=nullptr;
 bool (*owner)(void*)=nullptr;
 void* (*allocate)(size_t)=nullptr;
 void (*deallocate)(void*)=nullptr;
};
enum class SharingState { Off,Starting,Listening,Serving,Stopping,Retained,Error };
struct SharingStatus {
 SharingState state=SharingState::Off;
 int32_t error=0;
 unsigned lastHttpStatus=0;
 uint32_t completedRequests=0;
 bool transportOpen=false; // True also when terminal custody is unknown.
};
/* Foreground-owned finite local sharing. The caller initializes DigestSession
 * with fresh secure nonce bytes and credentials and keeps it and both grants
 * alive. No Wi-Fi, credentials, ports, roots or expiry are inferred. begin only
 * prepares state; tick drives one bounded socket/storage action. A listener is
 * active only after the caller explicitly starts this controller. */
class Sharing final {
 public:
 bool begin(const risc_app_data_export_v1*,const risc_tcp_connection_v1*,DigestSession*,
            const SharingHooks&,const risc_tcp_connection_listen_v1&,uint64_t now,uint64_t expiry);
 SharingState tick(uint64_t now);
 void requestStop();
 SharingStatus status()const{return status_;}
 /* True while explicitly active, including no connected client. The app must
  * wire this to its shared active-work idle inhibitor and drain explicit stop
  * before manual sleep, radio changes, launch or grant release. */
 bool activeWork()const{return status_.state!=SharingState::Off&&status_.state!=SharingState::Error;}
 private:
 const risc_app_data_export_v1* files_=nullptr;
 risc_tcp_connection_v1 tcp_{};
 DigestSession* authentication_=nullptr;
 SharingHooks hooks_{};
 risc_tcp_connection_listen_v1 bind_{};
 Session session_{};SharingStatus status_{};
 uint64_t listener_=0,client_=0,now_=0,expiry_=0;
 bool sessionStarted_=false;
 char challenge_[DigestChallengeCapacity]{};
 bool valid();
 void retained(int32_t);
 void fail(int32_t);
 bool authorized(const Request&,AuthPhase,const void*,size_t);
};
}
