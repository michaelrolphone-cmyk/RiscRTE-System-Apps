#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include "esp_timer.h"

#define WEBSOCKETS_MAX_HEADER_SIZE 14
enum WSopcode_t { WSop_continuation = 0, WSop_text = 1, WSop_binary = 2,
                  WSop_close = 8, WSop_ping = 9, WSop_pong = 10 };

class TestTcp {
public:
    explicit TestTcp(int socket) : descriptor(socket) {}
    ~TestTcp() { stop(); }
    int fd() const { return descriptor; }
    void stop() { if (descriptor >= 0) { ::close(descriptor); descriptor = -1; } }
    int setSocketOption(int level, int name, const void *value, size_t size) {
        return setsockopt(descriptor, level, name, value, static_cast<socklen_t>(size));
    }
private:
    int descriptor;
};
struct WSclient_t { TestTcp *tcp = nullptr; };

// Match the library's virtual write hook and its inherited control-frame call
// sites. The legacy five-second idle wait uses the deterministic test clock;
// packet I/O and backpressure use real sockets. This exposes the old deadline
// failure without adding wall-clock delays to CI.
class WebSocketsClient {
public:
    virtual ~WebSocketsClient() {}
    size_t fallbackWrites = 0, controlBytes = 0, disconnects = 0;
    void attach(TestTcp &tcp) { _client.tcp = &tcp; }
    bool attached() const { return _client.tcp && _client.tcp->fd() >= 0; }
    void scheduleHeartbeat() { heartbeatPending = true; }
    void loop() {
        if (heartbeatPending) {
            heartbeatPending = false;
            uint8_t ping[] = {0x89, 0x80, 1, 2, 3, 4};
            controlBytes = write(&_client, ping, sizeof(ping));
        }
    }
    size_t replyPong() {
        uint8_t pong[] = {0x8a, 0x80, 1, 2, 3, 4};
        return write(&_client, pong, sizeof(pong));
    }
    size_t sendHandshake() {
        uint8_t request[] = {'G', 'E', 'T', ' ', '/', '\r', '\n'};
        return write(&_client, request, sizeof(request));
    }
protected:
    WSclient_t _client;
    virtual size_t write(WSclient_t *client, uint8_t *out, size_t size) {
        ++fallbackWrites;
        uint64_t lastProgress = esp_timer_get_time();
        size_t total = 0;
        while (client->tcp && total < size) {
            const ssize_t count = ::send(client->tcp->fd(), out + total, size - total, MSG_DONTWAIT);
            if (count > 0) { total += count; lastProgress = esp_timer_get_time(); }
            else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                if (static_cast<uint64_t>(esp_timer_get_time()) - lastProgress >= 5000000) break;
                vTaskDelay(1);
            } else break;
        }
        return total;
    }
    void clientDisconnect(WSclient_t *client, const char *) {
        ++disconnects;
        if (client->tcp) client->tcp->stop();
        client->tcp = nullptr;
    }
    uint8_t createHeader(uint8_t *out, WSopcode_t opcode, size_t size,
                         bool masked, const uint8_t *mask, bool fin) {
        out[0] = static_cast<uint8_t>((fin ? 0x80 : 0) | opcode);
        size_t n = 2;
        if (size < 126) out[1] = static_cast<uint8_t>(size);
        else {
            out[1] = 126; out[2] = static_cast<uint8_t>(size >> 8);
            out[3] = static_cast<uint8_t>(size); n = 4;
        }
        if (masked) {
            out[1] |= 0x80;
            memcpy(out + n, mask, 4);
            n += 4;
        }
        return static_cast<uint8_t>(n);
    }
private:
    bool heartbeatPending = false;
};
