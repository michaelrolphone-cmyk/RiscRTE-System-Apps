#include "PortableNetworkSession.h"

/* No scheduler, sleeps or loops over backend calls. */
enum { SESSION_IDLE, SESSION_INSPECT, SESSION_LOAD, SESSION_SUBMIT,
       SESSION_OWNED, SESSION_BORROWED, SESSION_CLOSING, SESSION_ENDED };

static portable_network_result retain(portable_network_session *s) {
    portable_wifi_credentials_wipe(&s->request, sizeof(s->request));
    s->view.retained = true;
    s->view.result = PORTABLE_NETWORK_RETAINED;
    return s->view.result;
}
static bool owner(portable_network_session *s) {
    if (!s || s->view.retained) return false;
    if (!s->owner_ok || !s->owner_ok(s->owner_context)) {
        retain(s); return false;
    }
    return true;
}
static bool api(portable_network_session *s) {
    if (!owner(s)) return false;
    const wifi_api_v1 *w = s->wifi;
    if (!w || w->api_version != WIFI_API_V1 || w->struct_size < WIFI_ASYNC_V1_SIZE ||
        !w->status || !w->rssi || !w->addresses) return false;
    const wifi_async_v1 *a = (const wifi_async_v1 *)w;
    if (a->async_tag != RISC_RADIO_ASYNC_TAG || a->async_version != RISC_RADIO_ASYNC_VERSION ||
        !a->begin || !a->poll || !a->cancel || !a->service_begin || !a->service_end) return false;
    s->async = a; return true;
}
static portable_network_result finish(portable_network_session *s, portable_network_result result) {
    portable_wifi_credentials_wipe(&s->request, sizeof(s->request));
    s->view.result = result; s->phase = SESSION_ENDED;
    return result;
}
static bool transient(int rc) { return rc == RISC_RADIO_BUSY || rc == RISC_RADIO_AGAIN; }
static bool address_present(const wifi_ipv4_v1 *ip) {
    return ip->address[0] || ip->address[1] || ip->address[2] || ip->address[3];
}
static int32_t guarded_get(void *context, const char *key, void *data, uint32_t capacity, uint32_t *size) {
    portable_network_session *s = context;
    if (size) *size = 0;
    if (!owner(s)) return RISC_KEY_VALUE_CONTEXT;
    int32_t rc = s->storage->get(s->storage->context, key, data, capacity, size);
    if (!owner(s) || rc == RISC_KEY_VALUE_CONTEXT) {
        retain(s); return RISC_KEY_VALUE_CONTEXT;
    }
    return rc;
}
static int32_t no_write(void *context, const char *key, const void *data, uint32_t size) {
    (void)context; (void)key; (void)data; (void)size;
    return RISC_KEY_VALUE_INVALID;
}

void portable_network_session_init(portable_network_session *s, const wifi_api_v1 *w,
        const risc_key_value_v1 *kv, bool (*owner_ok)(void *), void *context) {
    if (!s) return;
    memset(s, 0, sizeof(*s)); s->wifi = w; s->storage = kv;
    s->owner_ok = owner_ok; s->owner_context = context;
}
portable_network_result portable_network_session_start(portable_network_session *s, uint32_t now) {
    if (!s) return PORTABLE_NETWORK_INVALID;
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    if (s->phase != SESSION_IDLE && s->phase != SESSION_ENDED) return s->view.result;
    if (s->operation || s->service_depth || s->service_lease) return s->view.result;
    if (!api(s)) return s->view.retained ? PORTABLE_NETWORK_RETAINED : finish(s, PORTABLE_NETWORK_UNAVAILABLE);
    memset(&s->view, 0, sizeof(s->view));
    s->started_ms = now; s->timed_out = s->cancel_accepted = false;
    s->phase = SESSION_INSPECT; return s->view.result = PORTABLE_NETWORK_PENDING;
}

/* Status/address/RSSI are copied reads. Never mistake UP alone for a usable IP.
 * A previously borrowed link is never reconnected after it disappears. */
