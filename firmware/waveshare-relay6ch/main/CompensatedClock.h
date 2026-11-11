#pragma once
#include <atomic>
#include <stdint.h>
namespace BiomeRelay {
// The first SNTP reply can only bootstrap a cold 1970 clock: lwIP skips
// round-trip correction for offsets greater than 34 years. The next reply
// uses four timestamps. This gate is process-wide and survives socket reattach.
class CompensatedClock {
 std::atomic<unsigned> count{0};
 public:
 unsigned synchronized(int64_t seconds){
  if(seconds<1704067200LL)return count.load();
  unsigned n=count.load();
  while(n<2&&!count.compare_exchange_weak(n,n+1)){}
  return count.load();
 }
 bool ready()const{return count.load()>=2;}
};
}
