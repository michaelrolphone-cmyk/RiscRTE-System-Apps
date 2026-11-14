#pragma once
#include "ahrs_telemetry_model.h"
#include <cstring>

namespace ahrs_persistence_model {
using namespace ahrs_telemetry_model;
struct Matrix {
    double v[9];
    Matrix():v{1,0,0,0,1,0,0,0,1} {}
};
inline Vec3 apply(const Matrix &m,const Vec3 &p) {
    return Vec3(m.v[0]*p.x+m.v[1]*p.y+m.v[2]*p.z,m.v[3]*p.x+m.v[4]*p.y+m.v[5]*p.z,m.v[6]*p.x+m.v[7]*p.y+m.v[8]*p.z);
}
inline Matrix transpose(const Matrix &m) { Matrix r;for(int i=0;i<3;++i)for(int j=0;j<3;++j)r.v[3*i+j]=m.v[3*j+i];return r; }
inline Matrix multiply(const Matrix &a,const Matrix &b) { Matrix r;for(int i=0;i<3;++i)for(int j=0;j<3;++j){r.v[3*i+j]=0;for(int k=0;k<3;++k)r.v[3*i+j]+=a.v[3*i+k]*b.v[3*k+j];}return r; }
inline double determinant(const Matrix &m) { const double *a=m.v;return a[0]*(a[4]*a[8]-a[5]*a[7])-a[1]*(a[3]*a[8]-a[5]*a[6])+a[2]*(a[3]*a[7]-a[4]*a[6]); }
inline bool finiteMatrix(const Matrix &m) { for(double x:m.v)if(!std::isfinite(x))return false;return true; }
inline bool rotation(const Matrix &m) { if(!finiteMatrix(m)||std::fabs(determinant(m)-1)>.002)return false;Matrix p=multiply(m,transpose(m));for(int i=0;i<9;++i)if(std::fabs(p.v[i]-(i%4==0?1:0))>.002)return false;return true; }
inline double dot(const Vec3 &a,const Vec3 &b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(const Vec3 &a,const Vec3 &b){return Vec3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
inline Vec3 scale(const Vec3 &a,double s){return Vec3(a.x*s,a.y*s,a.z*s);}
inline Matrix quaternionMatrix(const Quaternion &q){Matrix r;r.v[0]=1-2*(q.y*q.y+q.z*q.z);r.v[1]=2*(q.x*q.y-q.w*q.z);r.v[2]=2*(q.x*q.z+q.w*q.y);r.v[3]=2*(q.x*q.y+q.w*q.z);r.v[4]=1-2*(q.x*q.x+q.z*q.z);r.v[5]=2*(q.y*q.z-q.w*q.x);r.v[6]=2*(q.x*q.z-q.w*q.y);r.v[7]=2*(q.y*q.z+q.w*q.x);r.v[8]=1-2*(q.x*q.x+q.y*q.y);return r;}
// AK8963 die axes relative to the MPU9250 accelerometer/gyro die.
inline Vec3 magneticImuAxes(const Vec3 &v){return Vec3(v.y,v.x,-v.z);}
inline bool triad(const Vec3 &accel,const Vec3 &mag,Matrix &r){
    if(!finite(accel)||!finite(mag)||norm(accel)<1||norm(mag)<1)return false;
    const Vec3 up=scale(accel,1/norm(accel)),horizontal=subtract(mag,scale(up,dot(up,mag)));
    if(norm(horizontal)<norm(mag)*.05)return false;
    const Vec3 x=scale(horizontal,1/norm(horizontal)),y=cross(up,x);
    r.v[0]=x.x;r.v[1]=x.y;r.v[2]=x.z;r.v[3]=y.x;r.v[4]=y.y;r.v[5]=y.z;r.v[6]=up.x;r.v[7]=up.y;r.v[8]=up.z;return rotation(r);
}
struct Calibration {
    uint32_t magic=0x41524853,version=1,revision=0,flags=0;
    Vec3 gyroBias,magBias,northAccel,northRawMag;
    Matrix magCorrection,worldImu;
    char worldId[40]={},referenceId[64]={};
    uint32_t checksum=0;
};
constexpr uint32_t GYRO_VALID=1,WORLD_VALID=2,MAG_CALIBRATED=4;
inline uint32_t crc(const Calibration &c){const uint8_t *p=reinterpret_cast<const uint8_t *>(&c);uint32_t x=~0u;for(size_t i=0;i<offsetof(Calibration,checksum);++i){x^=p[i];for(int k=0;k<8;++k)x=(x>>1)^(0xedb88320u&-(x&1u));}return ~x;}
inline bool usable(const Calibration &c){
    if(c.magic!=0x41524853||c.version!=1||(c.flags&~7u)||!finite(c.gyroBias)||norm(c.gyroBias)>1||!finite(c.magBias)||!finiteMatrix(c.magCorrection)||determinant(c.magCorrection)<=0)return false;
    if(c.worldId[39]||c.referenceId[63]||norm(c.magBias)>10000)return false;
    for(double x:c.magCorrection.v)if(std::fabs(x)>100)return false;
    if(c.flags&WORLD_VALID){Matrix ref;if(!c.worldId[0]||!c.referenceId[0]||!rotation(c.worldImu)||!triad(c.northAccel,magneticImuAxes(apply(c.magCorrection,subtract(c.northRawMag,c.magBias))),ref))return false;}
    return true;
}
inline bool intact(const Calibration &c){return usable(c)&&crc(c)==c.checksum;}
class WorldHeading {
public:
    void reset(){ready_=false;generation_=0;}
    bool observe(const Attitude &a,const Vec3 &rawMag,const Calibration &c,uint32_t now){
        if(!(c.flags&WORLD_VALID)||!a.valid())return false;
        const Vec3 current=magneticImuAxes(apply(c.magCorrection,subtract(rawMag,c.magBias)));
        const Vec3 reference=magneticImuAxes(apply(c.magCorrection,subtract(c.northRawMag,c.magBias)));
        // Compare to the captured field, rather than assuming a particular room or location.
        if(!finite(current)||norm(reference)<1||norm(current)<.5*norm(reference)||norm(current)>1.5*norm(reference))return false;
        Matrix ref;if(!triad(c.northAccel,reference,ref))return false;
        const Vec3 field=apply(quaternionMatrix(a.quaternion()),current);
        if(std::hypot(field.x,field.y)<.05*norm(field))return false;
        const double target=-std::atan2(field.y,field.x);
        if(!ready_||generation_!=a.generation())offset_=target;
        else if(c.flags&MAG_CALIBRATED) {const double delta=std::atan2(std::sin(target-offset_),std::cos(target-offset_));const double dt=(now-lastMagMs_)/1000.0;offset_+=delta*(1-std::exp(-2*dt));}
        base_=multiply(c.worldImu,transpose(ref));ready_=true;generation_=a.generation();lastMagMs_=now;return true;
    }
    bool valid(const Attitude &a)const{return ready_&&a.valid()&&a.generation()==generation_;}
    Matrix orientation(const Attitude &a)const{Matrix yaw;yaw.v[0]=yaw.v[4]=std::cos(offset_);yaw.v[1]=-std::sin(offset_);yaw.v[3]=std::sin(offset_);return multiply(multiply(base_,yaw),quaternionMatrix(a.quaternion()));}
    uint32_t lastMagMs()const{return lastMagMs_;}
private:
    Matrix base_;bool ready_=false;uint32_t generation_=0,lastMagMs_=0;double offset_=0;
};
}
