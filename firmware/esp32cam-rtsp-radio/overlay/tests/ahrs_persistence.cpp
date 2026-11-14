#include "ahrs_persistence_model.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
using namespace ahrs_persistence_model;
static Matrix rz(double angle){Matrix r;r.v[0]=r.v[4]=std::cos(angle);r.v[1]=-std::sin(angle);r.v[3]=std::sin(angle);return r;}
static Matrix rx(double angle){Matrix r;r.v[4]=r.v[8]=std::cos(angle);r.v[5]=-std::sin(angle);r.v[7]=std::sin(angle);return r;}
static void near(const Matrix &a,const Matrix &b){for(int i=0;i<9;++i)assert(std::fabs(a.v[i]-b.v[i])<1e-6);}
static Calibration calibration(){Calibration c;c.flags=GYRO_VALID|WORLD_VALID;c.gyroBias=Vec3(.01,.02,.03);c.northAccel=Vec3(0,-9.6,-2);c.northRawMag=Vec3(-45,80,160);c.worldImu=rx(-PI_VALUE/2);std::strcpy(c.worldId,"survey");std::strcpy(c.referenceId,"immutable-original-north");c.checksum=crc(c);assert(intact(c));return c;}
int main(){
 const Calibration saved=calibration();Calibration loaded;std::memcpy(&loaded,&saved,sizeof(saved));assert(intact(loaded));
 loaded.gyroBias.x+=.1;assert(!intact(loaded));loaded=saved;loaded.version=2;assert(!intact(loaded));loaded=saved;loaded.worldImu.v[0]=-1;assert(!usable(loaded));loaded=saved;loaded.magCorrection.v[0]=std::numeric_limits<double>::quiet_NaN();assert(!usable(loaded));
 const auto c=saved;const Vec3 gravity=apply(c.worldImu,c.northAccel),field=apply(c.worldImu,magneticImuAxes(c.northRawMag));
 for(const auto &world:{c.worldImu,multiply(rz(PI_VALUE),c.worldImu),multiply(rz(PI_VALUE/2),c.worldImu),multiply(rx(.45),multiply(rz(-1),c.worldImu))}){
  const Vec3 accel=apply(transpose(world),gravity),magImu=apply(transpose(world),field),raw=magneticImuAxes(magImu);
  Attitude a;assert(a.update(accel,Vec3(),20));WorldHeading heading;
  assert(heading.observe(a,raw,c,20));near(heading.orientation(a),world);
  // A second boot starts the relative yaw at zero and recovers from measurements.
  Attitude rebooted;assert(rebooted.update(accel,Vec3(),100));WorldHeading recovery;assert(recovery.observe(rebooted,raw,loaded=saved,100));near(recovery.orientation(rebooted),world);
  a.reset(ResetReason::Reconnected);assert(!heading.valid(a));assert(a.update(accel,Vec3(),40));assert(!heading.valid(a));assert(heading.observe(a,raw,c,40));near(heading.orientation(a),world);
  assert(!heading.observe(a,scale(raw,4),c,60));assert(heading.valid(a));
 }
 // A full uploaded soft-iron matrix and hard-iron bias are applied before heading.
 auto corrected=saved;corrected.magBias=Vec3(20,-10,30);corrected.magCorrection.v[0]=2;corrected.magCorrection.v[4]=.5;corrected.magCorrection.v[8]=1.5;corrected.flags|=MAG_CALIBRATED;corrected.checksum=crc(corrected);assert(intact(corrected));
 Attitude a;assert(a.update(corrected.northAccel,Vec3(),20));WorldHeading h;assert(h.observe(a,corrected.northRawMag,corrected,20));near(h.orientation(a),corrected.worldImu);
 auto absent=saved;absent.flags=0;WorldHeading empty;assert(!empty.observe(a,saved.northRawMag,absent,20));
 std::puts("AHRS persistence integrity, magnetic axis mapping, full 3D pose and reboot recovery passed");
}
