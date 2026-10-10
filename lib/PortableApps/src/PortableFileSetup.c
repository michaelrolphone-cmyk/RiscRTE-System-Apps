#include "PortableFileSetup.h"
#include <string.h>

enum { PHASE_IDLE, PHASE_ACQUIRE, PHASE_OPEN, PHASE_ACTIVE, PHASE_CLOSE, PHASE_RELEASE };

static void wipe(void *memory, size_t size) {
    volatile unsigned char *p = memory;
    while (size--) *p++ = 0;
}
static bool empty_grant(const risc_runtime_capability_v1 *g) {
    return g->struct_size == sizeof(*g) && !g->slot && !g->generation && !g->api;
}
static bool valid_grant(const risc_runtime_capability_v1 *g) {
    return g->struct_size == sizeof(*g) && g->slot && g->generation && g->api;
}
static void clear_comparison(portable_file_setup *s) {
    s->view.pairing_generation = 0;
    s->view.pairing_number = 0;
    s->view.flags = 0;
}
static portable_file_setup_result retain(portable_file_setup *s, int32_t error) {
    wipe(&s->descriptor, sizeof(s->descriptor));
    clear_comparison(s);
    s->view.state = PORTABLE_FILE_SETUP_STATE_RETAINED;
    s->view.remaining_ms = 0;
    s->view.error = error;
    return PORTABLE_FILE_SETUP_RETAINED;
}
static bool owner(portable_file_setup *s) {
    if (!s || s->self != s || s->view.state == PORTABLE_FILE_SETUP_STATE_RETAINED) return false;
    if (!s->owner_ok || !s->owner_ok(s->owner_context)) {
        (void)retain(s, PORTABLE_FILE_SETUP_OWNER_LOST);
        return false;
    }
    return true;
}
static portable_file_setup_result result(const portable_file_setup *s) {
    switch (s->view.state) {
    case PORTABLE_FILE_SETUP_STATE_OFF: return PORTABLE_FILE_SETUP_OK;
    case PORTABLE_FILE_SETUP_STATE_UNAVAILABLE: return PORTABLE_FILE_SETUP_UNAVAILABLE;
    case PORTABLE_FILE_SETUP_STATE_CLEANUP_PENDING: return PORTABLE_FILE_SETUP_CLEANUP_PENDING;
    case PORTABLE_FILE_SETUP_STATE_RETAINED: return PORTABLE_FILE_SETUP_RETAINED;
    default: return PORTABLE_FILE_SETUP_PENDING;
    }
}
static void finish(portable_file_setup *s) {
    wipe(&s->descriptor, sizeof(s->descriptor));
    wipe(&s->api, sizeof(s->api));
    wipe(&s->config, sizeof(s->config));
    clear_comparison(s);
    s->view.state = s->unavailable_after_close ? PORTABLE_FILE_SETUP_STATE_UNAVAILABLE : PORTABLE_FILE_SETUP_STATE_OFF;
    s->view.provider_state = RISC_SETUP_OFF;
    s->view.remaining_ms = 0;
    s->phase = PHASE_IDLE;
    s->cleanup_mode = false;
}
static portable_file_setup_result ending(portable_file_setup *s, int32_t error, bool cleanup) {
    wipe(&s->descriptor, sizeof(s->descriptor));
    clear_comparison(s);
    s->cleanup_mode = s->cleanup_mode || cleanup;
    s->view.state = s->cleanup_mode ? PORTABLE_FILE_SETUP_STATE_CLEANUP_PENDING : PORTABLE_FILE_SETUP_STATE_ENDING;
    s->view.remaining_ms = 0;
    s->view.error = error;
    s->phase = s->token ? PHASE_CLOSE : PHASE_RELEASE;
    return result(s);
}
static size_t text_size(const char *text, size_t capacity) {
    for (size_t i = 0; i < capacity; ++i) {
        if (!text[i]) return i;
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return 0;
    }
    return 0;
}
static bool valid_descriptor(const risc_bluetooth_session_descriptor_v1 *d) {
    if (!d || d->struct_size != sizeof(*d) || !d->remaining_lifetime_ms) return false;
    size_t length = text_size(d->url, sizeof(d->url));
    size_t prefix = d->transport == RISC_SESSION_HTTP ? 7 : d->transport == RISC_SESSION_HTTPS ? 8 : 0;
    if (!prefix || length <= prefix || memcmp(d->url, prefix == 7 ? "http://" : "https://", prefix) ||
        !text_size(d->username, sizeof(d->username)) || !text_size(d->password, sizeof(d->password)) ||
        !text_size(d->session_id, sizeof(d->session_id))) return false;
    bool authority = true;
    size_t authority_size = 0;
    for (size_t i = prefix; i < length; ++i) {
        unsigned char ch = (unsigned char)d->url[i];
        if (ch <= 32 || ch == '#' || ch == '\\') return false;
        if (authority && (ch == '/' || ch == '?')) authority = false;
        if (authority) {
            if (ch == '@') return false;
            ++authority_size;
        }
    }
    return authority_size != 0;
}
bool portable_file_setup_init(portable_file_setup *s, const risc_runtime_api_v1 *runtime,
    bool (*owner_ok)(void *), void *context) {
    if (!s || (s->self && (s->self != s || s->phase != PHASE_IDLE ||
        s->view.state == PORTABLE_FILE_SETUP_STATE_RETAINED)) || !runtime || !owner_ok ||
        runtime->api_version != RISC_RUNTIME_API_V1 || runtime->struct_size < RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
        !runtime->acquire || !runtime->release) return false;
    memset(s, 0, sizeof(*s));
    s->self = s;
    s->owner_ok = owner_ok;
    s->owner_context = context;
    s->acquire = runtime->acquire;
    s->release = runtime->release;
    s->grant.struct_size = sizeof(s->grant);
    return true;
}
portable_file_setup_result portable_file_setup_start(portable_file_setup *s,
    const portable_file_setup_config *config, const risc_bluetooth_session_descriptor_v1 *descriptor,
    uint64_t now_ms) {
    if (!s || s->self != s) return PORTABLE_FILE_SETUP_INVALID;
    if (s->view.state == PORTABLE_FILE_SETUP_STATE_RETAINED) return PORTABLE_FILE_SETUP_RETAINED;
    if (s->phase != PHASE_IDLE) return result(s);
    if (!config || !config->instance_id || !config->setup_lifetime_ms ||
        config->setup_lifetime_ms > RISC_SESSION_SETUP_MAX_LIFETIME_MS ||
        !text_size(config->name, sizeof(config->name)) || !valid_descriptor(descriptor) ||
        now_ms > UINT64_MAX - descriptor->remaining_lifetime_ms) return PORTABLE_FILE_SETUP_INVALID;
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    s->config = *config;
    s->descriptor = *descriptor;
    s->view = (portable_file_setup_status){.state = PORTABLE_FILE_SETUP_STATE_STARTING};
    s->started_ms = s->last_ms = now_ms;
    s->pairing_generation = s->connection_generation = s->confirmed_generation = 0;
    s->cleanup_mode = s->unavailable_after_close = false;
    s->phase = PHASE_ACQUIRE;
    return PORTABLE_FILE_SETUP_PENDING;
}
static portable_file_setup_result acquire_provider(portable_file_setup *s) {
    s->grant = (risc_runtime_capability_v1){.struct_size = sizeof(s->grant)};
    bool ok = s->acquire(RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY, RISC_BLUETOOTH_SESSION_SETUP_API_V1,
        s->config.instance_id, &s->grant);
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    if (!ok) {
        if (!empty_grant(&s->grant)) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
        s->unavailable_after_close = true;
        finish(s);
        return PORTABLE_FILE_SETUP_UNAVAILABLE;
    }
    if (!valid_grant(&s->grant)) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
    const risc_bluetooth_session_setup_v1 *api = s->grant.api;
    if (api->api_version != RISC_BLUETOOTH_SESSION_SETUP_API_V1 || api->struct_size < sizeof(*api) ||
        !api->open || !api->poll || !api->status || !api->confirm_pairing || !api->close) {
        s->unavailable_after_close = true;
        return ending(s, PORTABLE_FILE_SETUP_CONTRACT, false);
    }
    s->api = *api;
    s->phase = PHASE_OPEN;
    return PORTABLE_FILE_SETUP_PENDING;
}
static portable_file_setup_result open_provider(portable_file_setup *s, uint64_t now_ms) {
    uint64_t elapsed = now_ms - s->started_ms;
    if (elapsed >= s->descriptor.remaining_lifetime_ms) return ending(s, RISC_SETUP_EXPIRED, false);
    s->descriptor.remaining_lifetime_ms -= (uint32_t)elapsed;
    int32_t rc = s->api.open(s->api.context, s->config.name, &s->descriptor,
        s->config.setup_lifetime_ms, &s->token);
    /* open retains no caller pointers. Never retain a second local secret copy
     * for an implicit retry, even if the result is BUSY or otherwise failed. */
    wipe(&s->descriptor, sizeof(s->descriptor));
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    s->view.error = rc;
    if (rc == RISC_SETUP_OK || rc == RISC_SETUP_PENDING) {
        if (!s->token) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
        s->phase = PHASE_ACTIVE;
        s->view.state = PORTABLE_FILE_SETUP_STATE_ACTIVE;
        s->view.provider_state = RISC_SETUP_STARTING;
        return PORTABLE_FILE_SETUP_PENDING;
    }
    if (rc == RISC_SETUP_CLEANUP_PENDING) {
        if (!s->token) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
        return ending(s, rc, true);
    }
    if (rc == RISC_SETUP_FAULT || rc == RISC_SETUP_EXPIRED) {
        s->unavailable_after_close = true;
        return ending(s, rc, false);
    }
    /* Canonical open validates before creating a token or claiming native
     * custody. INVALID with its zero output is a known rejected attempt. */
    if ((rc == RISC_SETUP_BUSY || rc == RISC_SETUP_INVALID) && !s->token) {
        s->unavailable_after_close = true;
        return ending(s, rc, false);
    }
    return retain(s, rc == RISC_SETUP_INVALID || rc == RISC_SETUP_CONTEXT ? rc : PORTABLE_FILE_SETUP_CONTRACT);
}
static portable_file_setup_result native_result(portable_file_setup *s, int32_t rc) {
    s->view.error = rc;
    switch (rc) {
    case RISC_SETUP_OK: case RISC_SETUP_PENDING: case RISC_SETUP_BUSY: return PORTABLE_FILE_SETUP_PENDING;
    case RISC_SETUP_FAULT: case RISC_SETUP_EXPIRED: return ending(s, rc, false);
    case RISC_SETUP_CLEANUP_PENDING: return ending(s, rc, true);
    case RISC_SETUP_INVALID: case RISC_SETUP_CONTEXT: case RISC_SETUP_RETAINED: return retain(s, rc);
    default: return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
    }
}
static bool valid_status(const portable_file_setup *s, const risc_bluetooth_session_setup_status_v1 *v) {
    if (v->struct_size != sizeof(*v) || v->state < RISC_SETUP_STARTING || v->state > RISC_SETUP_FAILED ||
        (v->flags & ~(RISC_SETUP_ENCRYPTED | RISC_SETUP_AUTHENTICATED | RISC_SETUP_NUMERIC_CONFIRMED | RISC_SETUP_READABLE)) ||
        v->remaining_ms > s->config.setup_lifetime_ms || v->pairing_number > 999999 ||
        v->pairing_generation < s->pairing_generation || v->connection_generation < s->connection_generation ||
        (s->connection_generation && v->connection_generation != s->connection_generation)) return false;
    if (v->state >= RISC_SETUP_CONNECTED && v->state <= RISC_SETUP_READY && !v->connection_generation) return false;
    if (s->connection_generation && v->state < RISC_SETUP_CONNECTED) return false;
    if (v->state == RISC_SETUP_PAIR_CONFIRM) {
        if (!v->pairing_generation || !v->connection_generation ||
            v->pairing_generation <= s->confirmed_generation ||
            (s->view.provider_state == RISC_SETUP_PAIR_CONFIRM &&
             v->pairing_generation == s->view.pairing_generation &&
             v->pairing_number != s->view.pairing_number)) return false;
    } else if (v->pairing_number) return false;
    if ((v->flags & RISC_SETUP_READABLE) && (v->state != RISC_SETUP_READY ||
        v->flags != (RISC_SETUP_ENCRYPTED | RISC_SETUP_AUTHENTICATED | RISC_SETUP_NUMERIC_CONFIRMED | RISC_SETUP_READABLE) ||
        !s->confirmed_generation || v->pairing_generation != s->confirmed_generation)) return false;
    return true;
}
portable_file_setup_result portable_file_setup_step(portable_file_setup *s, uint64_t now_ms) {
    if (!s || s->self != s) return PORTABLE_FILE_SETUP_INVALID;
    if (s->view.state == PORTABLE_FILE_SETUP_STATE_RETAINED) return PORTABLE_FILE_SETUP_RETAINED;
    if (s->phase == PHASE_IDLE) return result(s);
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    /* Cleanup never needs a clock and must not become ordinary work again. */
    if (s->phase == PHASE_CLOSE || s->phase == PHASE_RELEASE) return portable_file_setup_close(s);
    if (now_ms < s->last_ms) return retain(s, PORTABLE_FILE_SETUP_CLOCK_CHANGED);
    s->last_ms = now_ms;
    if (s->phase == PHASE_ACQUIRE) return acquire_provider(s);
    if (s->phase == PHASE_OPEN) return open_provider(s, now_ms);
    if (s->phase != PHASE_ACTIVE) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
    int32_t rc = s->api.poll(s->api.context, s->token, 16);
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    portable_file_setup_result mapped = native_result(s, rc);
    if (s->phase != PHASE_ACTIVE || rc == RISC_SETUP_BUSY) return mapped;
    risc_bluetooth_session_setup_status_v1 value = {.struct_size = sizeof(value)};
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    rc = s->api.status(s->api.context, s->token, &value);
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    mapped = native_result(s, rc);
    if (rc != RISC_SETUP_OK) return mapped;
    if (!valid_status(s, &value)) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
    s->view.provider_state = value.state;
    s->view.flags = value.flags;
    s->view.remaining_ms = value.remaining_ms;
    s->view.error = value.error;
    s->pairing_generation = value.pairing_generation;
    /* Provider generations survive close/reopen. Advertising can report the
     * previous connection's generation; latch only this session's connection. */
    if (value.state >= RISC_SETUP_CONNECTED && value.state <= RISC_SETUP_READY)
        s->connection_generation = value.connection_generation;
    s->view.pairing_generation = value.state == RISC_SETUP_PAIR_CONFIRM ? value.pairing_generation : 0;
    s->view.pairing_number = value.state == RISC_SETUP_PAIR_CONFIRM ? value.pairing_number : 0;
    if (value.state == RISC_SETUP_ENDED || value.state == RISC_SETUP_FAILED) return ending(s, value.error, false);
    return PORTABLE_FILE_SETUP_PENDING;
}
portable_file_setup_result portable_file_setup_confirm(portable_file_setup *s,
    uint32_t generation, uint32_t number, bool accept) {
    if (!s || s->self != s) return PORTABLE_FILE_SETUP_INVALID;
    if (s->view.state == PORTABLE_FILE_SETUP_STATE_RETAINED) return PORTABLE_FILE_SETUP_RETAINED;
    if (s->phase != PHASE_ACTIVE) return result(s);
    if (s->view.provider_state != RISC_SETUP_PAIR_CONFIRM || !generation ||
        generation != s->view.pairing_generation || number != s->view.pairing_number) return PORTABLE_FILE_SETUP_INVALID;
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    int32_t rc = s->api.confirm_pairing(s->api.context, s->token, generation, number, accept);
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    /* A comparison can change between display and the user's choice. INVALID
     * explicitly rejects that choice without changing the provider's pending
     * comparison; forget the old display and require a fresh explicit choice. */
    if (rc == RISC_SETUP_INVALID) {
        clear_comparison(s);
        s->view.provider_state = RISC_SETUP_CONNECTED;
        s->view.error = rc;
        return PORTABLE_FILE_SETUP_INVALID;
    }
    portable_file_setup_result mapped = native_result(s, rc);
    if (s->phase != PHASE_ACTIVE || rc == RISC_SETUP_BUSY) return mapped;
    clear_comparison(s);
    s->view.provider_state = RISC_SETUP_CONNECTED;
    s->confirmed_generation = generation;
    if (!accept) return ending(s, RISC_SETUP_OK, false);
    return PORTABLE_FILE_SETUP_PENDING;
}
portable_file_setup_result portable_file_setup_close(portable_file_setup *s) {
    if (!s || s->self != s) return PORTABLE_FILE_SETUP_INVALID;
    if (s->view.state == PORTABLE_FILE_SETUP_STATE_RETAINED) return PORTABLE_FILE_SETUP_RETAINED;
    if (s->phase == PHASE_IDLE) return result(s);
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    if (s->phase != PHASE_CLOSE && s->phase != PHASE_RELEASE) (void)ending(s, s->view.error, false);
    if (s->phase == PHASE_CLOSE) {
        int32_t rc = s->api.close(s->api.context, s->token);
        if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
        if (rc == RISC_SETUP_CLEANUP_PENDING) return ending(s, rc, true);
        if (rc == RISC_SETUP_PENDING || rc == RISC_SETUP_BUSY) return result(s);
        if (rc != RISC_SETUP_OK) return retain(s, rc);
        s->token = 0;
        s->phase = PHASE_RELEASE;
        /* Preserve cleanup-only until the owning grant has been released. */
        return result(s);
    }
    if (!s->grant.api) {
        if (!empty_grant(&s->grant)) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
        finish(s);
        return result(s);
    }
    if (!valid_grant(&s->grant)) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
    risc_runtime_capability_v1 released = s->grant;
    bool ok = s->release(&released);
    if (!owner(s)) return PORTABLE_FILE_SETUP_RETAINED;
    if (!ok || !empty_grant(&released)) return retain(s, PORTABLE_FILE_SETUP_CONTRACT);
    s->grant = released;
    finish(s);
    return result(s);
}
void portable_file_setup_get_status(const portable_file_setup *s, portable_file_setup_status *out) {
    if (!out) return;
    if (!s || s->self != s) {
        *out = (portable_file_setup_status){.state = PORTABLE_FILE_SETUP_STATE_RETAINED, .error = PORTABLE_FILE_SETUP_CONTRACT};
        return;
    }
    *out = s->view;
}
bool portable_file_setup_cleanup_only(const portable_file_setup *s) {
    return s && s->self == s && s->cleanup_mode && s->view.state != PORTABLE_FILE_SETUP_STATE_RETAINED;
}
