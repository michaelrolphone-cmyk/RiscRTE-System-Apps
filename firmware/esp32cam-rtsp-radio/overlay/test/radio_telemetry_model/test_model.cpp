#include "radio_telemetry_model.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
using namespace radio_telemetry_model;

int main() {
    uint8_t mac[6] = {};
    assert(parseMac("AA:bb:01:02:03:fF", mac));
    assert(mac[0] == 0xaa && mac[5] == 0xff);
    assert(isUnicastTarget(mac));
    assert(!parseMac("aa:bb:cc:dd:ee:ffx", mac));
    assert(!parseMac("aa-bb:cc:dd:ee:ff", mac));
    assert(!parseMac("aa:bb:cc:dd:ee:fg", mac));
    assert(parseMac("ff:ff:ff:ff:ff:ff", mac) && !isUnicastTarget(mac));
    assert(parseMac("00:00:00:00:00:00", mac) && !isUnicastTarget(mac));
    assert(std::fabs(distanceMeters(-59.0f, -59.0f, 2.5f) - 1.0f) < 0.0001f);
    assert(std::fabs(distanceMeters(-84.0f, -59.0f, 2.5f) - 10.0f) < 0.0001f);
    assert(std::isnan(distanceMeters(-59.0f, -59.0f, 0.0f)));
    assert(std::isnan(distanceMeters(NAN, -59.0f, 2.5f)));
    assert(std::isnan(distanceMeters(-127.0f, -1.0f, 0.5f)));
    uint8_t payload[25] = {0x4c, 0x00, 0x02, 0x15};
    payload[4] = 0x12; payload[19] = 0xef;
    payload[20] = 0x12; payload[21] = 0x34;
    payload[22] = 0xab; payload[23] = 0xcd;
    payload[24] = static_cast<uint8_t>(-59);
    IBeacon beacon = {};
    assert(decodeIBeacon(payload, sizeof(payload), beacon));
    assert(beacon.uuid[0] == 0x12 && beacon.uuid[15] == 0xef);
    assert(beacon.major == 0x1234 && beacon.minor == 0xabcd);
    assert(beacon.referenceAtOneMeter == -59);
    assert(!decodeIBeacon(payload, 24, beacon));
    payload[3] = 0x14;
    assert(!decodeIBeacon(payload, sizeof(payload), beacon));
    payload[3] = 0x15; payload[24] = 0;
    assert(!decodeIBeacon(payload, sizeof(payload), beacon));
    std::cout << "radio model tests passed\n";
}
