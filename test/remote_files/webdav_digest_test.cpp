#include "WebDavDigest.h"
#include "vendor/tinydtls_sha2/sha2.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

using namespace RiscWebDav;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)

struct Vector {
    const char *username, *password, *realm, *nonceHex, *method, *uri, *authorization, *bodyHex;
    bool integrity;
};
#include "webdav_digest_vectors.inc"

std::vector<unsigned char> unhex(const char* text) {
    std::vector<unsigned char> out;
    while (*text) {
        char pair[3] = {*text++, *text++, 0};
        out.push_back(static_cast<unsigned char>(std::strtoul(pair, nullptr, 16)));
    }
    return out;
}
bool zero(const void* data, size_t size) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) if (p[i]) return false;
    return true;
}
void begin(DigestSession& session, const Vector& v, uint64_t now = 10, uint64_t expiry = 1000) {
    std::vector<unsigned char> password(v.password, v.password + std::strlen(v.password));
    auto nonce = unhex(v.nonceHex);
    CHECK(session.begin(v.username, password.data(), password.size(), v.realm, nonce.data(), nonce.size(), now, expiry));
    CHECK(zero(password.data(), password.size()));
}
DigestResult verify(DigestSession& session, const Vector& v, DigestPhase phase,
                    const char* authorization = nullptr, uint64_t now = 20) {
    auto body = unhex(v.bodyHex);
    return session.verify(v.method, v.uri, authorization ? authorization : v.authorization,
                          body.data(), body.size(), now, phase);
}
std::string replaceOnce(std::string input, const std::string& old, const std::string& replacement) {
    size_t where = input.find(old); CHECK(where != std::string::npos);
    input.replace(where, old.size(), replacement);
    return input;
}

void independentVectors() {
    for (const auto& v : vectors) {
        DigestSession session;
        begin(session, v);
        CHECK(verify(session, v, DigestPhase::HeaderPrecheck) ==
              (v.integrity ? DigestResult::BodyRequired : DigestResult::Authorized));
        CHECK(verify(session, v, DigestPhase::HeaderPrecheck) ==
              (v.integrity ? DigestResult::BodyRequired : DigestResult::Authorized));
        CHECK(verify(session, v, DigestPhase::Final) == DigestResult::Authorized);
        CHECK(verify(session, v, DigestPhase::Final) == DigestResult::Replay);
        CHECK(verify(session, v, DigestPhase::HeaderPrecheck) == DigestResult::Replay);
    }
}

void malformedAndBinding() {
    const Vector& v = vectors[0];
    DigestSession session;
    begin(session, v);
    const std::string original = v.authorization;
    std::vector<std::string> malformed = {
        "", "Basic Zm9vOmJhcg==", "Digest", "Digest ", "Digest x", "Digest username",
        "Digest username=", "Digest username=\"", "Digest username=\"x\\",
        original + ",", original + ", nc=00000002", original + ", NC=00000002",
        original + ", username*=UTF-8''Mufasa", original + ", userhash=true",
        original + ", extension=anything", original + "\r\nX-Injected: yes",
        replaceOnce(original, "SHA-256", "MD5"), replaceOnce(original, "SHA-256", "SHA-256-sess"),
        replaceOnce(original, "qop=auth", "qop=bad"),
        replaceOnce(original, "nc=00000001", "nc=00000000"),
        replaceOnce(original, "nc=00000001", "nc=1"),
        replaceOnce(original, "nc=00000001", "nc=0000000g"),
        replaceOnce(original, "nc=00000001", "nc=000000001"),
        replaceOnce(original, "cnonce=\"", "cnonce=\"" + std::string(DigestCnonceMax, 'x')),
        std::string(DigestAuthorizationMax + 1, 'x')
    };
    for (const auto& auth : malformed)
        CHECK(verify(session, v, DigestPhase::Final, auth.c_str()) == DigestResult::Malformed);
    std::vector<std::string> unauthorized = {
        replaceOnce(original, "Mufasa", "mufasa"),
        replaceOnce(original, "http-auth@example.org", "other@example.org"),
        replaceOnce(original, "7ypf/", "8ypf/"),
        replaceOnce(original, "/dir/index.html", "/dir/other.html"),
        replaceOnce(original, "753927", "653927")
    };
    for (const auto& auth : unauthorized)
        CHECK(verify(session, v, DigestPhase::Final, auth.c_str()) == DigestResult::Unauthorized);
    CHECK(session.verify("HEAD", v.uri, v.authorization, nullptr, 0, 20, DigestPhase::Final) == DigestResult::Unauthorized);
    CHECK(session.verify("GET", "/dir/index%2ehtml", v.authorization, nullptr, 0, 20, DigestPhase::Final) == DigestResult::Unauthorized);
    CHECK(session.verify("BAD METHOD", v.uri, v.authorization, nullptr, 0, 20, DigestPhase::Final) == DigestResult::Malformed);
    CHECK(session.verify("GET", "/dir/a#fragment", v.authorization, nullptr, 0, 20, DigestPhase::Final) == DigestResult::Malformed);
    CHECK(session.verify("GET", v.uri, v.authorization, nullptr, 1, 20, DigestPhase::Final) == DigestResult::Malformed);
    CHECK(session.verify("GET", v.uri, v.authorization, "x", DigestBodyMax + 1, 20, DigestPhase::Final) == DigestResult::Malformed);
    CHECK(session.verify(nullptr, v.uri, v.authorization, nullptr, 0, 20, DigestPhase::Final) == DigestResult::Malformed);
    CHECK(session.verify("GET", v.uri, v.authorization, nullptr, 0, 20, static_cast<DigestPhase>(99)) == DigestResult::Malformed);
    // All the failures left nc=1 available, and case-insensitive field names are accepted.
    std::string mixed = replaceOnce(original, "Digest", "dIgEsT");
    mixed = replaceOnce(mixed, "username=", "USERNAME = ");
    mixed = replaceOnce(mixed, "SHA-256", "sha-256");
    mixed += ", userhash=false";
    CHECK(verify(session, v, DigestPhase::Final, mixed.c_str()) == DigestResult::Authorized);
}

