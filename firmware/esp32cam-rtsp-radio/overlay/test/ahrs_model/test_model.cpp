#include "ahrs_telemetry_model.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
using namespace ahrs_telemetry_model;

static bool near(double a,double b,double tolerance) { return std::fabs(a-b) <= tolerance; }
static void putBig(uint8_t *p,int16_t value) {
    const uint16_t word=static_cast<uint16_t>(value);
    p[0]=static_cast<uint8_t>(word>>8); p[1]=static_cast<uint8_t>(word);
}
static void putLittle(uint8_t *p,int value) {
    const uint16_t word=static_cast<uint16_t>(value);
    p[0]=static_cast<uint8_t>(word); p[1]=static_cast<uint8_t>(word>>8);
}

static void identities_and_imu_units() {
    assert(imuIdentity(0x71) && !imuIdentity(0x73) && !imuIdentity(0x68));
    assert(magIdentity(0x48) && !magIdentity(0x09));
    assert(barometerIdentity(0x58) && !barometerIdentity(0x60));
    uint8_t bytes[14]={};
    putBig(bytes,8192); putBig(bytes+2,-4096); putBig(bytes+4,0);
    putBig(bytes+6,3339); putBig(bytes+8,1310); putBig(bytes+10,-2620);
    const ImuReading reading=decodeImu(bytes);
    assert(near(reading.accel.x,GRAVITY,1e-12));
    assert(near(reading.accel.y,-GRAVITY/2,1e-12));
    assert(near(reading.gyro.x,10*RAD_PER_DEG,1e-12));
    assert(near(reading.gyro.y,-20*RAD_PER_DEG,1e-12));
    assert(near(reading.temperature,31.0,0.01));
    assert(!reading.saturated);
    putBig(bytes,-32768);
    assert(decodeImu(bytes).saturated);
    assert(be16(bytes)==-32768);
}
static void magnetometer_factory_trim_and_status() {
    const uint8_t asa[3]={128,192,64};
    const Vec3 trim=magFactoryScale(asa);
    assert(near(trim.x,1,1e-12) && near(trim.y,1.25,1e-12) && near(trim.z,0.75,1e-12));
    uint8_t bytes[8]={1,0,0,0,0,0,0,0x10};
    putLittle(bytes+1,1000); putLittle(bytes+3,-1000); putLittle(bytes+5,2000);
    Vec3 field;
    assert(decodeMag(bytes,trim,field)==MagResult::Valid);
    assert(near(field.x,150,1e-10) && near(field.y,-187.5,1e-10) && near(field.z,225,1e-10));
    bytes[0]=3; // Overrun means skipped samples, not invalid current data.
    assert(decodeMag(bytes,trim,field)==MagResult::Valid);
    bytes[7]=0x18;
    assert(decodeMag(bytes,trim,field)==MagResult::Overflow);
    bytes[7]=0;
    assert(decodeMag(bytes,trim,field)==MagResult::WrongResolution);
    bytes[0]=0;
    assert(decodeMag(bytes,trim,field)==MagResult::NotReady);
}
static void bosch_published_compensation_vector() {
    // Bosch datasheet example: adc_T=519888, adc_P=415148.
    const int coefficients[12]={27504,26435,-1000,36477,-10685,3024,2855,140,-7,15500,-14600,6000};
    uint8_t encoded[24]={};
    for (size_t i=0;i<12;++i) putLittle(encoded+2*i,coefficients[i]);
    BmpCalibration calibration;
    assert(decodeBmpCalibration(encoded,calibration));
    assert(calibration.t1==27504 && calibration.p1==36477 && calibration.p2==-10685 && calibration.p6==-7);
    BarometerReading reading;
    assert(compensateBmp(415148,519888,calibration,reading));
    assert(near(reading.temperature,25.08247793,0.00001));
    assert(near(reading.pressure,100653.2668,0.01));
    assert(reading.altitude>56 && reading.altitude<57);
    const uint8_t raw[6]={0x65,0x5a,0xc0,0x7e,0xed,0x00};
    assert(decodeBmp(raw,calibration,reading));
    assert(near(reading.pressure,100653.2668,0.01));
    assert(near(standardPressureAltitude(101325),0,1e-9));
    assert(near(standardPressureAltitude(89874.6),1000,0.1));
    calibration.p1=0;
    assert(!compensateBmp(415148,519888,calibration,reading));
    for (uint8_t &byte:encoded) byte=0xff;
    assert(!decodeBmpCalibration(encoded,calibration));
    for (uint8_t &byte:encoded) byte=0;
    assert(!decodeBmpCalibration(encoded,calibration));
}
static void gravity_orientations_and_quaternion() {
    Attitude flat,roll,pitch,upsideDown;
    assert(flat.update(Vec3(0,0,GRAVITY),Vec3(),0));
    assert(near(flat.anglesDeg().x,0,1e-9) && near(flat.anglesDeg().y,0,1e-9));
    const double half=GRAVITY/std::sqrt(2.0);
    assert(roll.update(Vec3(0,half,half),Vec3(),0));
    assert(near(roll.anglesDeg().x,45,1e-9) && near(roll.anglesDeg().z,0,1e-9));
    assert(pitch.update(Vec3(-GRAVITY/2,0,GRAVITY*std::sqrt(3.0)/2),Vec3(),0));
    assert(near(pitch.anglesDeg().y,30,1e-9));
    assert(upsideDown.update(Vec3(0,0,-GRAVITY),Vec3(),0));
    assert(near(std::fabs(upsideDown.anglesDeg().x),180,1e-9));
    const Quaternion q=roll.quaternion();
    assert(near(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z,1,1e-12));
}
static void gyro_integration_and_gravity_feedback() {
    Attitude yaw;
    assert(yaw.update(Vec3(0,0,GRAVITY),Vec3(),0));
    for (uint32_t i=1;i<=50;++i)
        assert(yaw.update(Vec3(0,0,GRAVITY),Vec3(0,0,PI_VALUE/2),20*i));
    assert(near(yaw.anglesDeg().z,90,0.05));
    assert(yaw.generation()==1 && yaw.originMs()==0);
    // Sustained specific acceleration outside the gravity band must not pull tilt.
    assert(yaw.update(Vec3(0,0,2*GRAVITY),Vec3(),1020));
    assert(!yaw.accelCorrection());
    assert(near(yaw.anglesDeg().x,0,1e-8));
    Attitude tilt;
    assert(tilt.update(Vec3(0,0,GRAVITY),Vec3(),0));
    for (uint32_t i=1;i<=150;++i)
        assert(tilt.update(Vec3(0,GRAVITY/2,GRAVITY*std::sqrt(3.0)/2),Vec3(),20*i));
    assert(near(tilt.anglesDeg().x,30,0.1)); // verifies correction sign and convergence
}
static void timing_gaps_wrap_and_invalid_samples() {
    assert(fresh(true,true,15,UINT32_MAX-4,20));
    assert(!fresh(true,true,16,UINT32_MAX-4,20));
    assert(!fresh(false,true,0,0,20) && !fresh(true,false,0,0,20));
    Attitude attitude;
    assert(!attitude.update(Vec3(),Vec3(),0));
    assert(!attitude.valid());
    assert(attitude.update(Vec3(0,0,GRAVITY),Vec3(),UINT32_MAX-15));
    assert(attitude.update(Vec3(0,0,GRAVITY),Vec3(0,0,PI_VALUE/2),16));
    assert(near(attitude.anglesDeg().z,2.88,0.01));
    assert(attitude.generation()==1);
    assert(attitude.update(Vec3(0,0,GRAVITY),Vec3(0,0,PI_VALUE/2),1000));
    assert(attitude.generation()==2 && attitude.originMs()==1000);
    assert(attitude.resetReason()==ResetReason::SampleGap);
    assert(near(attitude.anglesDeg().z,0,1e-9));
    assert(!attitude.update(Vec3(0,0,GRAVITY),Vec3(std::numeric_limits<double>::quiet_NaN(),0,0),1020));
    assert(!attitude.valid());
    assert(attitude.update(Vec3(0,0,GRAVITY),Vec3(),1040));
    assert(attitude.generation()==3);
}
static void requested_stationary_bias_window() {
    StationaryCalibration calibration;
    assert(!calibration.active() && calibration.state()==CalibrationState::Uncalibrated);
    calibration.start(0);
    const Vec3 bias(0.01,-0.02,0.005);
    for (uint32_t i=0;i<=100;++i)
        calibration.add(Vec3(0,0,GRAVITY),bias,i*20);
    assert(calibration.state()==CalibrationState::Succeeded && calibration.samples()==101);
    assert(near(calibration.bias().x,bias.x,1e-12) &&
           near(calibration.bias().y,bias.y,1e-12) &&
           near(calibration.bias().z,bias.z,1e-12));
    calibration.start(UINT32_MAX-500);
    for (uint32_t i=0;i<=100;++i)
        calibration.add(Vec3(0,GRAVITY,0),bias,static_cast<uint32_t>(UINT32_MAX-500+i*20));
    assert(calibration.state()==CalibrationState::Succeeded);
}
static void calibration_rejects_motion_variance_and_gaps() {
    StationaryCalibration calibration;
    calibration.start(0);
    assert(calibration.add(Vec3(0,0,GRAVITY),Vec3(0.2,0,0),20)==CalibrationState::MotionDetected);
    calibration.start(0);
    assert(calibration.add(Vec3(0,0,1.2*GRAVITY),Vec3(),20)==CalibrationState::MotionDetected);
    calibration.start(0);
    const double vertical=std::sqrt(GRAVITY*GRAVITY-0.2*0.2);
    for (uint32_t i=0;i<=100;++i)
        calibration.add(Vec3(i%2 ? 0.2 : -0.2,0,vertical),Vec3(),i*20);
    assert(calibration.state()==CalibrationState::MotionDetected); // norm alone would accept this
    calibration.start(0);
    for (uint32_t i=0;i<=100;++i)
        calibration.add(Vec3(0,0,GRAVITY),Vec3(i%2 ? 0.02 : -0.02,0,0),i*20);
    assert(calibration.state()==CalibrationState::MotionDetected);
    calibration.start(0);
    assert(calibration.add(Vec3(0,0,GRAVITY),Vec3(),120)==CalibrationState::SamplingGap);
    calibration.start(10);
    calibration.checkTimeout(111);
    assert(calibration.state()==CalibrationState::SamplingGap);
    calibration.start(0);
    calibration.interrupt();
    assert(calibration.state()==CalibrationState::Interrupted);
}
int main() {
    identities_and_imu_units();
    magnetometer_factory_trim_and_status();
    bosch_published_compensation_vector();
    gravity_orientations_and_quaternion();
    gyro_integration_and_gravity_feedback();
    timing_gaps_wrap_and_invalid_samples();
    requested_stationary_bias_window();
    calibration_rejects_motion_variance_and_gaps();
    std::cout << "8 AHRS model tests passed\n";
}
