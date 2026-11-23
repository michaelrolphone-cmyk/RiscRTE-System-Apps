#pragma once

#include <algorithm>
#include <atomic>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <lwip/sockets.h>
#include <lwip/opt.h>
#include <errno.h>
#include "live_write_progress.h"
#ifndef LIVE_WEBSOCKET_SEND_TIMING
#define LIVE_WEBSOCKET_SEND_TIMING 0
#endif
#if LIVE_WEBSOCKET_SEND_TIMING
#include "live_send_timing.h"
#endif

// The Links2004 client owns the handshake, incoming messages and control frames.
// Keep its reader running between outgoing fragments, including when TCP is full.
class LiveWebSocket : public WebSocketsClient {
public:
    uint32_t writeTimeoutCount() const { return writeTimeouts.load(); }
    uint32_t lastWriteProgressMs() const { return lastProgressMs.load(); }

    void transportChanged(bool telemetry) {
        ++epoch;
        clockId = "";
        if (telemetry && _client.tcp) {
            const int tos = 0xa0;  // CS5 prioritizes sensor data on WMM-capable links.
            _client.tcp->setSocketOption(IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
        }
    }

    void noteClock(JsonVariantConst id, uint64_t received) {
        if (!id.is<const char *>() && !id.is<uint64_t>()) return;
        String encodedId;
        serializeJson(id, encodedId);
        if (clockId != encodedId) {
            clockId = encodedId;
            clockReceived = received;
        }
    }

    bool flushClock(const String &boot) {
        if (clockId.isEmpty()) return false;
        const String replyingTo = clockId;
        StaticJsonDocument<512> reply;
        reply["type"] = "clock";
        reply["id"] = serialized(replyingTo);
        reply["boot_id"] = boot;
        reply["received_monotonic_us"] = clockReceived;
        reply["served_monotonic_us"] = static_cast<uint64_t>(esp_timer_get_time());
        uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE + 512];
        char *payload = reinterpret_cast<char *>(buffer + WEBSOCKETS_MAX_HEADER_SIZE);
        const size_t size = serializeJson(reply, payload, 512);
        if (!writeLive(WSop_text, buffer, size)) return false;
        if (clockId == replyingTo) clockId = "";
        return true;
    }

