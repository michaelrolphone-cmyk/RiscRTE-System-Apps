#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace WatchUpdate {
constexpr size_t kIndependentCatalogMaxApps=128;
class WorkBudget {
 public:
  WorkBudget(bool (*callback)(void*), void* context)
      : callback_(callback), context_(context) {}
  bool poll(bool force = false) {
    if (!alive_) return false;
    const bool due = force || bytes_ >= 1024;
    if (due) {
      bytes_ = 0;
      if (callback_ && !callback_(context_)) alive_ = false;
    }
    return alive_;
  }
  bool byte() {
    if (!alive_) return false;
    ++bytes_;
    return bytes_ < 1024 || poll();
  }
  bool alive() const { return alive_; }
 private:
  bool (*callback_)(void*);
  void* context_;
  size_t bytes_ = 0;
  bool alive_ = true;
};

// A bounded JSON cursor. Keys are unescaped ASCII and duplicate-checked;
// ignored descriptive values may contain valid JSON escapes and UTF-8.
// Identity-bearing values use text(), which admits only unescaped ASCII.
class Cursor {
 public:
  Cursor(const char* data, size_t length, WorkBudget& budget)
      : data_(data), length_(length), budget_(budget) {}
  void ws() {
    while (position_ < length_ && budget_.alive()) {
      const char c = data_[position_];
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
      advance();
    }
  }
  bool take(char c) {
    ws();
    if (!budget_.alive() || position_ == length_ || data_[position_] != c) return false;
    return advance();
  }
  bool next(char close, bool& more) {
    if (take(close)) { more = false; return true; }
    if (take(',')) { more = true; return true; }
    return false;
  }
  bool end() { ws(); return budget_.alive() && position_ == length_; }
  bool text(char* out, size_t capacity) {
    if (!out || capacity < 2 || !take('"')) return false;
    size_t used = 0;
    while (position_ < length_) {
      const unsigned char c = static_cast<unsigned char>(data_[position_]);
      if (!advance()) return false;
      if (c == '"') { out[used] = 0; return used != 0; }
      if (c < 0x20 || c > 0x7e || c == '\\' || used + 1 >= capacity) return false;
      out[used++] = static_cast<char>(c);
    }
    return false;
  }
  bool key(char* out, size_t capacity) { return text(out, capacity) && take(':'); }
  bool integer(uint64_t& value) {
    ws();
    if (position_ == length_ || data_[position_] < '0' || data_[position_] > '9') return false;
    if (data_[position_] == '0' && position_ + 1 < length_ &&
        data_[position_ + 1] >= '0' && data_[position_ + 1] <= '9') return false;
    value = 0;
    do {
      const uint64_t digit = static_cast<uint64_t>(data_[position_] - '0');
      if (value > (UINT64_MAX - digit) / 10u) return false;
      value = value * 10u + digit;
      if (!advance()) return false;
    } while (position_ < length_ && data_[position_] >= '0' && data_[position_] <= '9');
    return true;
  }
  bool nullValue() { ws(); return literal("null"); }
  bool objectAhead() { ws(); return position_ < length_ && data_[position_] == '{'; }
  bool slice(const char*& start, size_t& length) {
    ws();
    const size_t at = position_;
    if (!value()) return false;
    start = data_ + at;
    length = position_ - at;
    return true;
  }
  bool skip() { return value(); }
  bool objectOnly() { return objectAhead() && value() && end(); }
 private:
  struct Key { size_t offset; size_t length; };
  static constexpr size_t kMaxDepth = 10;
  static constexpr size_t kMaxKeys = 64;
  const char* data_;
  size_t length_;
  WorkBudget& budget_;
  size_t position_ = 0;
  size_t depth_ = 0;
  size_t activeKeys_ = 0;
  Key keys_[kMaxKeys]{};

