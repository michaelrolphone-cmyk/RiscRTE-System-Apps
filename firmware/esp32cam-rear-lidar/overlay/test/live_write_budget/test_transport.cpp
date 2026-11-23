#include <cassert>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <functional>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

#include "live_websocket.h"

uint64_t transportTestClockUs = 1;
std::function<void()> transportTestOnDelay;
static_assert(LiveWriteProgress::idleLimitUs == EXPECTED_IDLE_LIMIT_US,
              "The tested header must carry the selected front/rear budget");

namespace {
constexpr uint64_t peerPauseUs = 3000000;
constexpr uint64_t sensorPeriodUs = 200000;

struct Pair {
    int sockets[2];
    Pair() {
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
        for (int descriptor : sockets) {
            const int flags = fcntl(descriptor, F_GETFL, 0);
            assert(flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0);
        }
        int bytes = 4096;
        assert(setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes)) == 0);
    }
    ~Pair() { ::close(sockets[1]); } // TestTcp owns the sending descriptor.
};

void resetClock() {
    transportTestClockUs = 1;
    transportTestOnDelay = nullptr;
}

size_t fillSocket(int descriptor) {
    uint8_t bytes[2048] = {};
    size_t total = 0;
    for (;;) {
        const ssize_t count = ::send(descriptor, bytes, sizeof(bytes), MSG_DONTWAIT);
        if (count > 0) { total += static_cast<size_t>(count); continue; }
        assert(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        assert(total > 0);
        return total;
    }
}

std::vector<uint8_t> drainSocket(int descriptor) {
    std::vector<uint8_t> result;
    uint8_t bytes[2048];
    for (;;) {
        const ssize_t count = recv(descriptor, bytes, sizeof(bytes), MSG_DONTWAIT);
        if (count > 0) { result.insert(result.end(), bytes, bytes + count); continue; }
        assert(count == 0 || (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)));
        return result;
    }
}

void sensorTick(LiveWebSocket &sensor, int peer) {
    assert(sensor.replyPong() == 6);
    const auto wire = drainSocket(peer);
    assert(wire.size() == 6 && wire[0] == 0x8a);
    assert(sensor.attached() && sensor.disconnects == 0);
    assert(sensor.writeTimeoutCount() == 0 && sensor.fallbackWrites == 0);
}

void assertClosedAt(LiveWebSocket &socket, uint64_t before, uint64_t elapsed) {
    assert(!socket.attached() && socket.disconnects == 1);
    assert(socket.writeTimeoutCount() == 1 && socket.fallbackWrites == 0);
    assert(transportTestClockUs - before == elapsed);
}

void threeSecondPauseThenHeartbeatAndDataRecovery() {
    resetClock();
    Pair videoPair, sensorPair;
    TestTcp videoTcp(videoPair.sockets[0]), sensorTcp(sensorPair.sockets[0]);
    LiveWebSocket video, sensor;
    video.attach(videoTcp); sensor.attach(sensorTcp);
    const size_t filled = fillSocket(videoTcp.fd());
    video.scheduleHeartbeat();
    const uint64_t before = transportTestClockUs;
    uint64_t lastSensor = before;
    unsigned sensorWrites = 0;
    bool released = false;
    transportTestOnDelay = [&]() {
        if (transportTestClockUs - lastSensor >= sensorPeriodUs) {
            sensorTick(sensor, sensorPair.sockets[1]);
            ++sensorWrites; lastSensor = transportTestClockUs;
        }
        if (!released && transportTestClockUs - before >= peerPauseUs) {
            assert(drainSocket(videoPair.sockets[1]).size() == filled);
            released = true;
        }
    };
    std::vector<uint8_t> frame(WEBSOCKETS_MAX_HEADER_SIZE + 32);
    for (size_t i = 0; i < 32; ++i) frame[WEBSOCKETS_MAX_HEADER_SIZE + i] = i;
    const bool result = video.writeLive(WSop_binary, frame.data(), 32);
    transportTestOnDelay = nullptr;
    if (LiveWriteProgress::idleLimitUs > peerPauseUs) {
        assert(result && released && video.attached() && video.disconnects == 0);
        assert(video.writeTimeoutCount() == 0 && video.fallbackWrites == 0);
        assert(transportTestClockUs - before == peerPauseUs);
        const auto wire = drainSocket(videoPair.sockets[1]);
        assert(wire.size() == 44 && wire[0] == 0x89);
        assert(wire[6] == 0x82 && wire[7] == (0x80 | 32));
        for (size_t i = 0; i < 32; ++i) assert((wire[12 + i] ^ wire[8 + i % 4]) == i);
        assert(sensorWrites == peerPauseUs / sensorPeriodUs);
    } else {
        assert(!result && !released);
        assertClosedAt(video, before, LiveWriteProgress::idleLimitUs);
        assert(sensorWrites == LiveWriteProgress::idleLimitUs / sensorPeriodUs);
    }
}

