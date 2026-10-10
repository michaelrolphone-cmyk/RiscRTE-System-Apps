#pragma once
#include <cstddef>
#include <cstdint>

namespace RiscWebDav {
constexpr size_t DigestAuthorizationMax = 1536;
constexpr size_t DigestTargetMax = 768;
constexpr size_t DigestUsernameMax = 64;
constexpr size_t DigestRealmMax = 64;
constexpr size_t DigestPasswordMax = 256;
constexpr size_t DigestCnonceMax = 96;
constexpr size_t DigestReplaySlots = 8;
constexpr size_t DigestBodyMax = 65536;
constexpr size_t DigestChallengeCapacity = 256;

enum class DigestPhase { HeaderPrecheck, Final };
enum class DigestResult {
    Authorized, BodyRequired, Unauthorized, Malformed, Expired,
    Replay, ReplayCapacity, Inactive
};

/* One foreground-owned, finite authentication session. RFC 7616 SHA-256,
 * qop=auth/auth-int; no MD5, legacy no-qop, userhash or SHA-256-sess fallback.
 * No I/O, allocation, entropy generation, persistence or background work.
 * Not thread-safe: serialize begin/challenge/verify/clear with request handling.
 * Digest authenticates requests; it does not encrypt their contents.
 */
class DigestSession final {
public:
    DigestSession() = default;
    ~DigestSession();
    DigestSession(const DigestSession&) = delete;
    DigestSession& operator=(const DigestSession&) = delete;

    /* The caller must supply 16..48 fresh cryptographically random nonce bytes
     * per session and an absolute finite expiry in the same monotonic units as
     * now. Never reuse a nonce with the same credentials after clear/restart.
     * Password is a mutable readable/writable span of 1..DigestPasswordMax
     * bytes. That span is wiped on success AND failure; an invalid oversized
     * span is rejected without accessing it. No plaintext password is copied.
     * Credentials are caller-normalized octets; username and realm are bounded
     * printable ASCII. Realm excludes quote/backslash; username excludes colon.
     * Invalid initialization clears any previous authentication session.
     */
    bool begin(const char* username, void* password, size_t passwordSize,
               const char* realm, const void* nonceBytes, size_t nonceSize,
               uint64_t now, uint64_t expiry);

    /* Writes only a WWW-Authenticate field VALUE. Returns false, empties out
     * when possible, and wipes the session on expiry or backward clock motion.
     */
    bool challenge(char* out, size_t capacity, uint64_t now);

    /* Pass the exact undecoded original request-target, including percent
     * escapes, and exact method token. Text inputs must be NUL terminated
     * within their documented bounds. bodySize is bounded to DigestBodyMax.
     * HeaderPrecheck never consumes a count or reserves a replay slot. auth-int
     * returns BodyRequired after structural/nonce checks: this is NOT final
     * authorization. Final must receive the complete entity body and must
     * succeed before storage access/effects; only it consumes the nonce count.
     * Replay slots never evict a live cnonce; exhaustion requires a fresh
     * explicitly initialized session. Failed verification changes no counts.
     */
    DigestResult verify(const char* method, const char* requestTarget,
                        const char* authorization, const void* body,
                        size_t bodySize, uint64_t now, DigestPhase phase);
    void clear();
    bool active() const { return active_; }

private:
    struct ReplayEntry { char cnonce[DigestCnonceMax + 1]{}; uint32_t count = 0; };
    unsigned char ha1_[32]{}, usernameHash_[32]{};
    char realm_[DigestRealmMax + 1]{}, nonce_[65]{};
    ReplayEntry replay_[DigestReplaySlots]{};
    uint64_t lastNow_ = 0, expiry_ = 0;
    bool active_ = false;
    bool live(uint64_t now);
};
}
