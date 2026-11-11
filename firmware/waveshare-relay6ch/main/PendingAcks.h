#pragma once
#include <atomic>
#include <stdint.h>
namespace BiomeRelay {
// The receive callback only publishes the newest sequence. Network writes are
// drained by the cooperative owner, never from the WebSocket receive task.
struct PendingAcks {
 std::atomic<uint64_t> received{0};
 uint64_t sentReceived=0,sentApplied=0;
 void accepted(uint64_t sequence){received.store(sequence,std::memory_order_release);}
 template<class Sender> bool drain(uint64_t applied,Sender send){
  const uint64_t newest=received.load(std::memory_order_acquire);
  if(newest>sentReceived){if(!send(false,newest))return false;sentReceived=newest;}
  if(applied>sentApplied){if(!send(true,applied))return false;sentApplied=applied;}
  return true;
 }
};
}
