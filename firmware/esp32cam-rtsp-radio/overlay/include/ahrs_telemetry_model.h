#pragma once
// Pure sensor conversion, six-axis attitude, and stationary calibration model.
// MPU-9250: https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ps-mpu-9250a-01-v1.1.pdf
// Registers: https://cdn.sparkfun.com/assets/learn_tutorials/5/5/0/MPU-9250-Register-Map.pdf
// BMP280: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ahrs_telemetry_model {
constexpr double PI = 3.14159265358979323846;
constexpr double GRAVITY = 9.80665;
constexpr double RAD_PER_DEG = PI / 180.0;
constexpr uint32_t IMU_FRESH_MS = 500;
constexpr uint32_t MAG_FRESH_MS = 1000;
constexpr uint32_t BARO_FRESH_MS = 2000;

struct Vec3 {
    double x, y, z;
    Vec3(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
};
struct Quaternion {
    double w, x, y, z;
    Quaternion(double a = 1, double b = 0, double c = 0, double d = 0)
        : w(a), x(b), y(c), z(d) {}
};
inline bool finite(const Vec3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline double norm(const Vec3 &v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
inline Vec3 subtract(const Vec3 &a, const Vec3 &b) {
    return Vec3(a.x-b.x, a.y-b.y, a.z-b.z);
}
inline bool fresh(bool valid, bool seen, uint32_t now, uint32_t observed, uint32_t limit) {
    return valid && seen && static_cast<uint32_t>(now-observed) <= limit;
}
inline int16_t signedWord(uint16_t value) {
    const int32_t number = value <= 32767u ? static_cast<int32_t>(value)
                                        : static_cast<int32_t>(value)-65536;
    return static_cast<int16_t>(number);
}
inline int16_t be16(const uint8_t *p) {
    return signedWord(static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]));
}
inline uint16_t leUnsigned16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
inline int16_t le16(const uint8_t *p) { return signedWord(leUnsigned16(p)); }
inline bool imuIdentity(uint8_t id) { return id == 0x71; }
inline bool magIdentity(uint8_t id) { return id == 0x48; }
inline bool barometerIdentity(uint8_t id) { return id == 0x58; }

struct ImuReading {
    Vec3 accel;
    Vec3 gyro;
    double temperature = 0;
    bool saturated = false;
};
inline ImuReading decodeImu(const uint8_t bytes[14]) {
    ImuReading value;
    const int16_t ax = be16(bytes), ay = be16(bytes+2), az = be16(bytes+4);
    const int16_t temperature = be16(bytes+6);
    const int16_t gx = be16(bytes+8), gy = be16(bytes+10), gz = be16(bytes+12);
    value.accel = Vec3(ax*GRAVITY/8192.0, ay*GRAVITY/8192.0, az*GRAVITY/8192.0);
    value.gyro = Vec3(gx*RAD_PER_DEG/131.0, gy*RAD_PER_DEG/131.0, gz*RAD_PER_DEG/131.0);
    value.temperature = temperature/333.87 + 21.0;
    const int16_t axes[] = {ax,ay,az,gx,gy,gz};
    for (int16_t axis : axes)
        if (axis == -32768 || axis == 32767) value.saturated = true;
    return value;
}

inline Vec3 magFactoryScale(const uint8_t asa[3]) {
    return Vec3(1.0+(static_cast<int>(asa[0])-128)/256.0,
                1.0+(static_cast<int>(asa[1])-128)/256.0,
                1.0+(static_cast<int>(asa[2])-128)/256.0);
}
enum class MagResult { NotReady, Valid, Overflow, WrongResolution };
inline MagResult decodeMag(const uint8_t bytes[8], const Vec3 &factoryScale, Vec3 &field) {
    // ST1 followed by six little-endian data bytes and ST2. Reading ST2 releases the latch.
    if (!(bytes[0] & 0x01)) return MagResult::NotReady;
    if (bytes[7] & 0x08) return MagResult::Overflow;
    if (!(bytes[7] & 0x10)) return MagResult::WrongResolution; // BITM must match configured 16-bit mode
    field = Vec3(le16(bytes+1)*0.15*factoryScale.x,
                 le16(bytes+3)*0.15*factoryScale.y,
                 le16(bytes+5)*0.15*factoryScale.z);
    return MagResult::Valid;
}

struct BmpCalibration {
    uint16_t t1 = 0, p1 = 0;
    int16_t t2 = 0, t3 = 0, p2 = 0, p3 = 0, p4 = 0;
    int16_t p5 = 0, p6 = 0, p7 = 0, p8 = 0, p9 = 0;
};
inline bool decodeBmpCalibration(const uint8_t bytes[24], BmpCalibration &c) {
    c.t1=leUnsigned16(bytes); c.t2=le16(bytes+2); c.t3=le16(bytes+4);
    c.p1=leUnsigned16(bytes+6); c.p2=le16(bytes+8); c.p3=le16(bytes+10);
    c.p4=le16(bytes+12); c.p5=le16(bytes+14); c.p6=le16(bytes+16);
    c.p7=le16(bytes+18); c.p8=le16(bytes+20); c.p9=le16(bytes+22);
    return c.t1 != 0 && c.t1 != 0xffff && c.p1 != 0 && c.p1 != 0xffff;
}
struct BarometerReading {
    double pressure = 0;
    double temperature = 0;
    double altitude = 0;
};
inline double standardPressureAltitude(double pressurePa) {
    return 44330.76923 * (1.0-std::pow(pressurePa/101325.0, 0.1902632365));
}
inline bool compensateBmp(uint32_t adcPressure, uint32_t adcTemperature,
                          const BmpCalibration &c, BarometerReading &out) {
    if (!c.p1 || !c.t1 || adcPressure > 0xfffff || adcTemperature > 0xfffff ||
        adcPressure == 0x80000 || adcTemperature == 0x80000) return false;
    // Bosch floating-point compensation equations, with same-sample t_fine.
    double v1=(adcTemperature/16384.0-c.t1/1024.0)*c.t2;
    const double t=adcTemperature/131072.0-c.t1/8192.0;
    double v2=t*t*c.t3;
    const double fine=v1+v2;
    const double temperature=fine/5120.0;
    v1=fine/2.0-64000.0;
    v2=v1*v1*c.p6/32768.0;
    v2=v2+v1*c.p5*2.0;
    v2=v2/4.0+c.p4*65536.0;
    v1=(c.p3*v1*v1/524288.0+c.p2*v1)/524288.0;
    v1=(1.0+v1/32768.0)*c.p1;
    if (v1 == 0 || !std::isfinite(v1)) return false;
    double pressure=(1048576.0-adcPressure-v2/4096.0)*6250.0/v1;
    v1=c.p9*pressure*pressure/2147483648.0;
    v2=pressure*c.p8/32768.0;
    pressure+=(v1+v2+c.p7)/16.0;
    if (!std::isfinite(pressure) || !std::isfinite(temperature) ||
        pressure < 30000 || pressure > 110000 || temperature < -40 || temperature > 85)
        return false;
    out.pressure=pressure;
    out.temperature=temperature;
    out.altitude=standardPressureAltitude(pressure);
    return std::isfinite(out.altitude);
}
inline bool decodeBmp(const uint8_t bytes[6], const BmpCalibration &c, BarometerReading &out) {
    const uint32_t p=(static_cast<uint32_t>(bytes[0])<<12) |
                     (static_cast<uint32_t>(bytes[1])<<4) | (bytes[2]>>4);
    const uint32_t t=(static_cast<uint32_t>(bytes[3])<<12) |
                     (static_cast<uint32_t>(bytes[4])<<4) | (bytes[5]>>4);
    return compensateBmp(p,t,c,out);
}

enum class ResetReason { Startup, SampleGap, Reconnected, Calibration, Paused, InvalidSample };
inline const char *resetReasonName(ResetReason reason) {
    switch (reason) {
        case ResetReason::SampleGap: return "sample_gap";
        case ResetReason::Reconnected: return "imu_reconnected";
        case ResetReason::Calibration: return "gyro_calibration";
        case ResetReason::Paused: return "paused";
        case ResetReason::InvalidSample: return "invalid_sample";
        default: return "startup";
    }
}
class Attitude {
public:
    void reset(ResetReason reason) {
        initialized_=false;
        accelCorrection_=false;
        reason_=reason;
    }
    bool update(const Vec3 &accel, const Vec3 &gyro, uint32_t now) {
        if (!finite(accel) || !finite(gyro)) {
            reset(ResetReason::InvalidSample);
            return false;
        }
        const double magnitude=norm(accel);
        const bool gravityUsable=magnitude >= 0.8*GRAVITY && magnitude <= 1.2*GRAVITY;
        const uint32_t elapsed=now-lastMs_;
        if (initialized_ && elapsed > 200) reset(ResetReason::SampleGap);
        if (!initialized_) {
            if (!gravityUsable) return false;
            const double roll=std::atan2(accel.y,accel.z);
            const double pitch=std::atan2(-accel.x,std::sqrt(accel.y*accel.y+accel.z*accel.z));
            const double cr=std::cos(roll/2), sr=std::sin(roll/2);
            const double cp=std::cos(pitch/2), sp=std::sin(pitch/2);
            q_=Quaternion(cr*cp,sr*cp,cr*sp,-sr*sp);
            initialized_=true;
            accelCorrection_=true;
            lastMs_=originMs_=now;
            ++generation_;
            return true;
        }
        if (!elapsed) return true;
        Vec3 corrected=gyro;
        accelCorrection_=gravityUsable;
        if (gravityUsable) {
            // Proportional Mahony gravity feedback; yaw has no absolute observation.
            const Vec3 measured(accel.x/magnitude,accel.y/magnitude,accel.z/magnitude);
            const Vec3 expected(2*(q_.x*q_.z-q_.w*q_.y),
                                2*(q_.w*q_.x+q_.y*q_.z),
                                q_.w*q_.w-q_.x*q_.x-q_.y*q_.y+q_.z*q_.z);
            const double gain=2.0;
            corrected.x+=gain*(measured.y*expected.z-measured.z*expected.y);
            corrected.y+=gain*(measured.z*expected.x-measured.x*expected.z);
            corrected.z+=gain*(measured.x*expected.y-measured.y*expected.x);
        }
        const double halfDt=elapsed/2000.0;
        const Quaternion before=q_;
        q_.w+=(-before.x*corrected.x-before.y*corrected.y-before.z*corrected.z)*halfDt;
        q_.x+=( before.w*corrected.x+before.y*corrected.z-before.z*corrected.y)*halfDt;
        q_.y+=( before.w*corrected.y-before.x*corrected.z+before.z*corrected.x)*halfDt;
        q_.z+=( before.w*corrected.z+before.x*corrected.y-before.y*corrected.x)*halfDt;
        const double length=std::sqrt(q_.w*q_.w+q_.x*q_.x+q_.y*q_.y+q_.z*q_.z);
        if (!std::isfinite(length) || length < 0.1) {
            reset(ResetReason::InvalidSample);
            return false;
        }
        q_.w/=length; q_.x/=length; q_.y/=length; q_.z/=length;
        lastMs_=now;
        return true;
    }
    bool valid() const { return initialized_; }
    bool accelCorrection() const { return accelCorrection_; }
    Quaternion quaternion() const { return q_; }
    Vec3 anglesDeg() const {
        double sine=2*(q_.w*q_.y-q_.z*q_.x);
        if (sine > 1) sine=1;
        if (sine < -1) sine=-1;
        return Vec3(std::atan2(2*(q_.w*q_.x+q_.y*q_.z),1-2*(q_.x*q_.x+q_.y*q_.y))/RAD_PER_DEG,
                    std::asin(sine)/RAD_PER_DEG,
                    std::atan2(2*(q_.w*q_.z+q_.x*q_.y),1-2*(q_.y*q_.y+q_.z*q_.z))/RAD_PER_DEG);
    }
    uint32_t generation() const { return generation_; }
    uint32_t originMs() const { return originMs_; }
    uint32_t observedMs() const { return lastMs_; }
    ResetReason resetReason() const { return reason_; }
private:
    Quaternion q_;
    bool initialized_=false, accelCorrection_=false;
    uint32_t lastMs_=0, originMs_=0, generation_=0;
    ResetReason reason_=ResetReason::Startup;
};

enum class CalibrationState {
    Uncalibrated, Queued, Collecting, Succeeded, MotionDetected,
    SamplingGap, Interrupted, Unavailable
};
inline const char *calibrationName(CalibrationState status) {
    switch (status) {
        case CalibrationState::Queued: return "queued";
        case CalibrationState::Collecting: return "collecting";
        case CalibrationState::Succeeded: return "succeeded";
        case CalibrationState::MotionDetected: return "motion_detected";
        case CalibrationState::SamplingGap: return "sampling_gap";
        case CalibrationState::Interrupted: return "interrupted";
        case CalibrationState::Unavailable: return "unavailable";
        default: return "uncalibrated";
    }
}
class StationaryCalibration {
public:
    void start(uint32_t now) {
        state_=CalibrationState::Collecting;
        started_=last_=now;
        count_=0;
        meanAccel_=meanGyro_=m2Accel_=m2Gyro_=Vec3();
    }
    bool active() const { return state_ == CalibrationState::Collecting; }
    void interrupt() { if (active()) state_=CalibrationState::Interrupted; }
    void checkTimeout(uint32_t now) {
        if (active() && static_cast<uint32_t>(now-last_) > 100)
            state_=CalibrationState::SamplingGap;
    }
    CalibrationState add(const Vec3 &accel, const Vec3 &gyro, uint32_t now) {
        if (!active()) return state_;
        if (static_cast<uint32_t>(now-last_) > 100 ||
            static_cast<uint32_t>(now-started_) > 5000) {
            state_=CalibrationState::SamplingGap;
            return state_;
        }
        if (!finite(accel) || !finite(gyro) ||
            norm(accel) < 0.9*GRAVITY || norm(accel) > 1.1*GRAVITY ||
            norm(gyro) > 5*RAD_PER_DEG) {
            state_=CalibrationState::MotionDetected;
            return state_;
        }
        last_=now;
        ++count_;
        accumulate(accel,meanAccel_,m2Accel_);
        accumulate(gyro,meanGyro_,m2Gyro_);
        if (static_cast<uint32_t>(now-started_) >= 2000 && count_ >= 80) {
            const double accelVariance=(m2Accel_.x+m2Accel_.y+m2Accel_.z)/(count_-1);
            const double gyroVariance=(m2Gyro_.x+m2Gyro_.y+m2Gyro_.z)/(count_-1);
            state_=(accelVariance <= 0.15*0.15 && gyroVariance <= 0.015*0.015)
                ? CalibrationState::Succeeded : CalibrationState::MotionDetected;
        }
        return state_;
    }
    CalibrationState state() const { return state_; }
    uint32_t samples() const { return count_; }
    Vec3 bias() const { return meanGyro_; }
private:
    void accumulate(const Vec3 &sample, Vec3 &mean, Vec3 &m2) {
        const Vec3 delta=subtract(sample,mean);
        mean.x+=delta.x/count_; mean.y+=delta.y/count_; mean.z+=delta.z/count_;
        m2.x+=delta.x*(sample.x-mean.x);
        m2.y+=delta.y*(sample.y-mean.y);
        m2.z+=delta.z*(sample.z-mean.z);
    }
    CalibrationState state_=CalibrationState::Uncalibrated;
    uint32_t started_=0, last_=0, count_=0;
    Vec3 meanAccel_,meanGyro_,m2Accel_,m2Gyro_;
};
} // namespace ahrs_telemetry_model