static portable_network_result inspect(portable_network_session *s) {
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    wifi_link_t link = s->wifi->status(s->wifi->context);
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    if (link != WIFI_LINK_DOWN && link != WIFI_LINK_JOINING && link != WIFI_LINK_UP) return retain(s);
    s->view.link = link;
    if (link != WIFI_LINK_UP) {
        memset(&s->view.ipv4, 0, sizeof(s->view.ipv4));
        s->view.rssi = 0;
        if (s->phase == SESSION_BORROWED) return finish(s, PORTABLE_NETWORK_FAILED);
        if (link == WIFI_LINK_DOWN) s->phase = s->request.kind ? SESSION_SUBMIT : SESSION_LOAD;
        return s->view.result = PORTABLE_NETWORK_PENDING;
    }
    wifi_ipv4_v1 ip = {{0}, {0}, {0}}, ap = {{0}, {0}, {0}};
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    bool present = s->wifi->addresses(s->wifi->context, &ip, &ap);
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    if (!present || !address_present(&ip)) {
        memset(&s->view.ipv4, 0, sizeof(s->view.ipv4));
        return s->view.result = PORTABLE_NETWORK_PENDING;
    }
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    int8_t rssi = s->wifi->rssi(s->wifi->context);
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    portable_wifi_credentials_wipe(&s->request, sizeof(s->request));
    s->view.ipv4 = ip; s->view.rssi = rssi; s->view.borrowed = true;
    s->phase = SESSION_BORROWED; return s->view.result = PORTABLE_NETWORK_READY;
}
static bool progress_valid(const portable_network_session *s, int rc, const risc_radio_progress_v1 *p) {
    if ((rc != RISC_RADIO_PENDING && rc != RISC_RADIO_QUIESCENT) ||
        p->struct_size != sizeof(*p) || p->operation_id != s->operation ||
        p->phase < RISC_RADIO_QUEUED || p->phase > RISC_RADIO_IDLE ||
        p->phase == RISC_RADIO_SCANNING || p->phase == RISC_RADIO_RESULTS ||
        p->failure > RISC_RADIO_FAILURE_CANCELLED || p->owned > 1 || p->quiescent > 1 ||
        p->station_state > WIFI_LINK_UP || p->quiescent != (p->phase == RISC_RADIO_IDLE) ||
        p->owned == p->quiescent || ((rc == RISC_RADIO_QUIESCENT) != (p->quiescent != 0)) ||
        (p->phase == RISC_RADIO_CONNECTED && p->station_state != WIFI_LINK_UP) ||
        p->reserved[0] || p->reserved[1] || p->reserved[2] || p->reserved[3]) return false;
    const garden_radio_scan_result_v1 *scan = &p->scan;
    if (scan->struct_size != sizeof(*scan) || scan->count > GARDEN_RADIO_SCAN_MAX ||
        scan->state > GARDEN_RADIO_SCAN_FAILED || scan->reserved) return false;
    for (unsigned i = 0; i < scan->count; ++i) {
        if (scan->entries[i].reserved || !memchr(scan->entries[i].ssid, 0, sizeof(scan->entries[i].ssid))) return false;
    }
    return true;
}
static portable_network_result poll_operation(portable_network_session *s) {
    risc_radio_progress_v1 p = {.struct_size = sizeof(p)};
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    int rc = s->async->poll(s->wifi->context, s->operation, &p);
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    if (transient(rc)) return s->view.result;
    if (!progress_valid(s, rc, &p)) return retain(s);
    s->view.link = (wifi_link_t)p.station_state; s->view.rssi = p.rssi;
    memcpy(&s->view.ipv4, p.station, sizeof(s->view.ipv4));
    if (p.quiescent) {
        s->operation = 0; s->cancel_accepted = false;
        s->view.link = WIFI_LINK_DOWN; s->view.rssi = 0;
        memset(&s->view.ipv4, 0, sizeof(s->view.ipv4));
        if (s->phase == SESSION_CLOSING) {
            s->view.closing = false; s->phase = SESSION_IDLE;
            return s->view.result = s->timed_out ? PORTABLE_NETWORK_FAILED : PORTABLE_NETWORK_IDLE;
        }
        return finish(s, PORTABLE_NETWORK_FAILED);
    }
    if (s->phase == SESSION_CLOSING) return s->view.result = PORTABLE_NETWORK_PENDING;
    return s->view.result = p.phase == RISC_RADIO_CONNECTED && address_present(&s->view.ipv4)
        ? PORTABLE_NETWORK_READY : PORTABLE_NETWORK_PENDING;
}
portable_network_result portable_network_session_step(portable_network_session *s, uint32_t now) {
    if (!s) return PORTABLE_NETWORK_INVALID;
    if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
    if (s->phase == SESSION_CLOSING) {
        (void)portable_network_session_close(s); return s->view.result;
    }
    if (s->phase == SESSION_IDLE || s->phase == SESSION_ENDED) return s->view.result;
    if (s->service_depth || s->service_lease) return s->view.result;
    if (s->view.result != PORTABLE_NETWORK_READY && (uint32_t)(now - s->started_ms) >= PORTABLE_NETWORK_JOIN_TIMEOUT_MS) {
        s->timed_out = true;
        if (s->operation) { (void)portable_network_session_close(s); return s->view.result; }
        return finish(s, PORTABLE_NETWORK_FAILED);
    }
    if (s->phase == SESSION_INSPECT || s->phase == SESSION_BORROWED) return inspect(s);
    if (s->phase == SESSION_LOAD) {
        if (!portable_wifi_credentials_api_valid(s->storage)) return finish(s, PORTABLE_NETWORK_UNAVAILABLE);
        portable_wifi_credentials credentials = {{0}, {0}};
        const risc_key_value_v1 guarded = {RISC_KEY_VALUE_API_V1, sizeof(guarded), s, guarded_get, no_write};
        int rc = portable_wifi_profile_load(&guarded, PORTABLE_NETWORK_DEFAULT_PROFILE, &credentials);
        if (s->view.retained) { portable_wifi_credentials_clear(&credentials); return PORTABLE_NETWORK_RETAINED; }
        if (rc == PORTABLE_WIFI_CREDENTIALS_LOADED) {
            s->request = (risc_radio_request_v1){.struct_size = sizeof(s->request), .kind = RISC_RADIO_REQUEST_JOIN};
            memcpy(s->request.ssid, credentials.ssid, sizeof(s->request.ssid));
            memcpy(s->request.password, credentials.password, sizeof(s->request.password));
            /* Recheck for somebody else's healthy/joining link before begin. */
            s->phase = SESSION_INSPECT;
        }
        portable_wifi_credentials_clear(&credentials);
        if (rc == PORTABLE_WIFI_CREDENTIALS_AGAIN || rc == PORTABLE_WIFI_CREDENTIALS_LOADED) return PORTABLE_NETWORK_PENDING;
        return finish(s, rc == PORTABLE_WIFI_CREDENTIALS_EMPTY ? PORTABLE_NETWORK_EMPTY :
            rc == PORTABLE_WIFI_CREDENTIALS_INVALID ? PORTABLE_NETWORK_INVALID : PORTABLE_NETWORK_UNAVAILABLE);
    }
    if (s->phase == SESSION_SUBMIT) {
        uint32_t id = 0;
        if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
        int rc = s->async->begin(s->wifi->context, &s->request, &id);
        /* Preserve even an unexpected returned ID before the post-call fence. */
        if (id) s->operation = id;
        if (!owner(s)) return PORTABLE_NETWORK_RETAINED;
        if (transient(rc) && !id) { s->phase = SESSION_INSPECT; return PORTABLE_NETWORK_PENDING; }
        portable_wifi_credentials_wipe(&s->request, sizeof(s->request));
        if (rc == RISC_RADIO_ACCEPTED && id) { s->phase = SESSION_OWNED; return PORTABLE_NETWORK_PENDING; }
        /* The request was validated by the codec and constructed here. INVALID
         * cannot establish a valid provider/owner context at this boundary. */
        if (id || rc != RISC_RADIO_UNAVAILABLE) return retain(s);
        return finish(s, PORTABLE_NETWORK_UNAVAILABLE);
    }
    return poll_operation(s);
}
portable_network_close_result portable_network_session_close(portable_network_session *s) {
    if (!s || !owner(s)) return PORTABLE_NETWORK_CLOSE_RETAINED;
    portable_wifi_credentials_wipe(&s->request, sizeof(s->request));
    s->view.closing = true; s->phase = SESSION_CLOSING;
    s->view.result = PORTABLE_NETWORK_PENDING;
    if (s->service_depth || s->service_lease) return PORTABLE_NETWORK_CLOSE_PENDING;
    if (!s->operation) {
        s->phase = SESSION_IDLE; s->view.closing = false; s->view.borrowed = false;
        s->view.link = WIFI_LINK_DOWN; s->view.rssi = 0;
        memset(&s->view.ipv4, 0, sizeof(s->view.ipv4));
        s->view.result = s->timed_out ? PORTABLE_NETWORK_FAILED : PORTABLE_NETWORK_IDLE;
        return PORTABLE_NETWORK_CLOSED;
    }
    if (s->cancel_accepted) {
        (void)poll_operation(s);
        return s->view.retained ? PORTABLE_NETWORK_CLOSE_RETAINED :
            s->operation ? PORTABLE_NETWORK_CLOSE_PENDING : PORTABLE_NETWORK_CLOSED;
    }
    if (!owner(s)) return PORTABLE_NETWORK_CLOSE_RETAINED;
    int rc = s->async->cancel(s->wifi->context, s->operation);
    if (!owner(s)) return PORTABLE_NETWORK_CLOSE_RETAINED;
    if (rc == RISC_RADIO_PENDING || rc == RISC_RADIO_QUIESCENT) s->cancel_accepted = true;
    else if (!transient(rc)) { retain(s); return PORTABLE_NETWORK_CLOSE_RETAINED; }
    /* Even QUIESCENT must be corroborated by a checked copied terminal poll. */
    return PORTABLE_NETWORK_CLOSE_PENDING;
}
portable_network_service_result portable_network_session_service_begin(portable_network_session *s) {
    if (!s || !owner(s)) return PORTABLE_NETWORK_SERVICE_RETAINED;
    if (s->view.closing) return PORTABLE_NETWORK_SERVICE_PENDING;
    if (s->service_depth) {
        if (s->service_depth == UINT32_MAX) { retain(s); return PORTABLE_NETWORK_SERVICE_RETAINED; }
        ++s->service_depth; return PORTABLE_NETWORK_SERVICE_OK;
    }
    if (!api(s)) {
        if (!s->view.retained) retain(s);
        return PORTABLE_NETWORK_SERVICE_RETAINED;
    }
    uint32_t lease = 0;
    if (!owner(s)) return PORTABLE_NETWORK_SERVICE_RETAINED;
    int rc = s->async->service_begin(s->wifi->context, &lease);
    if (lease) s->service_lease = lease;
    if (!owner(s)) return PORTABLE_NETWORK_SERVICE_RETAINED;
    if (transient(rc) && !lease) return PORTABLE_NETWORK_SERVICE_PENDING;
    if (rc != RISC_RADIO_ACCEPTED || !lease) { retain(s); return PORTABLE_NETWORK_SERVICE_RETAINED; }
    s->service_depth = 1; return PORTABLE_NETWORK_SERVICE_OK;
}
portable_network_service_result portable_network_session_service_end(portable_network_session *s) {
    if (!s || !owner(s)) return PORTABLE_NETWORK_SERVICE_RETAINED;
    if (!s->service_depth || !s->service_lease) { retain(s); return PORTABLE_NETWORK_SERVICE_RETAINED; }
    if (s->service_depth > 1) { --s->service_depth; return PORTABLE_NETWORK_SERVICE_OK; }
    if (!owner(s)) return PORTABLE_NETWORK_SERVICE_RETAINED;
    int rc = s->async->service_end(s->wifi->context, s->service_lease);
    if (!owner(s)) return PORTABLE_NETWORK_SERVICE_RETAINED;
    if (transient(rc)) return PORTABLE_NETWORK_SERVICE_PENDING;
    if (rc != RISC_RADIO_ACCEPTED) { retain(s); return PORTABLE_NETWORK_SERVICE_RETAINED; }
    s->service_depth = s->service_lease = 0; return PORTABLE_NETWORK_SERVICE_OK;
}
void portable_network_session_snapshot(const portable_network_session *s, portable_network_snapshot *out) {
    if (!out) return;
    if (s) *out = s->view;
    else { memset(out, 0, sizeof(*out)); out->result = PORTABLE_NETWORK_INVALID; }
}
