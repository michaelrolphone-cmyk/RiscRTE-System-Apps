#include "../include/ahrs_telemetry_model.h"
#include <cassert>
#include <cmath>
using namespace ahrs_telemetry_model;
int main(){
 Attitude a;assert(a.update(Vec3(0,0,GRAVITY),Vec3(0,0,0),100));
 for(unsigned ms=110;ms<=1100;ms+=10)assert(a.update(Vec3(0,0,GRAVITY),Vec3(0,0,PI_VALUE/2),ms));
 const auto q=a.quaternion();const auto generation=a.generation(),origin=a.originMs();assert(std::abs(a.anglesDeg().z)>80);
 for(int i=0;i<20;++i){a.reset(ResetReason::Calibration);assert(a.valid());assert(a.generation()==generation&&a.originMs()==origin);const auto p=a.quaternion();assert(p.w==q.w&&p.x==q.x&&p.y==q.y&&p.z==q.z);}
 assert(a.update(Vec3(0,0,GRAVITY),Vec3(0,0,0),1110));assert(std::abs(a.anglesDeg().z)>80);
 Attitude fresh;fresh.reset(ResetReason::Calibration);assert(!fresh.valid());assert(fresh.update(Vec3(0,0,GRAVITY),Vec3(0,0,0),10));
 a.reset(ResetReason::InvalidSample);assert(!a.valid());
}