    bool writeLive(WSopcode_t kind, uint8_t *buffer, size_t size,
                   bool yieldBetweenFragments = false
#if LIVE_WEBSOCKET_SEND_TIMING
                   , LiveSendTiming *timing = nullptr
#endif
                   ) {
#if LIVE_WEBSOCKET_SEND_TIMING
        LiveSendAttempt attempt(timing, size);
#endif
        const uint32_t session = epoch;
        const size_t maximum = TCP_MSS - WEBSOCKETS_MAX_HEADER_SIZE;
        size_t offset = 0;
        while (offset < size) {
            if (!_client.tcp || epoch != session) return false;
            const size_t length = std::min(maximum, size - offset);
            uint8_t *body = buffer + WEBSOCKETS_MAX_HEADER_SIZE + offset;
            uint8_t header[WEBSOCKETS_MAX_HEADER_SIZE];
            uint8_t mask[4];
            esp_fill_random(mask, sizeof(mask));
            const uint8_t headerSize = createHeader(header, offset ? WSop_continuation : kind,
                length, true, mask, offset + length == size);
            // Earlier fragment bytes have already been copied into the TCP stack.
            uint8_t *wire = body - headerSize;
            memcpy(wire, header, headerSize);
            for (size_t i = 0; i < length; ++i) body[i] ^= mask[i % 4];
            // The inherited loop may send heartbeat/pong frames here. Its virtual
            // write() uses the same nonblocking path, including on a full socket.
#if LIVE_WEBSOCKET_SEND_TIMING
            const uint64_t loopStart = timing ? esp_timer_get_time() : 0;
#endif
            loop();
#if LIVE_WEBSOCKET_SEND_TIMING
            if (timing) {
                timing->counters.loop_us += esp_timer_get_time() - loopStart;
                ++timing->counters.loop_calls;
            }
#endif
            if (epoch != session || !_client.tcp) return false;
#if LIVE_WEBSOCKET_SEND_TIMING
            if (timing) ++timing->counters.fragments_attempted;
            // Only the explicit data write receives this result. A heartbeat,
            // pong or close dispatched by loop() remains solely in loop_us.
            const size_t count = writeBounded(&_client, wire, headerSize + length, timing);
#else
            const size_t count = write(&_client, wire, headerSize + length);
#endif
            if (count != headerSize + length || epoch != session) return false;
            offset += length;
#if LIVE_WEBSOCKET_SEND_TIMING
            if (timing) {
                ++timing->counters.fragments_completed;
                timing->counters.completed_payload_bytes += length;
            }
#endif
            if (yieldBetweenFragments && offset < size) {
#if LIVE_WEBSOCKET_SEND_TIMING
                const uint64_t yieldStart = timing ? esp_timer_get_time() : 0;
#endif
                vTaskDelay(1);
#if LIVE_WEBSOCKET_SEND_TIMING
                if (timing) {
                    timing->counters.fragment_yield_us += esp_timer_get_time() - yieldStart;
                    ++timing->counters.fragment_yields;
                }
#endif
            }
        }
#if LIVE_WEBSOCKET_SEND_TIMING
        attempt.delivered();
#endif
        return true;
    }

protected:
    // WebSocketsClient routes handshake, heartbeat, pong and close frames through
    // this virtual hook. Never fall back to WiFiClient::write(), which can block
    // before writeLive's own progress checks get a chance to run.
    size_t write(WSclient_t *client, uint8_t *out, size_t size) override {
        return writeBounded(client, out, size
#if LIVE_WEBSOCKET_SEND_TIMING
                            , nullptr
#endif
                            );
    }

private:
    size_t writeBounded(WSclient_t *client, uint8_t *out, size_t size
#if LIVE_WEBSOCKET_SEND_TIMING
                        , LiveSendTiming *timing
#endif
                        ) {
        if (client != &_client || !client->tcp || !out) return 0;
        const uint32_t session = epoch;
        const int descriptor = client->tcp->fd();
        LiveWriteProgress progress(esp_timer_get_time());
        size_t written = 0;
        while (written < size) {
            if (epoch != session || !client->tcp ||
                client->tcp->fd() != descriptor) return written;
#if LIVE_WEBSOCKET_SEND_TIMING
            const uint64_t sendStart = timing ? esp_timer_get_time() : 0;
#endif
            const int count = ::send(descriptor, out + written, size - written, MSG_DONTWAIT);
            const int sendError = errno;
#if LIVE_WEBSOCKET_SEND_TIMING
            if (timing) {
                timing->counters.send_syscall_us += esp_timer_get_time() - sendStart;
                ++timing->counters.send_calls;
                if (count > 0) timing->counters.wire_bytes += static_cast<size_t>(count);
            }
#endif
            if (count > 0) {
                written += static_cast<size_t>(count);
                const uint64_t nowUs = esp_timer_get_time();
                progress.wroteBytes(nowUs);
                lastProgressMs.store(static_cast<uint32_t>(nowUs / 1000));
            } else if (count < 0 &&
                       (sendError == EAGAIN || sendError == EWOULDBLOCK || sendError == EINTR)) {
#if LIVE_WEBSOCKET_SEND_TIMING
                if (timing) {
                    if (sendError == EINTR) ++timing->counters.interrupted;
                    else ++timing->counters.would_block;
                }
#endif
                if (progress.stalled(esp_timer_get_time())) {
                    ++writeTimeouts;
                    abortWrite("Live write stopped making progress");
                    return written;
                }
#if LIVE_WEBSOCKET_SEND_TIMING
                const uint64_t waitStart = timing ? esp_timer_get_time() : 0;
#endif
                vTaskDelay(1);
#if LIVE_WEBSOCKET_SEND_TIMING
                if (timing) {
                    timing->counters.send_wait_us += esp_timer_get_time() - waitStart;
                    ++timing->counters.send_waits;
                }
#endif
            } else {
                abortWrite("Live write failed");
                return written;
            }
        }
        return written;
    }

    void abortWrite(const char *reason) {
        _client.tcp->stop();
        clientDisconnect(&_client, reason);
    }

    std::atomic<uint32_t> writeTimeouts{0}, lastProgressMs{0};
    uint32_t epoch = 0;
    String clockId;
    uint64_t clockReceived = 0;
};