void parserBoundaries() {
    const Vector& v = vectors[0];
    DigestSession session;
    begin(session, v);
    const std::string full = v.authorization;
    // Every truncation before the response's closing quote must fail safely.
    for (size_t i = 0; i < full.size(); ++i) {
        std::string shortened = full.substr(0, i);
        CHECK(verify(session, v, DigestPhase::HeaderPrecheck, shortened.c_str()) != DigestResult::Authorized);
    }
    // Controls are rejected at every possible position, even inside quotes.
    for (size_t i = 0; i < full.size(); ++i) {
        for (unsigned char control : {1, 13, 10, 127, 255}) {
            std::string altered = full; altered[i] = static_cast<char>(control);
            CHECK(verify(session, v, DigestPhase::Final, altered.c_str()) == DigestResult::Malformed);
        }
    }
    // Case-insensitive duplicate detection must cover every required directive.
    for (const char* directive : {"USERNAME", "REALM", "NONCE", "URI", "RESPONSE", "ALGORITHM", "QOP", "NC", "CNONCE"}) {
        std::string duplicate = full + ", " + directive + "=x";
        CHECK(verify(session, v, DigestPhase::Final, duplicate.c_str()) == DigestResult::Malformed);
    }
    // Quoted token directives are allowed by HTTP auth parsing; the exact nc
    // text remains part of the digest calculation after quoted-pair decoding.
    std::string quoted = replaceOnce(full, "algorithm=SHA-256", "algorithm=\"SHA-256\"");
    quoted = replaceOnce(quoted, "nc=00000001", "nc=\"00000001\"");
    quoted = replaceOnce(quoted, "qop=auth", "qop=\"auth\"");
    CHECK(verify(session, v, DigestPhase::Final, quoted.c_str()) == DigestResult::Authorized);
}

void integrityAndCounts() {
    const Vector& v = integrityVector;
    DigestSession session;
    begin(session, v);
    CHECK(verify(session, v, DigestPhase::HeaderPrecheck) == DigestResult::BodyRequired);
    auto body = unhex(v.bodyHex);
    CHECK(!body.empty()); body[0] ^= 1;
    CHECK(session.verify(v.method, v.uri, v.authorization, body.data(), body.size(), 20, DigestPhase::Final) == DigestResult::Unauthorized);
    CHECK(session.verify(v.method, v.uri, v.authorization, nullptr, 0, 20, DigestPhase::Final) == DigestResult::Unauthorized);
    CHECK(verify(session, v, DigestPhase::Final) == DigestResult::Authorized);

    begin(session, countVectors[0]);
    CHECK(verify(session, countVectors[0], DigestPhase::Final) == DigestResult::Authorized); // 1
    CHECK(verify(session, countVectors[2], DigestPhase::HeaderPrecheck) == DigestResult::Authorized); // 3
    CHECK(verify(session, countVectors[1], DigestPhase::Final) == DigestResult::Authorized); // 2
    CHECK(verify(session, countVectors[2], DigestPhase::Final) == DigestResult::Authorized); // 3
    CHECK(verify(session, countVectors[1], DigestPhase::Final) == DigestResult::Replay);
    CHECK(verify(session, countVectors[3], DigestPhase::Final) == DigestResult::Authorized); // ffffffff
    CHECK(verify(session, countVectors[2], DigestPhase::Final) == DigestResult::Replay);
    CHECK(verify(session, countVectors[4], DigestPhase::Final) == DigestResult::Authorized); // new cnonce

    begin(session, capacityVectors[0]);
    // Prechecks and failed final digests must not allocate a replay slot.
    for (const auto& item : capacityVectors) {
        CHECK(verify(session, item, DigestPhase::HeaderPrecheck) == DigestResult::Authorized);
        std::string bad(item.authorization);
        size_t i = bad.find("response=\"") + std::strlen("response=\"");
        bad[i] = bad[i] == '0' ? '1' : '0';
        CHECK(verify(session, item, DigestPhase::Final, bad.c_str()) == DigestResult::Unauthorized);
    }
    for (size_t i = 0; i < DigestReplaySlots; ++i)
        CHECK(verify(session, capacityVectors[i], DigestPhase::Final) == DigestResult::Authorized);
    CHECK(verify(session, capacityVectors[DigestReplaySlots], DigestPhase::Final) == DigestResult::ReplayCapacity);
    CHECK(verify(session, capacityVectors[0], DigestPhase::Final) == DigestResult::Replay);
    CHECK(verify(session, capacityNext, DigestPhase::Final) == DigestResult::Authorized);
}

