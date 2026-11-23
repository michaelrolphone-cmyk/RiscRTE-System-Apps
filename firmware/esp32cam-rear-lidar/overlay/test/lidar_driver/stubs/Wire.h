#pragma once
#include <Arduino.h>
#include <algorithm>
#include <functional>
#include <map>
#include <vector>

// This boundary models returned byte counts and register-pointer continuation.
// The test executes the production IO/platform/ULD implementation above it.
class TwoWire {
public:
    struct Peer {
        int count = -1, nack = 0;
        unsigned requests = 0, failAt = 0, reads = 0, transmissions = 0, bulkWrites = 0;
        uint8_t value = 0;
        std::vector<uint8_t> memory;
        std::function<uint8_t(uint16_t)> response;
    };
    std::map<uint8_t, Peer> peers;
    std::vector<size_t> requested;
    void beginTransmission(uint8_t address) { active = address; tx.clear(); }
    size_t write(uint8_t value) { tx.push_back(value); return 1; }
    uint8_t endTransmission(bool = true) {
        Peer &peer = peers[active];
        ++peer.transmissions;
        if (peer.nack) return static_cast<uint8_t>(peer.nack);
        if (tx.size() >= 2) cursor[active] = (static_cast<uint16_t>(tx[0]) << 8) | tx[1];
        if (tx.size() > 4) ++peer.bulkWrites;
        return 0;
    }
    size_t requestFrom(uint8_t address, size_t count) {
        active = address;
        Peer &peer = peers[address];
        ++peer.requests; requested.push_back(count); rx.clear(); at = 0;
        size_t delivered = peer.count < 0 ? count : std::min(count, static_cast<size_t>(peer.count));
        if (peer.failAt && peer.requests >= peer.failAt) delivered = 0;
        for (size_t i = 0; i < delivered; ++i) {
            const uint16_t position = cursor[address]++;
            rx.push_back(peer.response ? peer.response(position) :
                position < peer.memory.size() ? peer.memory[position] : peer.value);
        }
        return rx.size();
    }
    int available() { return static_cast<int>(rx.size() - at); }
    int read() {
        ++peers[active].reads;
        return at < rx.size() ? rx[at++] : -1;
    }
private:
    uint8_t active = 0;
    std::map<uint8_t, uint16_t> cursor;
    std::vector<uint8_t> tx, rx;
    size_t at = 0;
};
inline TwoWire Wire;
