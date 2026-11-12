#pragma once
#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace camera_light_model {
struct Rgb { uint8_t red, green, blue; };
struct State {
    bool on = false;
    uint8_t brightness = 128;
    Rgb rgb = {255, 255, 255};
};

inline bool same(const State &a, const State &b) {
    return a.on == b.on && a.brightness == b.brightness &&
           a.rgb.red == b.rgb.red && a.rgb.green == b.rgb.green && a.rgb.blue == b.rgb.blue;
}

inline uint8_t scale(uint8_t color, uint8_t brightness) {
    return static_cast<uint8_t>((static_cast<uint16_t>(color) * brightness + 127u) / 255u);
}

inline Rgb transmittedRgb(const State &state) {
    if (!state.on) return {0, 0, 0};
    return {scale(state.rgb.red, state.brightness), scale(state.rgb.green, state.brightness),
            scale(state.rgb.blue, state.brightness)};
}

// Reader position lets us reject trailing non-JSON content instead of silently
// accepting only the first object. ArduinoJson otherwise stops at its closing brace.
struct JsonReader {
    const char *data;
    size_t size;
    size_t position = 0;
    JsonReader(const char *text, size_t length) : data(text), size(length) {}
    int read() {
        return position < size ? static_cast<unsigned char>(data[position++]) : -1;
    }
    size_t readBytes(char *buffer, size_t count) {
        size_t copied = 0;
        while (copied < count && position < size) buffer[copied++] = data[position++];
        return copied;
    }
};

inline bool byteValue(JsonVariantConst value, uint8_t &output) {
    if (!value.is<unsigned int>()) return false;
    const unsigned int number = value.as<unsigned int>();
    if (number > 255) return false;
    output = static_cast<uint8_t>(number);
    return true;
}

inline bool parseUpdate(const char *body, size_t length, const State &current,
                        State &updated, const char *&error) {
    error = nullptr;
    if (!body || length == 0 || length > 256) {
        error = "Provide a JSON object of 1 to 256 bytes";
        return false;
    }
    // ArduinoJson stores object keys as C strings. Reject NUL before parsing
    // so an escaped key such as "on\\u0000ignored" cannot alias the valid "on".
    for (size_t i = 0; i < length; ++i) {
        if (body[i] == '\0' || (i + 6 <= length && !std::memcmp(body + i, "\\u0000", 6))) {
            error = "Embedded NUL is not allowed in light JSON";
            return false;
        }
    }
    StaticJsonDocument<512> document;
    JsonReader reader(body, length);
    if (deserializeJson(document, reader) || !document.is<JsonObject>()) {
        error = "Invalid light JSON object";
        return false;
    }
    for (size_t i = reader.position; i < length; ++i) {
        if (body[i] != ' ' && body[i] != '\t' && body[i] != '\r' && body[i] != '\n') {
            error = "Unexpected content after light JSON object";
            return false;
        }
    }
    JsonObjectConst object = document.as<JsonObjectConst>();
    if (object.size() == 0) {
        error = "Provide on, brightness, or rgb";
        return false;
    }
    State candidate = current;
    for (JsonPairConst item : object) {
        const JsonString key = item.key();
        const JsonVariantConst value = item.value();
        if (key.size() == 2 && !std::memcmp(key.c_str(), "on", 2)) {
            if (!value.is<bool>()) { error = "on must be a boolean"; return false; }
            candidate.on = value.as<bool>();
        } else if (key.size() == 10 && !std::memcmp(key.c_str(), "brightness", 10)) {
            if (!byteValue(value, candidate.brightness)) {
                error = "brightness must be an integer from 0 to 255";
                return false;
            }
        } else if (key.size() == 3 && !std::memcmp(key.c_str(), "rgb", 3)) {
            if (!value.is<JsonArrayConst>() || value.as<JsonArrayConst>().size() != 3) {
                error = "rgb must contain exactly three integers from 0 to 255";
                return false;
            }
            const JsonArrayConst rgb = value.as<JsonArrayConst>();
            if (!byteValue(rgb[0], candidate.rgb.red) ||
                !byteValue(rgb[1], candidate.rgb.green) ||
                !byteValue(rgb[2], candidate.rgb.blue)) {
                error = "rgb must contain exactly three integers from 0 to 255";
                return false;
            }
        } else {
            error = "Only on, brightness, and rgb may be changed";
            return false;
        }
    }
    updated = candidate; // publish only after the entire request is valid
    return true;
}

struct Pulse {
    uint16_t duration0 = 0;
    uint16_t duration1 = 0;
    bool level0 = false;
    bool level1 = false;
};
constexpr size_t FRAME_ITEMS = 25;

inline uint32_t ticksForNs(uint32_t clockHz, uint32_t nanoseconds) {
    return static_cast<uint32_t>((static_cast<uint64_t>(clockHz) * nanoseconds +
                                 500000000ULL) / 1000000000ULL);
}

// One WS2812 pixel, GRB, MSB first. Both bit values last 1.25 microseconds:
// zero 350ns high + 900ns low; one 850ns high + 400ns low.
// A 300us low latch/reset follows the data. It does not turn the light off.
inline bool waveform(const State &state, uint32_t clockHz, Pulse output[FRAME_ITEMS]) {
    const uint32_t zeroHigh = ticksForNs(clockHz, 350);
    const uint32_t zeroLow = ticksForNs(clockHz, 900);
    const uint32_t oneHigh = ticksForNs(clockHz, 850);
    const uint32_t oneLow = ticksForNs(clockHz, 400);
    const uint32_t resetHalf = ticksForNs(clockHz, 150000);
    const uint32_t durations[] = {zeroHigh, zeroLow, oneHigh, oneLow, resetHalf};
    for (uint32_t duration : durations)
        if (duration == 0 || duration > 32767) return false;
    const Rgb rgb = transmittedRgb(state);
    const uint8_t wire[] = {rgb.green, rgb.red, rgb.blue};
    size_t index = 0;
    for (uint8_t channel : wire) {
        for (int bit = 7; bit >= 0; --bit) {
            const bool one = (channel & (1u << bit)) != 0;
            output[index].duration0 = static_cast<uint16_t>(one ? oneHigh : zeroHigh);
            output[index].duration1 = static_cast<uint16_t>(one ? oneLow : zeroLow);
            output[index].level0 = true;
            output[index].level1 = false;
            ++index;
        }
    }
    output[index].duration0 = static_cast<uint16_t>(resetHalf);
    output[index].duration1 = static_cast<uint16_t>(resetHalf);
    output[index].level0 = false;
    output[index].level1 = false;
    return true;
}
} // namespace camera_light_model
