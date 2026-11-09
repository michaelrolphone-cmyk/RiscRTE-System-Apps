#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace radio_telemetry_model {
inline int hexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline bool parseMac(const char *text, uint8_t address[6]) {
    if (!text || std::strlen(text) != 17) return false;
    for (size_t i = 0; i < 6; ++i) {
        int high = hexDigit(text[i * 3]);
        int low = hexDigit(text[i * 3 + 1]);
        if (high < 0 || low < 0 || (i < 5 && text[i * 3 + 2] != ':')) return false;
        address[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

inline bool isUnicastTarget(const uint8_t address[6]) {
    uint8_t any = 0;
    for (size_t i = 0; i < 6; ++i) any |= address[i];
    return any != 0 && !(address[0] & 1);
}

inline float distanceMeters(float rssi, float referenceAtOneMeter, float exponent) {
    if (!std::isfinite(rssi) || !std::isfinite(referenceAtOneMeter) ||
        !std::isfinite(exponent) || exponent < 0.5f || exponent > 6.0f)
        return NAN;
    float distance = std::pow(10.0f, (referenceAtOneMeter - rssi) / (10.0f * exponent));
    return std::isfinite(distance) && distance <= 10000.0f ? distance : NAN;
}

struct IBeacon {
    uint8_t uuid[16];
    uint16_t major;
    uint16_t minor;
    int8_t referenceAtOneMeter;
};

inline bool decodeIBeacon(const uint8_t *data, size_t length, IBeacon &out) {
    if (!data || length != 25 || data[0] != 0x4c || data[1] != 0x00 ||
        data[2] != 0x02 || data[3] != 0x15)
        return false;
    const int power = static_cast<int8_t>(data[24]);
    if (power >= 0 || power < -127) return false;
    std::memcpy(out.uuid, data + 4, sizeof(out.uuid));
    out.major = static_cast<uint16_t>((data[20] << 8) | data[21]);
    out.minor = static_cast<uint16_t>((data[22] << 8) | data[23]);
    out.referenceAtOneMeter = static_cast<int8_t>(power);
    return true;
}
} // namespace radio_telemetry_model