void lifecycle() {
    const Vector& v = vectors[0];
    DigestSession session;
    CHECK(verify(session, v, DigestPhase::Final) == DigestResult::Inactive);
    begin(session, v);
    char challenge[DigestChallengeCapacity];
    CHECK(session.challenge(challenge, sizeof(challenge), 20));
    CHECK(std::strstr(challenge, "algorithm=SHA-256"));
    CHECK(std::strstr(challenge, "qop=\"auth,auth-int\""));
    CHECK(std::strstr(challenge, "nonce=\"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v\""));
    CHECK(!session.challenge(challenge, 1, 20) && !challenge[0]);
    CHECK(verify(session, v, DigestPhase::Final, nullptr, 999) == DigestResult::Authorized);
    CHECK(verify(session, v, DigestPhase::Final, nullptr, 1000) == DigestResult::Expired);
    CHECK(!session.active());
    CHECK(!session.challenge(challenge, sizeof(challenge), 1000) && !challenge[0]);
    begin(session, v);
    CHECK(session.challenge(challenge, sizeof(challenge), 30));
    CHECK(verify(session, v, DigestPhase::Final, nullptr, 29) == DigestResult::Expired);
    begin(session, v); session.clear();
    CHECK(verify(session, v, DigestPhase::Final) == DigestResult::Inactive);
    auto nonce = unhex(v.nonceHex);
    for (int invalid = 0; invalid != 6; ++invalid) {
        begin(session, v);
        unsigned char password[6] = {'s','e','c','r','e','t'};
        CHECK(!session.begin(invalid == 0 ? "bad:username" : v.username,
                             password, sizeof(password), invalid == 1 ? "bad\"realm" : v.realm,
                             invalid == 2 ? nullptr : nonce.data(), invalid == 3 ? 15 : nonce.size(),
                             10, invalid == 4 ? 10 : (invalid == 5 ? UINT64_MAX : 1000)));
        CHECK(zero(password, sizeof(password)) && !session.active());
    }
    // Inspect inactive storage for retained HA1/password/nonce/replay data, including destruction.
    alignas(DigestSession) unsigned char storage[sizeof(DigestSession)]{};
    auto* placed = new(storage) DigestSession;
    begin(*placed, v); CHECK(verify(*placed, v, DigestPhase::Final) == DigestResult::Authorized);
    placed->clear(); CHECK(zero(storage, sizeof(storage)));
    begin(*placed, v); placed->~DigestSession(); CHECK(zero(storage, sizeof(storage)));
}

void hashes() {
    for (const auto& v : hashVectors) {
        auto data = unhex(v.inputHex);
        dtls_sha256_ctx context;
        dtls_sha256_init(&context);
        // Exercise unaligned and multi-update input as well as padding boundaries.
        for (size_t i = 0; i < data.size();) {
            size_t count = std::min<size_t>(13, data.size() - i);
            dtls_sha256_update(&context, data.data() + i, count); i += count;
        }
        unsigned char actual[32]; dtls_sha256_final(actual, &context);
        auto expected = unhex(v.expectedHex);
        CHECK(expected.size() == sizeof(actual) && !std::memcmp(expected.data(), actual, sizeof(actual)));
        CHECK(zero(&context, sizeof(context)));
    }
}

int main() {
    hashes(); independentVectors(); malformedAndBinding(); parserBoundaries(); integrityAndCounts(); lifecycle();
    std::printf("WebDAV SHA-256 Digest PASS: RFC vector, independent hashes, auth/auth-int, parser, replay, expiry, wipe (%zu vectors)\n", sizeof(vectors) / sizeof(vectors[0]));
}
