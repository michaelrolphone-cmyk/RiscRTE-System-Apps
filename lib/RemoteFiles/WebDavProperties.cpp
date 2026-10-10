#include "WebDavProperties.h"
#include <cstring>

namespace RiscWebDav {
namespace {
constexpr char XmlUri[] = "http://www.w3.org/XML/1998/namespace";
constexpr char XmlnsUri[] = "http://www.w3.org/2000/xmlns/";
constexpr size_t AttributesMax = 16, BindingsMax = 32;
struct Span { size_t at = 0, size = 0; };
bool space(uint32_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool xmlChar(uint32_t c) {
    return space(c) || (c >= 0x20 && c <= 0xd7ff) ||
           (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff);
}
bool nameStart(uint32_t c) {
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xf6) ||
           (c >= 0xf8 && c <= 0x2ff) || (c >= 0x370 && c <= 0x37d) ||
           (c >= 0x37f && c <= 0x1fff) || (c >= 0x200c && c <= 0x200d) ||
           (c >= 0x2070 && c <= 0x218f) || (c >= 0x2c00 && c <= 0x2fef) ||
           (c >= 0x3001 && c <= 0xd7ff) || (c >= 0xf900 && c <= 0xfdcf) ||
           (c >= 0xfdf0 && c <= 0xfffd) || (c >= 0x10000 && c <= 0xeffff);
}
bool nameChar(uint32_t c) {
    return nameStart(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') ||
           c == 0xb7 || (c >= 0x300 && c <= 0x36f) || (c >= 0x203f && c <= 0x2040);
}
bool utf8(const char* s, size_t end, size_t& p, uint32_t& c) {
    if (p >= end) return false;
    c = static_cast<unsigned char>(s[p++]);
    if (c < 0x80) return xmlChar(c);
    unsigned n = 0; uint32_t minimum = 0;
    if (c >= 0xc2 && c <= 0xdf) { n = 1; minimum = 0x80; c &= 0x1f; }
    else if (c >= 0xe0 && c <= 0xef) { n = 2; minimum = 0x800; c &= 0xf; }
    else if (c >= 0xf0 && c <= 0xf4) { n = 3; minimum = 0x10000; c &= 7; }
    else return false;
    if (end - p < n) return false;
    while (n--) {
        unsigned char v = static_cast<unsigned char>(s[p++]);
        if ((v & 0xc0) != 0x80) return false;
        c = (c << 6) | (v & 0x3f);
    }
    return c >= minimum && xmlChar(c);
}
size_t encode(uint32_t c, char* out) {
    if (c < 0x80) { out[0] = static_cast<char>(c); return 1; }
    if (c < 0x800) {
        out[0] = static_cast<char>(0xc0 | (c >> 6));
        out[1] = static_cast<char>(0x80 | (c & 0x3f)); return 2;
    }
    if (c < 0x10000) {
        out[0] = static_cast<char>(0xe0 | (c >> 12));
        out[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3f));
        out[2] = static_cast<char>(0x80 | (c & 0x3f)); return 3;
    }
    out[0] = static_cast<char>(0xf0 | (c >> 18));
    out[1] = static_cast<char>(0x80 | ((c >> 12) & 0x3f));
    out[2] = static_cast<char>(0x80 | ((c >> 6) & 0x3f));
    out[3] = static_cast<char>(0x80 | (c & 0x3f)); return 4;
}
bool sameCase(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        const char x = *a >= 'A' && *a <= 'Z' ? *a + ('a' - 'A') : *a;
        const char y = *b >= 'A' && *b <= 'Z' ? *b + ('a' - 'A') : *b;
        if (x != y) return false;
    }
    return !*a && !*b;
}
}
namespace detail {
class PropertyParser {
public:
    PropertyParser(const char* data, size_t size, PropertyRequest& out)
        : data_(data), size_(size), out_(out) {}
    unsigned run();
private:
    enum class Role { Root, All, Names, Prop, Include, Property, Ignore };
    struct Frame { Span name; size_t namespaceMark = 0; Role role = Role::Ignore; };
    struct Attribute { Span name, value; };
    struct Binding { Span prefix, value; };
    const char* data_;
    size_t size_, pos_ = 0, depth_ = 0, bindings_ = 0, occurrences_ = 0;
    PropertyRequest& out_;
    Frame frames_[PropertyXmlDepthMax]{};
    Binding namespaces_[BindingsMax]{};
    unsigned error_ = 400;
    bool rootSeen_ = false, selection_ = false, include_ = false;
    bool limit() { error_ = 413; return false; }
    bool at(const char* literal) const {
        const size_t n = std::strlen(literal);
        return n <= size_ - pos_ && !std::memcmp(data_ + pos_, literal, n);
    }
    bool equals(Span a, const char* b) const {
        return a.size == std::strlen(b) && !std::memcmp(data_ + a.at, b, a.size);
    }
    bool equals(Span a, Span b) const {
        return a.size == b.size && !std::memcmp(data_ + a.at, data_ + b.at, a.size);
    }
    bool spaces() {
        const size_t begin = pos_;
        while (pos_ < size_ && space(static_cast<unsigned char>(data_[pos_]))) ++pos_;
        return pos_ != begin;
    }
    bool name(Span& result);
    void split(Span name, Span& prefix, Span& local) const;
    bool reference(size_t& pos, size_t end, uint32_t& value);
    bool decode(Span value, char* out, size_t capacity, bool attribute, bool whitespace = false);
    bool quoted(Span& value);
    bool resolve(Span name, char* uri, Span& local, bool attribute = false);
    bool add(Span local, const char* uri);
    bool intern(const char* value, size_t size, uint16_t& offset);
    bool open();
    bool close();
    bool comment();
    bool instruction();
    bool declaration();
    bool text(bool cdata = false);
};

bool PropertyParser::name(Span& result) {
    result.at = pos_;
    bool first = true, colon = false;
    while (pos_ < size_) {
        size_t next = pos_; uint32_t c = 0;
        if (!utf8(data_, size_, next, c)) return false;
        if (c == ':') {
            if (first || colon) return false;
            colon = true; first = true; pos_ = next; continue;
        }
        if (!(first ? nameStart(c) : nameChar(c))) break;
        first = false; pos_ = next;
    }
    result.size = pos_ - result.at;
    return !first && result.size;
}
void PropertyParser::split(Span value, Span& prefix, Span& local) const {
    prefix = {}; local = value;
    for (size_t i = 0; i < value.size; ++i) if (data_[value.at + i] == ':') {
        prefix = {value.at, i}; local = {value.at + i + 1, value.size - i - 1}; return;
    }
}
bool PropertyParser::reference(size_t& p, size_t end, uint32_t& value) {
    const size_t start = ++p;
    while (p < end && data_[p] != ';') ++p;
    if (p == end || p == start) return false;
    const Span ref{start, p++ - start};
    if (equals(ref, "amp")) value = '&';
    else if (equals(ref, "lt")) value = '<';
    else if (equals(ref, "gt")) value = '>';
    else if (equals(ref, "quot")) value = '"';
    else if (equals(ref, "apos")) value = '\'';
    else {
        size_t n = ref.at;
        if (data_[n++] != '#') return false;
        unsigned base = 10;
        if (n < ref.at + ref.size && data_[n] == 'x') { ++n; base = 16; }
        if (n == ref.at + ref.size) return false;
        value = 0;
        for (; n < ref.at + ref.size; ++n) {
            const char c = data_[n];
            const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') :
                c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) :
                c >= 'A' && c <= 'F' ? unsigned(c - 'A' + 10) : 16;
            if (digit >= base || value > (0x10ffff - digit) / base) return false;
            value = value * base + digit;
        }
    }
    return xmlChar(value);
}
bool PropertyParser::decode(Span value, char* out, size_t capacity, bool attribute, bool whitespace) {
    size_t p = value.at, used = 0, end = value.at + value.size;
    while (p < end) {
        uint32_t c = 0;
        if (data_[p] == '&') {
            if (!reference(p, end, c)) return false;
        } else {
            if (!utf8(data_, end, p, c) || (attribute && c == '<')) return false;
            if (c == '\r') {
                if (p < end && data_[p] == '\n') ++p;
                c = '\n';
            }
            if (attribute && space(c)) c = ' ';
        }
        if (whitespace && !space(c)) return false;
        if (out) {
            char bytes[4]; const size_t n = encode(c, bytes);
            if (n >= capacity - used) return limit();
            std::memcpy(out + used, bytes, n); used += n;
        }
    }
    if (out) out[used] = 0;
    return true;
}
bool PropertyParser::quoted(Span& value) {
    if (pos_ >= size_ || (data_[pos_] != '\'' && data_[pos_] != '"')) return false;
    const char quote = data_[pos_++]; value.at = pos_;
    while (pos_ < size_ && data_[pos_] != quote) ++pos_;
    if (pos_ == size_) return false;
    value.size = pos_++ - value.at;
    return decode(value, nullptr, 0, true);
}
bool PropertyParser::resolve(Span value, char* uri, Span& local, bool attribute) {
    Span prefix; split(value, prefix, local);
    if (equals(prefix, "xmlns")) return false;
    if (equals(prefix, "xml")) { std::strcpy(uri, XmlUri); return true; }
    if (attribute && !prefix.size) { uri[0] = 0; return true; }
    for (size_t i = bindings_; i; --i) if (equals(namespaces_[i - 1].prefix, prefix))
        return decode(namespaces_[i - 1].value, uri, PropertyNamespaceMax + 1, true);
    uri[0] = 0; return !prefix.size;
}
bool PropertyParser::intern(const char* value, size_t size, uint16_t& offset) {
    for (size_t i = 0; i < out_.used_;) {
        const size_t n = std::strlen(out_.strings_ + i);
        if (size == n && !std::memcmp(out_.strings_ + i, value, n)) { offset = i; return true; }
        i += n + 1;
    }
    if (size + 1 > sizeof(out_.strings_) - out_.used_) return limit();
    offset = out_.used_;
    std::memcpy(out_.strings_ + out_.used_, value, size);
    out_.used_ += size + 1;
    return true;
}
bool PropertyParser::add(Span local, const char* uri) {
    if (++occurrences_ > PropertyCountMax || local.size > PropertyLocalNameMax) return limit();
    for (size_t i = 0; i < out_.count; ++i)
        if (!std::strcmp(out_.namespaceUri(i), uri) &&
            std::strlen(out_.localName(i)) == local.size &&
            !std::memcmp(out_.localName(i), data_ + local.at, local.size)) return true;
    uint16_t uriOffset = 0, localOffset = 0;
    if (!intern(uri, std::strlen(uri), uriOffset) ||
        !intern(data_ + local.at, local.size, localOffset)) return false;
    auto& entry = out_.names_[out_.count++]; entry.uri = uriOffset; entry.local = localOffset;
    return true;
}

