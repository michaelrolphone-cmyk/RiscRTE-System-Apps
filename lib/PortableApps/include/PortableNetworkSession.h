#ifndef PORTABLE_NETWORK_SESSION_H
#define PORTABLE_NETWORK_SESSION_H
#include "PortableWifiProfiles.h"
#include "WifiApi.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Foreground saved-profile consumer. The caller owns and keeps live its
 * authorized net.wifi@1 instance 15 and storage.key-value@1 namespace 6 grants.
 * No acquisition, grant release, storage writes, synchronous connect, policy
 * defaults, background reconnect, or implicit start occurs here. Slot zero is
 * the explicit network-tool default, not the last selected Settings profile.
 * owner_ok must be a pure cached predicate, with no backend calls or cleanup.
 * A false predicate or retained/ambiguous result permanently fences this
 * session: keep the invocation, grant tables, operation and lease in custody.
 */
#define PORTABLE_NETWORK_DEFAULT_PROFILE 0u
#define PORTABLE_NETWORK_JOIN_TIMEOUT_MS 30000u

typedef enum {
    PORTABLE_NETWORK_IDLE = 0,
    PORTABLE_NETWORK_PENDING = 1,
    PORTABLE_NETWORK_READY = 2,
    PORTABLE_NETWORK_EMPTY = -1,
    PORTABLE_NETWORK_INVALID = -2,
    PORTABLE_NETWORK_UNAVAILABLE = -3,
    PORTABLE_NETWORK_FAILED = -4,
    PORTABLE_NETWORK_RETAINED = -5
} portable_network_result;
typedef enum {
    PORTABLE_NETWORK_CLOSED = 0,
    PORTABLE_NETWORK_CLOSE_PENDING = 1,
    PORTABLE_NETWORK_CLOSE_RETAINED = -1
} portable_network_close_result;
typedef enum {
    PORTABLE_NETWORK_SERVICE_OK = 0,
    PORTABLE_NETWORK_SERVICE_PENDING = 1,
    PORTABLE_NETWORK_SERVICE_RETAINED = -1
} portable_network_service_result;
typedef struct {
    portable_network_result result;
    wifi_link_t link;
    int8_t rssi;
    wifi_ipv4_v1 ipv4;
    bool borrowed, closing, retained;
} portable_network_snapshot;

typedef struct {
    /* Private state, exposed solely for allocation without a heap. Do not
     * mutate/copy a live session or use fields instead of the public API. */
    const wifi_api_v1 *wifi;
    const risc_key_value_v1 *storage;
    const wifi_async_v1 *async;
    bool (*owner_ok)(void *);
    void *owner_context;
    risc_radio_request_v1 request;
    portable_network_snapshot view;
    uint32_t operation, service_lease, service_depth, started_ms;
    unsigned phase;
    bool cancel_accepted, timed_out;
} portable_network_session;

/* Initialize only fresh storage or a session that close confirmed CLOSED.
 * Does not call any backend. The caller must not discard a retained session. */
void portable_network_session_init(portable_network_session *session,
    const wifi_api_v1 *wifi, const risc_key_value_v1 *storage,
    bool (*owner_ok)(void *), void *owner_context);
/* Explicit start merely arms foreground work; subsequent steps are bounded:
 * at most four codec reads, three copied link reads, or one async call. Every
 * backend call has an owner check before and after it. Yield between steps. */
portable_network_result portable_network_session_start(portable_network_session *session, uint32_t now_ms);
portable_network_result portable_network_session_step(portable_network_session *session, uint32_t now_ms);
void portable_network_session_snapshot(const portable_network_session *session, portable_network_snapshot *out);
/* Stop application services first, pair outstanding service_end, then close.
 * Pending is healthy asynchronous cleanup, not a retained failure. Repeat
 * close/step until CLOSED before releasing grants, sleeping or launching.
 * A borrowed healthy connection is left running; only our accepted operation
 * is cancelled. No timeout turns pending native cleanup into success. */
portable_network_close_result portable_network_session_close(portable_network_session *session);
/* Nestable, explicit leases around bounded service/storage work. PENDING
 * authorizes no work; retry at a later foreground tick. End can defer, so keep
 * its lease and retry end (never repeat the protected work) until OK. Close
 * waits for caller-owned lease scopes to end. These also work before start. */
portable_network_service_result portable_network_session_service_begin(portable_network_session *session);
portable_network_service_result portable_network_session_service_end(portable_network_session *session);
#ifdef __cplusplus
}
#endif
#endif
