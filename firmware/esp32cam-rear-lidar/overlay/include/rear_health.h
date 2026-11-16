#pragma once
#include <stdint.h>
namespace rear_health {
inline bool due(uint32_t now,uint32_t next){return static_cast<int32_t>(now-next)>=0;}
inline bool fresh(uint64_t now,uint64_t sample){return sample && now>=sample && now-sample<=500000;}
inline bool linkFresh(uint64_t now,uint64_t rx){return rx && now>=rx && now-rx<=3000000;}
}
