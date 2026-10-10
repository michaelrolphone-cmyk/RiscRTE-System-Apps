#include "WebDavDigest.h"
#include "WebDavBounds.h"
#include "vendor/tinydtls_sha2/sha2.h"
#include <cstdio>
#include <cstring>

namespace RiscWebDav {
namespace {
void wipe(void* data, size_t size) {
    volatile unsigned char* p = static_cast<volatile unsigned char*>(data);
    while (size--) *p++ = 0;
}
struct WipeGuard {
    void* data; size_t size;
    ~WipeGuard() { if (data) wipe(data, size); }
};
class Hash final {
public:
    Hash() { dtls_sha256_init(&context_); }
    ~Hash() { wipe(&context_, sizeof(context_)); }
    void add(const void* data, size_t size) {
        if (size) dtls_sha256_update(&context_, static_cast<const uint8_t*>(data), size);
    }
    void text(const char* s) { add(s, std::strlen(s)); }
    void final(unsigned char result[32]) { dtls_sha256_final(result, &context_); }
private:
    dtls_sha256_ctx context_{};
};
void hex(const unsigned char input[32], char out[65]) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i) {
        out[2 * i] = digits[input[i] >> 4];
        out[2 * i + 1] = digits[input[i] & 15];
    }
    out[64] = 0;
}
int hexDigit(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = detail::asciiLower(c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
bool equalDigest(const unsigned char* a, const unsigned char* b) {
    unsigned int difference = 0;
    for (size_t i = 0; i < 32; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}
bool same(const char* a, const char* b) {
    while (*a && *b && detail::asciiLower(*a) == detail::asciiLower(*b)) { ++a; ++b; }
    return !*a && !*b;
}
bool token(unsigned char c) {
    return detail::asciiAlphaNumeric(c) || (c && std::strchr("!#$%&'*+-.^_`|~", c));
}
bool boundedText(const char* s, size_t max, size_t& size) {
    if (!s) return false;
    size = detail::boundedLength(s, max + 1);
    return size && size <= max;
}
bool credentialText(const char* s, size_t max, bool realm, size_t& size) {
    if (!boundedText(s, max, size)) return false;
    for (size_t i = 0; i < size; ++i) {
        const unsigned char c = s[i];
        if (c < 32 || c > 126 || (realm ? (c == '"' || c == '\\') : c == ':')) return false;
    }
    return true;
}
void base64(const unsigned char* input, size_t size, char* out) {
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t j = 0;
    for (size_t i = 0; i < size; i += 3) {
        const unsigned int value = (unsigned(input[i]) << 16) |
            (i + 1 < size ? unsigned(input[i + 1]) << 8 : 0) |
            (i + 2 < size ? unsigned(input[i + 2]) : 0);
        out[j++] = digits[(value >> 18) & 63]; out[j++] = digits[(value >> 12) & 63];
        out[j++] = i + 1 < size ? digits[(value >> 6) & 63] : '=';
        out[j++] = i + 2 < size ? digits[value & 63] : '=';
    }
    out[j] = 0;
}
struct Fields {
    char username[DigestUsernameMax + 1]{}, realm[DigestRealmMax + 1]{};
    char nonce[65]{}, uri[DigestTargetMax + 1]{}, response[65]{};
    char algorithm[16]{}, qop[9]{}, nc[9]{}, cnonce[DigestCnonceMax + 1]{};
    char userhash[6]{};
    uint16_t seen = 0;
};
bool field(Fields& fields, const char* key, char*& destination, size_t& capacity, uint16_t& bit) {
#define DIGEST_FIELD(name, index) \
    if (same(key, #name)) { destination = fields.name; capacity = sizeof(fields.name); bit = 1u << index; return true; }
    DIGEST_FIELD(username, 0) DIGEST_FIELD(realm, 1) DIGEST_FIELD(nonce, 2)
    DIGEST_FIELD(uri, 3) DIGEST_FIELD(response, 4) DIGEST_FIELD(algorithm, 5)
    DIGEST_FIELD(qop, 6) DIGEST_FIELD(nc, 7) DIGEST_FIELD(cnonce, 8)
    DIGEST_FIELD(userhash, 9)
#undef DIGEST_FIELD
    return false; // No extension directives were advertised.
}
bool parse(const char* input, Fields& result) {
    size_t size = 0;
    if (!boundedText(input, DigestAuthorizationMax, size)) return false;
    for (size_t i = 0; i < size; ++i)
        if ((static_cast<unsigned char>(input[i]) < 32 && input[i] != '\t') ||
            static_cast<unsigned char>(input[i]) >= 127) return false;
    const char* p = input;
    while (*p == ' ' || *p == '\t') ++p;
    char scheme[7]{};
    size_t n = 0;
    while (token(*p)) { if (n == sizeof(scheme) - 1) return false; scheme[n++] = *p++; }
    if (!same(scheme, "Digest") || (*p != ' ' && *p != '\t')) return false;
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return false;
    while (*p) {
        char key[16]{}; n = 0;
        while (token(*p)) { if (n == sizeof(key) - 1) return false; key[n++] = *p++; }
        if (!n) return false;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p++ != '=') return false;
        while (*p == ' ' || *p == '\t') ++p;
        char* out = nullptr; size_t capacity = 0; uint16_t bit = 0;
        if (!field(result, key, out, capacity, bit) || (result.seen & bit)) return false;
        result.seen |= bit;
        n = 0;
        if (*p == '"') {
            ++p;
            while (*p && *p != '"') {
                if (*p == '\\') { ++p; if (!*p) return false; }
                if (static_cast<unsigned char>(*p) < 32 || n + 1 >= capacity) return false;
                out[n++] = *p++;
            }
            if (*p != '"') return false;
            ++p;
        } else {
            while (token(*p)) { if (n + 1 >= capacity) return false; out[n++] = *p++; }
        }
        if (!n) return false;
        out[n] = 0;
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        if (*p++ != ',') return false;
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) return false;
    }
    return (result.seen & 0x1ffu) == 0x1ffu &&
        (!(result.seen & 0x200u) || same(result.userhash, "false"));
}
}

DigestSession::~DigestSession() { clear(); }
void DigestSession::clear() {
    wipe(ha1_, sizeof(ha1_)); wipe(usernameHash_, sizeof(usernameHash_));
    wipe(realm_, sizeof(realm_)); wipe(nonce_, sizeof(nonce_)); wipe(replay_, sizeof(replay_));
    lastNow_ = expiry_ = 0; active_ = false;
}
bool DigestSession::live(uint64_t now) {
    if (!active_) return false;
    if (now < lastNow_ || now >= expiry_) { clear(); return false; }
    lastNow_ = now;
    return true;
}
bool DigestSession::begin(const char* username, void* password, size_t passwordSize,
                          const char* realm, const void* nonceBytes, size_t nonceSize,
                          uint64_t now, uint64_t expiry) {
    clear();
    if (!password || passwordSize > DigestPasswordMax) return false;
    WipeGuard passwordGuard{password, passwordSize};
    size_t usernameSize = 0, realmSize = 0;
    if (!passwordSize || !credentialText(username, DigestUsernameMax, false, usernameSize) ||
        !credentialText(realm, DigestRealmMax, true, realmSize) || !nonceBytes ||
        nonceSize < 16 || nonceSize > 48 || expiry <= now || expiry == UINT64_MAX) return false;
    Hash a1;
    a1.add(username, usernameSize); a1.text(":"); a1.add(realm, realmSize);
    a1.text(":"); a1.add(password, passwordSize); a1.final(ha1_);
    Hash identity; identity.add(username, usernameSize); identity.final(usernameHash_);
    std::memcpy(realm_, realm, realmSize + 1);
    base64(static_cast<const unsigned char*>(nonceBytes), nonceSize, nonce_);
    lastNow_ = now; expiry_ = expiry; active_ = true;
    return true;
}
bool DigestSession::challenge(char* out, size_t capacity, uint64_t now) {
    if (out && capacity) out[0] = 0;
    if (!live(now) || !out || !capacity) return false;
    const int size = std::snprintf(out, capacity,
        "Digest realm=\"%s\", nonce=\"%s\", algorithm=SHA-256, qop=\"auth,auth-int\"", realm_, nonce_);
    if (size < 0 || static_cast<size_t>(size) >= capacity) { out[0] = 0; return false; }
    return true;
}
DigestResult DigestSession::verify(const char* method, const char* requestTarget,
                                  const char* authorization, const void* body,
                                  size_t bodySize, uint64_t now, DigestPhase phase) {
    if (!active_) return DigestResult::Inactive;
    if (!live(now)) return DigestResult::Expired;
    size_t methodSize = 0, targetSize = 0;
    if ((phase != DigestPhase::HeaderPrecheck && phase != DigestPhase::Final) ||
        !boundedText(method, 15, methodSize) ||
        !boundedText(requestTarget, DigestTargetMax, targetSize) ||
        bodySize > DigestBodyMax || (!body && bodySize)) return DigestResult::Malformed;
    for (size_t i = 0; i < methodSize; ++i) if (!token(method[i])) return DigestResult::Malformed;
    for (size_t i = 0; i < targetSize; ++i) {
        const unsigned char c = requestTarget[i];
        if (c <= 32 || c >= 127 || c == '#') return DigestResult::Malformed;
    }
    if (requestTarget[0] != '/' && std::strcmp(requestTarget, "*")) return DigestResult::Malformed;
    Fields fields;
    WipeGuard fieldsGuard{&fields, sizeof(fields)};
    if (!parse(authorization, fields) || !same(fields.algorithm, "SHA-256") ||
        (std::strcmp(fields.qop, "auth") && std::strcmp(fields.qop, "auth-int")) ||
        std::strlen(fields.nc) != 8 || std::strlen(fields.response) != 64) return DigestResult::Malformed;
    uint32_t nc = 0;
    for (size_t i = 0; i < 8; ++i) {
        const int digit = hexDigit(fields.nc[i]);
        if (digit < 0) return DigestResult::Malformed;
        nc = (nc << 4) | static_cast<uint32_t>(digit);
    }
    if (!nc) return DigestResult::Malformed;
    unsigned char supplied[32]{}, identity[32]{};
    WipeGuard suppliedGuard{supplied, sizeof(supplied)}, identityGuard{identity, sizeof(identity)};
    for (size_t i = 0; i < 32; ++i) {
        const int a = hexDigit(fields.response[2 * i]), b = hexDigit(fields.response[2 * i + 1]);
        if (a < 0 || b < 0) return DigestResult::Malformed;
        supplied[i] = static_cast<unsigned char>((a << 4) | b);
    }
    Hash username; username.text(fields.username); username.final(identity);
    if (!equalDigest(identity, usernameHash_) || std::strcmp(fields.realm, realm_) ||
        std::strcmp(fields.nonce, nonce_) || std::strcmp(fields.uri, requestTarget)) return DigestResult::Unauthorized;
    size_t slot = DigestReplaySlots, empty = DigestReplaySlots;
    for (size_t i = 0; i < DigestReplaySlots; ++i) {
        if (!replay_[i].count) { if (empty == DigestReplaySlots) empty = i; }
        else if (!std::strcmp(replay_[i].cnonce, fields.cnonce)) slot = i;
    }
    if (slot != DigestReplaySlots && nc <= replay_[slot].count) return DigestResult::Replay;
    if (slot == DigestReplaySlots) slot = empty;
    if (slot == DigestReplaySlots) return DigestResult::ReplayCapacity;
    const bool integrity = !std::strcmp(fields.qop, "auth-int");
    if (integrity && phase == DigestPhase::HeaderPrecheck) return DigestResult::BodyRequired;

    unsigned char value[32]{};
    char a1hex[65]{}, a2hex[65]{}, bodyhex[65]{};
    WipeGuard valueGuard{value, sizeof(value)}, a1Guard{a1hex, sizeof(a1hex)};
    WipeGuard a2Guard{a2hex, sizeof(a2hex)}, bodyGuard{bodyhex, sizeof(bodyhex)};
    Hash a2; a2.add(method, methodSize); a2.text(":"); a2.add(requestTarget, targetSize);
    if (integrity) {
        Hash entity; entity.add(body, bodySize); entity.final(value); hex(value, bodyhex);
        a2.text(":"); a2.text(bodyhex);
    }
    a2.final(value); hex(value, a2hex); hex(ha1_, a1hex);
    Hash response;
    response.text(a1hex); response.text(":"); response.text(nonce_); response.text(":");
    response.text(fields.nc); response.text(":"); response.text(fields.cnonce); response.text(":");
    response.text(fields.qop); response.text(":"); response.text(a2hex); response.final(value);
    if (!equalDigest(value, supplied)) return DigestResult::Unauthorized;
    if (phase == DigestPhase::Final) {
        std::memcpy(replay_[slot].cnonce, fields.cnonce, std::strlen(fields.cnonce) + 1);
        replay_[slot].count = nc;
    }
    return DigestResult::Authorized;
}
}