void absentReaderClosesVideoWhileSensorKeepsWriting() {
    resetClock();
    Pair videoPair, sensorPair;
    TestTcp videoTcp(videoPair.sockets[0]), sensorTcp(sensorPair.sockets[0]);
    LiveWebSocket video, sensor;
    video.attach(videoTcp); sensor.attach(sensorTcp);
    fillSocket(videoTcp.fd());
    const uint64_t before = transportTestClockUs;
    uint64_t lastSensor = before;
    unsigned sensorWrites = 0;
    transportTestOnDelay = [&]() {
        if (transportTestClockUs - lastSensor >= sensorPeriodUs) {
            sensorTick(sensor, sensorPair.sockets[1]);
            ++sensorWrites; lastSensor = transportTestClockUs;
        }
    };
    uint8_t frame[WEBSOCKETS_MAX_HEADER_SIZE + 32] = {};
    assert(!video.writeLive(WSop_binary, frame, 32));
    transportTestOnDelay = nullptr;
    assertClosedAt(video, before, LiveWriteProgress::idleLimitUs);
    assert(sensorWrites == LiveWriteProgress::idleLimitUs / sensorPeriodUs);
    sensorTick(sensor, sensorPair.sockets[1]);
}

void byteProgressRestartsIdleBudgetAndReconnectIsClean() {
    resetClock();
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket;
    socket.attach(tcp);
    const uint64_t before = transportTestClockUs;
    bool released = false;
    transportTestOnDelay = [&]() {
        if (!released && transportTestClockUs - before >= peerPauseUs) {
            const auto partial = drainSocket(pair.sockets[1]);
            assert(!partial.empty() && partial.size() < 65536);
            released = true;
        }
    };
    std::vector<uint8_t> frame(WEBSOCKETS_MAX_HEADER_SIZE + 65536, 0x5a);
    assert(!socket.writeLive(WSop_binary, frame.data(), 65536));
    transportTestOnDelay = nullptr;
    const bool canRecover = LiveWriteProgress::idleLimitUs > peerPauseUs;
    assert(released == canRecover);
    assertClosedAt(socket, before, LiveWriteProgress::idleLimitUs + (canRecover ? peerPauseUs : 0));

    Pair replacement; TestTcp replacementTcp(replacement.sockets[0]);
    socket.attach(replacementTcp);
    socket.transportChanged(false);
    std::vector<uint8_t> fresh(WEBSOCKETS_MAX_HEADER_SIZE + 32, 0xc7);
    assert(socket.writeLive(WSop_binary, fresh.data(), 32));
    const auto wire = drainSocket(replacement.sockets[1]);
    assert(wire.size() == 38 && wire[0] == 0x82);
    for (size_t i = 0; i < 32; ++i) assert((wire[6 + i] ^ wire[2 + i % 4]) == 0xc7);
    assert(socket.attached() && socket.disconnects == 1 && socket.fallbackWrites == 0);
}

void pongAndHandshakeCanRecoverBeforeTheSameBudget() {
    for (int handshake = 0; handshake < 2; ++handshake) {
        resetClock();
        Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket;
        socket.attach(tcp);
        const size_t filled = fillSocket(tcp.fd());
        const uint64_t before = transportTestClockUs;
        bool released = false;
        transportTestOnDelay = [&]() {
            if (!released && transportTestClockUs - before >= peerPauseUs) {
                assert(drainSocket(pair.sockets[1]).size() == filled);
                released = true;
            }
        };
        const size_t count = handshake ? socket.sendHandshake() : socket.replyPong();
        transportTestOnDelay = nullptr;
        if (LiveWriteProgress::idleLimitUs > peerPauseUs) {
            assert(count == (handshake ? 7U : 6U) && released);
            assert(transportTestClockUs - before == peerPauseUs);
            const auto wire = drainSocket(pair.sockets[1]);
            assert(wire.size() == count && wire[0] == (handshake ? 'G' : 0x8a));
            assert(socket.attached() && socket.disconnects == 0);
            assert(socket.writeTimeoutCount() == 0 && socket.fallbackWrites == 0);
        } else {
            assert(count == 0 && !released);
            assertClosedAt(socket, before, LiveWriteProgress::idleLimitUs);
        }
    }
}
}

int main() {
    threeSecondPauseThenHeartbeatAndDataRecovery();
    absentReaderClosesVideoWhileSensorKeepsWriting();
    byteProgressRestartsIdleBudgetAndReconnectIsClean();
    pongAndHandshakeCanRecoverBeforeTheSameBudget();
    printf("4 transport-budget scenarios passed: idle_limit_us=%llu, real socket backpressure, deterministic clock\n",
           static_cast<unsigned long long>(LiveWriteProgress::idleLimitUs));
}
