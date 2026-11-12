#include "radio_telemetry_model.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
using namespace radio_telemetry_model;

int main() {
    PassiveWifiSweep sweep;
    assert(!sweep.begin(0, 13, 0));
    assert(!sweep.begin(1, 15, 0));
    assert(sweep.begin(1, 13, 100));
    uint32_t now = 100;
    for (unsigned channel = 1; channel <= 13; ++channel) {
        assert(sweep.active() && sweep.channel() == channel);
        const uint32_t next = sweep.advance(now, 60000);
        assert(next - now >= PassiveWifiSweep::SPACING_MS);
        now = next;
    }
    assert(!sweep.active() && now == 60100);
    // Regulatory subranges and 32-bit uptime wrap must retain channel order.
    assert(sweep.begin(6, 2, 0xffffff00u));
    assert(sweep.channel() == 6);
    now = sweep.advance(0xffffff00u, 60000);
    assert(now == uint32_t(0xffffff00u + 1000u) && sweep.channel() == 7);
    now = sweep.advance(now, 60000);
    assert(!sweep.active() && now == uint32_t(0xffffff00u + 60000u));
    assert(sweep.begin(1, 13, 0));
    now = 0;
    while (sweep.active()) { const uint32_t next = sweep.advance(now, 10000); assert(next - now >= 1000); now = next; }
    assert(now == 13000); // Never eliminate home-channel gaps for short intervals.
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