  bool advance() { ++position_; return budget_.byte(); }
  bool rawTake(char c) {
    if (position_ == length_ || data_[position_] != c || !budget_.alive()) return false;
    return advance();
  }
  bool literal(const char* text) {
    const size_t n = std::strlen(text);
    if (n > length_ - position_ || std::memcmp(data_ + position_, text, n)) return false;
    for (size_t i = 0; i < n; ++i) if (!advance()) return false;
    return true;
  }
  bool hex4(uint16_t& result) {
    result = 0;
    for (unsigned i = 0; i < 4; ++i) {
      if (position_ == length_) return false;
      const char c = data_[position_];
      const int digit = c >= '0' && c <= '9' ? c - '0' :
                        c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                        c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (digit < 0 || !advance()) return false;
      result = static_cast<uint16_t>((result << 4) | digit);
    }
    return true;
  }
  bool utf8(unsigned char first) {
    unsigned remaining = 0;
    uint32_t cp = 0, minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { remaining = 1; cp = first & 31u; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { remaining = 2; cp = first & 15u; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; cp = first & 7u; minimum = 0x10000; }
    else return false;
    for (unsigned i = 0; i < remaining; ++i) {
      if (position_ == length_) return false;
      const unsigned char next = static_cast<unsigned char>(data_[position_]);
      if (next < 0x80 || next > 0xbf || !advance()) return false;
      cp = (cp << 6) | (next & 63u);
    }
    return cp >= minimum && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
  }
  bool string(bool key, size_t& offset, size_t& length) {
    if (!rawTake('"')) return false;
    offset = position_;
    while (position_ < length_) {
      const unsigned char c = static_cast<unsigned char>(data_[position_]);
      if (!advance()) return false;
      if (c == '"') {
        length = position_ - offset - 1;
        return !key || (length && length <= 63);
      }
      if (c < 0x20 || (key && c >= 0x7f)) return false;
      if (c >= 0x80) { if (!utf8(c)) return false; continue; }
      if (c != '\\') continue;
      if (key || position_ == length_) return false;
      const char escaped = data_[position_];
      if (!advance()) return false;
      if (escaped == '"' || escaped == '\\' || escaped == '/' || escaped == 'b' ||
          escaped == 'f' || escaped == 'n' || escaped == 'r' || escaped == 't') continue;
      uint16_t first = 0, second = 0;
      if (escaped != 'u' || !hex4(first) || (first >= 0xdc00 && first <= 0xdfff)) return false;
      if (first >= 0xd800 && first <= 0xdbff &&
          (!rawTake('\\') || !rawTake('u') || !hex4(second) ||
           second < 0xdc00 || second > 0xdfff)) return false;
    }
    return false;
  }
  bool digits() {
    const size_t start = position_;
    while (position_ < length_ && data_[position_] >= '0' && data_[position_] <= '9')
      if (!advance()) return false;
    return position_ != start;
  }
  bool number() {
    (void)rawTake('-');
    if (!rawTake('0')) {
      if (position_ == length_ || data_[position_] < '1' || data_[position_] > '9' || !digits()) return false;
    }
    if (rawTake('.') && !digits()) return false;
    if (position_ < length_ && (data_[position_] == 'e' || data_[position_] == 'E')) {
      if (!advance()) return false;
      if (position_ < length_ && (data_[position_] == '+' || data_[position_] == '-'))
        if (!advance()) return false;
      if (!digits()) return false;
    }
    return budget_.alive();
  }
  bool value() {
    ws();
    if (position_ == length_ || !budget_.alive()) return false;
    size_t offset = 0, length = 0;
    switch (data_[position_]) {
      case '{': return object();
      case '[': return array();
      case '"': return string(false, offset, length);
      case 't': return literal("true");
      case 'f': return literal("false");
      case 'n': return literal("null");
      default: return number();
    }
  }
  bool object() {
    if (depth_ >= kMaxDepth || !take('{')) return false;
    ++depth_;
    const size_t base = activeKeys_;
    if (take('}')) { --depth_; return true; }
    for (;;) {
      ws();
      size_t offset = 0, length = 0;
      if (!string(true, offset, length)) return false;
      for (size_t i = base; i < activeKeys_; ++i)
        if (keys_[i].length == length &&
            !std::memcmp(data_ + keys_[i].offset, data_ + offset, length)) return false;
      if (activeKeys_ == kMaxKeys) return false;
      keys_[activeKeys_++] = {offset, length};
      if (!take(':') || !value()) return false;
      if (take('}')) { activeKeys_ = base; --depth_; return true; }
      if (!take(',')) return false;
    }
  }
  bool array() {
    if (depth_ >= kMaxDepth || !take('[')) return false;
    ++depth_;
    if (take(']')) { --depth_; return true; }
    size_t count = 0;
    for (;;) {
      if (++count > kIndependentCatalogMaxApps || !value()) return false;
      if (take(']')) { --depth_; return true; }
      if (!take(',')) return false;
    }
  }
};

}
