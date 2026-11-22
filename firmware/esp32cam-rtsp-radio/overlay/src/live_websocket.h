#pragma once

#include <algorithm>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <lwip/sockets.h>
#include <lwip/opt.h>
#include <errno.h>
#include "live_write_progress.h"

// The Links2004 client owns the handshake, incoming messages and control frames.
// Keep its reader running between outgoing fragments, including when TCP is full.
class LiveWebSocket : public WebSocketsClient {
public:
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
                   bool yieldBetweenFragments = false) {
        const uint32_t session = epoch;
        const size_t maximum = TCP_MSS - WEBSOCKETS_MAX_HEADER_SIZE;
        LiveWriteProgress progress(esp_timer_get_time());
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
            size_t written = 0;
            while (written < headerSize + length) {
                // Control frames are legal between fragments, never inside one.
                if (written == 0) loop();
                if (epoch != session || !_client.tcp) return false;
                const int count = ::send(_client.tcp->fd(), wire + written,
                    headerSize + length - written, MSG_DONTWAIT);
                if (count > 0) {
                    written += static_cast<size_t>(count);
                    progress.wroteBytes(esp_timer_get_time());
                } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                    if (progress.stalled(esp_timer_get_time())) {
                        // A close frame cannot be written into a partial fragment.
                        abortWrite("Live write stopped making progress");
                        return false;
                    }
                    vTaskDelay(1);
                } else {
                    if (epoch == session) abortWrite("Live write failed");
                    return false;
                }
            }
            offset += length;
            if (yieldBetweenFragments && offset < size) vTaskDelay(1);
        }
        return true;
    }

private:
    void abortWrite(const char *reason) {
        _client.tcp->stop();
        clientDisconnect(&_client, reason);
    }

    uint32_t epoch = 0;
    String clockId;
    uint64_t clockReceived = 0;
};
