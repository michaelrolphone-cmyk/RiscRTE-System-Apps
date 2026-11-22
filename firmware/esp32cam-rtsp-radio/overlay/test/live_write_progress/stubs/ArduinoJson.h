#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Only the JSON/String surface used by live_websocket.h is needed here. The
// regression exercises its real send(), socket lifecycle and virtual dispatch.
class String {
public:
    String(const char *value = "") : text(value) {}
    bool isEmpty() const { return text.empty(); }
    bool operator==(const String &other) const { return text == other.text; }
    bool operator!=(const String &other) const { return !(*this == other); }
private:
    std::string text;
};
struct JsonVariantConst {
    template <typename T> bool is() const { return true; }
};
struct JsonSlot {
    template <typename T> JsonSlot &operator=(const T &) { return *this; }
};
template <size_t Capacity> struct StaticJsonDocument {
    JsonSlot operator[](const char *) { return {}; }
};
inline String serialized(const String &value) { return value; }
inline void serializeJson(JsonVariantConst, String &out) { out = String("1"); }
template <size_t Capacity>
size_t serializeJson(const StaticJsonDocument<Capacity> &, char *out, size_t size) {
    if (size < 2) return 0;
    out[0] = '{'; out[1] = '}';
    return 2;
}