bool PropertyParser::open() {
    if (depth_ == PropertyXmlDepthMax) return limit();
    if (!depth_ && rootSeen_) return false;
    ++pos_;
    Span element;
    if (!name(element)) return false;
    Attribute attrs[AttributesMax]{}; size_t count = 0;
    bool empty = false;
    for (;;) {
        const bool separated = spaces();
        if (at("/>")) { pos_ += 2; empty = true; break; }
        if (at(">")) { ++pos_; break; }
        if (!separated) return false;
        if (count == AttributesMax) return limit();
        auto& attr = attrs[count];
        if (!name(attr.name)) return false;
        for (size_t i = 0; i < count; ++i) if (equals(attrs[i].name, attr.name)) return false;
        spaces(); if (!at("=")) return false; ++pos_; spaces();
        if (!quoted(attr.value)) return false;
        ++count;
    }
    const size_t mark = bindings_;
    char uri[PropertyNamespaceMax + 1]; Span local;
    for (size_t i = 0; i < count; ++i) {
        Span prefix; split(attrs[i].name, prefix, local);
        if (!equals(attrs[i].name, "xmlns") && !equals(prefix, "xmlns")) continue;
        const Span boundPrefix = prefix.size ? local : Span{};
        if (!decode(attrs[i].value, uri, sizeof(uri), true)) return false;
        if (equals(boundPrefix, "xmlns") || !std::strcmp(uri, XmlnsUri) ||
            (equals(boundPrefix, "xml") != (!std::strcmp(uri, XmlUri))) ||
            (boundPrefix.size && !uri[0])) return false;
        if (bindings_ == BindingsMax) return limit();
        namespaces_[bindings_++] = {boundPrefix, attrs[i].value};
    }
    // Namespace declarations apply to every name in the same start tag,
    // irrespective of attribute order. Duplicate expanded attributes are invalid.
    for (size_t i = 0; i < count; ++i) {
        Span prefix; split(attrs[i].name, prefix, local);
        if (equals(attrs[i].name, "xmlns") || equals(prefix, "xmlns")) continue;
        if (!resolve(attrs[i].name, uri, local, true)) return false;
        for (size_t j = 0; j < i; ++j) {
            Span previousPrefix, previousLocal; split(attrs[j].name, previousPrefix, previousLocal);
            if (equals(attrs[j].name, "xmlns") || equals(previousPrefix, "xmlns") ||
                !equals(local, previousLocal)) continue;
            char previousUri[PropertyNamespaceMax + 1];
            if (!resolve(attrs[j].name, previousUri, previousLocal, true) ||
                !std::strcmp(uri, previousUri)) return false;
        }
    }
    if (!resolve(element, uri, local)) return false;
    const bool dav = !std::strcmp(uri, "DAV:");
    Role role = Role::Ignore;
    if (!depth_) {
        if (!dav || !equals(local, "propfind")) return false;
        rootSeen_ = true; role = Role::Root;
    } else {
        const Role parent = frames_[depth_ - 1].role;
        if (parent == Role::Root && dav) {
            if (equals(local, "allprop") || equals(local, "propname") || equals(local, "prop")) {
                if (selection_) return false;
                selection_ = true;
                if (equals(local, "allprop")) { role = Role::All; out_.mode = PropertyMode::All; }
                else if (equals(local, "propname")) { role = Role::Names; out_.mode = PropertyMode::Names; }
                else { role = Role::Prop; out_.mode = PropertyMode::Named; }
            } else if (equals(local, "include")) {
                if (include_) return false;
                include_ = true; role = Role::Include;
            }
        } else if (parent == Role::Prop || parent == Role::Include) {
            role = Role::Property;
            if (!add(local, uri)) return false;
        } else if (parent == Role::Property) return false; // A request cannot set property values.
    }
    frames_[depth_++] = {element, mark, role};
    if (empty) { --depth_; bindings_ = mark; }
    return true;
}
bool PropertyParser::close() {
    if (!depth_) return false;
    pos_ += 2; Span element;
    if (!name(element) || !equals(element, frames_[depth_ - 1].name)) return false;
    spaces(); if (!at(">")) return false; ++pos_;
    bindings_ = frames_[--depth_].namespaceMark;
    return true;
}
bool PropertyParser::comment() {
    pos_ += 4;
    while (pos_ < size_) {
        if (at("-->")) { pos_ += 3; return true; }
        if (at("--")) return false;
        ++pos_;
    }
    return false;
}
bool PropertyParser::instruction() {
    pos_ += 2; Span target;
    if (!name(target)) return false;
    if (target.size == 3) {
        char word[4]{}; std::memcpy(word, data_ + target.at, 3);
        if (sameCase(word, "xml")) return false;
    }
    if (!at("?>") && !spaces()) return false;
    while (pos_ < size_ && !at("?>")) ++pos_;
    if (!at("?>")) return false;
    pos_ += 2; return true;
}
bool PropertyParser::declaration() {
    pos_ += 5;
    unsigned stage = 0;
    for (;;) {
        const bool separated = spaces();
        if (at("?>")) { pos_ += 2; return stage != 0; }
        if (!separated) return false;
        Span key, value;
        if (!name(key)) return false;
        spaces(); if (!at("=")) return false; ++pos_; spaces();
        if (!quoted(value)) return false;
        if (!stage && equals(key, "version")) {
            if (!equals(value, "1.0")) { error_ = 415; return false; }
            stage = 1;
        } else if (stage == 1 && equals(key, "encoding")) {
            char encoding[32];
            if (value.size >= sizeof(encoding)) { error_ = 415; return false; }
            std::memcpy(encoding, data_ + value.at, value.size); encoding[value.size] = 0;
            if (!sameCase(encoding, "UTF-8") && !sameCase(encoding, "US-ASCII")) {
                error_ = 415; return false;
            }
            if (sameCase(encoding, "US-ASCII")) for (size_t i = 0; i < size_; ++i)
                if (static_cast<unsigned char>(data_[i]) >= 128) return false;
            stage = 2;
        } else if (stage < 3 && stage && equals(key, "standalone")) {
            if (!equals(value, "yes") && !equals(value, "no")) return false;
            stage = 3;
        } else return false;
    }
}
bool PropertyParser::text(bool cdata) {
    if (cdata && !depth_) return false;
    if (cdata) pos_ += 9;
    const size_t begin = pos_;
    while (pos_ < size_) {
        if (at("]]>")) { if (!cdata) return false; break; }
        if (!cdata && data_[pos_] == '<') break;
        ++pos_;
    }
    if (cdata && !at("]]>")) return false;
    const bool whitespace = !depth_ || frames_[depth_ - 1].role != Role::Ignore;
    if (!cdata && depth_) {
        if (!decode({begin, pos_ - begin}, nullptr, 0, false, whitespace)) return false;
    } else if (whitespace) {
        for (size_t i = begin; i < pos_; ++i)
            if (!space(static_cast<unsigned char>(data_[i]))) return false;
    }
    if (cdata) pos_ += 3;
    return true;
}
unsigned PropertyParser::run() {
    if (size_ >= 2 && ((static_cast<unsigned char>(data_[0]) == 0xff && static_cast<unsigned char>(data_[1]) == 0xfe) ||
        (static_cast<unsigned char>(data_[0]) == 0xfe && static_cast<unsigned char>(data_[1]) == 0xff))) return 415;
    for (size_t p = 0; p < size_;) { uint32_t c; if (!utf8(data_, size_, p, c)) return 400; }
    if (at("\xef\xbb\xbf")) pos_ += 3;
    if (at("<?xml") && pos_ + 5 < size_ && space(static_cast<unsigned char>(data_[pos_ + 5])))
        if (!declaration()) return error_;
    while (pos_ < size_) {
        bool ok;
        if (at("<!--")) ok = comment();
        else if (at("<?")) ok = instruction();
        else if (at("<![CDATA[")) ok = text(true);
        else if (at("<!")) return 400; // Reject every DTD/declaration before any expansion.
        else if (at("</")) ok = close();
        else if (at("<")) ok = open();
        else ok = text();
        if (!ok) return error_;
    }
    return !depth_ && rootSeen_ && selection_ && (!include_ || out_.mode == PropertyMode::All) ? 0 : 400;
}
}
unsigned parseProperties(const char* data, size_t size, PropertyRequest& out) {
    out = {};
    if (size > PropertyXmlMax) return 413;
    if (!size) return 0;
    if (!data) return 400;
    detail::PropertyParser parser(data, size, out);
    const unsigned status = parser.run();
    if (status) out = {};
    return status;
}
}
