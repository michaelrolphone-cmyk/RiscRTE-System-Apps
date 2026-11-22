#include <cassert>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

#include "../../src/live_websocket.h"

uint64_t transportTestClockUs = 1;

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
    ~Pair() { ::close(sockets[1]); }
};

void fillSocket(int descriptor) {
    uint8_t bytes[2048] = {};
    for (;;) {
        const ssize_t n = ::send(descriptor, bytes, sizeof(bytes), MSG_DONTWAIT);
        if (n > 0) continue;
        assert(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        return;
    }
}

void assertBoundedClose(LiveWebSocket &socket, uint64_t before) {
    assert(socket.disconnects == 1);
    assert(!socket.attached());
    if (transportTestClockUs - before != LiveWriteProgress::idleLimitUs)
        fprintf(stderr, "Control stall elapsed_us=%llu expected_us=%llu\n", static_cast<unsigned long long>(transportTestClockUs - before), static_cast<unsigned long long>(LiveWriteProgress::idleLimitUs));
    assert(transportTestClockUs - before == LiveWriteProgress::idleLimitUs);
}

void stalledHeartbeatDuringFragmentClosesOnlyVideo() {
    Pair videoPair, sensorPair;
    TestTcp videoTcp(videoPair.sockets[0]), sensorTcp(sensorPair.sockets[0]);
    LiveWebSocket video, sensor;
    video.attach(videoTcp); sensor.attach(sensorTcp);
    fillSocket(videoTcp.fd());
    video.scheduleHeartbeat();
    uint8_t frame[WEBSOCKETS_MAX_HEADER_SIZE + 32] = {};
    const uint64_t before = transportTestClockUs;
    assert(!video.writeLive(WSop_binary, frame, 32));
    assertBoundedClose(video, before);
    assert(video.controlBytes == 0);

    // The sensor socket remains usable after the video control write stalls.
    assert(sensor.replyPong() == 6);
    assert(sensor.attached() && sensor.disconnects == 0);
    uint8_t received[6];
    assert(recv(sensorPair.sockets[1], received, sizeof(received), MSG_DONTWAIT) == 6);
    assert(received[0] == 0x8a);
}

void stalledPongAndHandshakeUseBoundedPath() {
    for (int handshake = 0; handshake < 2; ++handshake) {
        Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket;
        socket.attach(tcp); fillSocket(tcp.fd());
        const uint64_t before = transportTestClockUs;
        assert((handshake ? socket.sendHandshake() : socket.replyPong()) == 0);
        assertBoundedClose(socket, before);
    }
}

void partialDataWriteClosesWithoutReplay() {
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket;
    socket.attach(tcp);
    std::vector<uint8_t> frame(WEBSOCKETS_MAX_HEADER_SIZE + 65536, 0x5a);
    const uint64_t before = transportTestClockUs;
    assert(!socket.writeLive(WSop_binary, frame.data(), 65536));
    assertBoundedClose(socket, before);
    uint8_t wire[65536];
    const ssize_t received = recv(pair.sockets[1], wire, sizeof(wire), MSG_DONTWAIT);
    assert(received > 0 && received < 65536);
    assert((wire[0] & 0x0f) == WSop_binary);
}

void successfulControlAndDataRetainWireFormat() {
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket;
    socket.attach(tcp); socket.scheduleHeartbeat();
    std::vector<uint8_t> frame(WEBSOCKETS_MAX_HEADER_SIZE + 32);
    for (size_t i = 0; i < 32; ++i) frame[WEBSOCKETS_MAX_HEADER_SIZE + i] = static_cast<uint8_t>(i);
    const uint64_t before = transportTestClockUs;
    assert(socket.writeLive(WSop_binary, frame.data(), 32));
    assert(socket.controlBytes == 6 && socket.disconnects == 0);
    assert(transportTestClockUs == before);
    uint8_t wire[64];
    const ssize_t received = recv(pair.sockets[1], wire, sizeof(wire), MSG_DONTWAIT);
    assert(received == 6 + 6 + 32);
    assert(wire[0] == 0x89 && wire[6] == 0x82 && wire[7] == (0x80 | 32));
    for (size_t i = 0; i < 32; ++i) assert((wire[12 + i] ^ wire[8 + i % 4]) == i);
}

int main() {
    stalledHeartbeatDuringFragmentClosesOnlyVideo();
    stalledPongAndHandshakeUseBoundedPath();
    partialDataWriteClosesWithoutReplay();
    successfulControlAndDataRetainWireFormat();
    puts("4 live socket transport regressions passed");
}
