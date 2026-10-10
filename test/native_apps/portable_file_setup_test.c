#include "PortableFileSetup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { ACQUIRE, OPEN, POLL, STATUS, CONFIRM, CLOSE, RELEASE, OPERATIONS };
typedef struct {
    bool owner, acquire_ok, release_ok, grant_live, native_live, close_only;
    unsigned calls[OPERATIONS], order[128], order_size;
    int lose_owner_at, malformed_acquire, malformed_release;
    int32_t open_rc, poll_rc, status_rc, confirm_rc, close_rc;
    uint64_t open_token, last_close_token;
    uint32_t confirmed_generation, confirmed_number;
    bool accepted;
    risc_bluetooth_session_descriptor_v1 descriptor;
    risc_bluetooth_session_setup_status_v1 status;
    risc_bluetooth_session_setup_v1 api;
} fixture;
static fixture f;
static unsigned tests;
static bool owns(void *context) { assert(context == &f); return f.owner; }
static void called(unsigned operation) {
    assert(f.owner);
    assert(operation < OPERATIONS && f.order_size < sizeof(f.order) / sizeof(*f.order));
    if (f.close_only) assert(operation == CLOSE || operation == RELEASE);
    ++f.calls[operation];
    f.order[f.order_size++] = operation;
}
static void leaving(unsigned operation) { if (f.lose_owner_at == (int)operation) f.owner = false; }
static int32_t open_native(void *context, const char *name,
    const risc_bluetooth_session_descriptor_v1 *descriptor, uint32_t duration, uint64_t *token) {
    assert(context == &f && f.grant_live && !f.native_live);
    assert(!strcmp(name, "Watch files") && duration == 60000);
    called(OPEN);
    f.descriptor = *descriptor;
    *token = f.open_token;
    f.native_live = *token != 0 || f.open_rc == RISC_SETUP_OK || f.open_rc == RISC_SETUP_PENDING;
    f.close_only = f.open_rc == RISC_SETUP_CLEANUP_PENDING;
    leaving(OPEN);
    return f.open_rc;
}
static int32_t poll_native(void *context, uint64_t token, uint32_t events) {
    assert(context == &f && f.grant_live && f.native_live && token == f.open_token && events == 16);
    called(POLL); leaving(POLL); return f.poll_rc;
}
static int32_t status_native(void *context, uint64_t token, risc_bluetooth_session_setup_status_v1 *out) {
    assert(context == &f && f.grant_live && f.native_live && token == f.open_token && out->struct_size == sizeof(*out));
    called(STATUS);
    if (f.status_rc == RISC_SETUP_OK) *out = f.status;
    leaving(STATUS); return f.status_rc;
}
static int32_t confirm_native(void *context, uint64_t token, uint32_t generation, uint32_t number, bool accept) {
    assert(context == &f && f.grant_live && f.native_live && token == f.open_token);
    called(CONFIRM);
    f.confirmed_generation = generation; f.confirmed_number = number; f.accepted = accept;
    leaving(CONFIRM); return f.confirm_rc;
}
static int32_t close_native(void *context, uint64_t token) {
    assert(context == &f && f.grant_live && f.native_live && token == f.open_token);
    called(CLOSE); f.last_close_token = token;
    memset(&f.descriptor, 0, sizeof(f.descriptor));
    if (f.close_rc == RISC_SETUP_CLEANUP_PENDING) f.close_only = true;
    if (f.close_rc == RISC_SETUP_OK) f.native_live = false;
    leaving(CLOSE); return f.close_rc;
}
static bool acquire(const char *name, uint32_t version, uint64_t instance, risc_runtime_capability_v1 *out) {
    assert(!f.grant_live && !strcmp(name, RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY));
    assert(version == 1 && instance == UINT64_C(0x1234567800000091));
    assert(out->struct_size == sizeof(*out) && !out->slot && !out->generation && !out->api);
    called(ACQUIRE);
    if (f.acquire_ok || f.malformed_acquire) {
        *out = (risc_runtime_capability_v1){sizeof(*out), 4, 8, &f.api};
        f.grant_live = true;
        if (f.malformed_acquire == 1) out->slot = 0;
        if (f.malformed_acquire == 2) out->generation = 0;
        if (f.malformed_acquire == 3) out->api = NULL;
        if (f.malformed_acquire == 4) out->struct_size = 0;
    }
    leaving(ACQUIRE); return f.acquire_ok;
}
static bool release(risc_runtime_capability_v1 *grant) {
    assert(f.grant_live && !f.native_live && grant->slot == 4 && grant->generation == 8 && grant->api == &f.api);
    called(RELEASE);
    if (f.release_ok) {
        f.grant_live = false;
        if (!f.malformed_release) *grant = (risc_runtime_capability_v1){.struct_size = sizeof(*grant)};
    } else if (f.malformed_release) memset(grant, 0, sizeof(*grant));
    leaving(RELEASE); return f.release_ok;
}
static const risc_runtime_api_v1 runtime = {
    .api_version = 1, .struct_size = sizeof(runtime), .acquire = acquire, .release = release
};
static portable_file_setup_config config(void) {
    return (portable_file_setup_config){UINT64_C(0x1234567800000091), 60000, "Watch files"};
}
static risc_bluetooth_session_descriptor_v1 descriptor(void) {
    return (risc_bluetooth_session_descriptor_v1){sizeof(risc_bluetooth_session_descriptor_v1),
        RISC_SESSION_HTTP, 10000, "http://192.0.2.1:8080/", "files", "ephemeral-secret", "session-123"};
}
static void reset(void) {
    memset(&f, 0, sizeof(f));
    f.owner = f.acquire_ok = f.release_ok = true;
    f.lose_owner_at = -1;
    f.open_token = UINT64_C(0xAABBCCDDEEFF0011);
    f.api = (risc_bluetooth_session_setup_v1){1, sizeof(f.api), &f, open_native, poll_native,
        status_native, confirm_native, close_native};
    f.status = (risc_bluetooth_session_setup_status_v1){.struct_size = sizeof(f.status),
        .state = RISC_SETUP_ADVERTISING, .remaining_ms = 9000};
    ++tests;
}
static portable_file_setup_status snapshot(portable_file_setup *s) {
    portable_file_setup_status out;
    unsigned calls = f.order_size;
    portable_file_setup_get_status(s, &out);
    (void)portable_file_setup_cleanup_only(s);
    assert(f.order_size == calls);
    return out;
}
static bool zero(const void *memory, size_t size) {
    const unsigned char *p = memory;
    for (size_t i = 0; i < size; ++i) if (p[i]) return false;
    return true;
}
static void start(portable_file_setup *s) {
    assert(portable_file_setup_init(s, &runtime, owns, &f));
    portable_file_setup_config c = config();
    risc_bluetooth_session_descriptor_v1 d = descriptor();
    assert(portable_file_setup_start(s, &c, &d, 1000) == PORTABLE_FILE_SETUP_PENDING);
    assert(!f.order_size && snapshot(s).state == PORTABLE_FILE_SETUP_STATE_STARTING);
}
static void step(portable_file_setup *s, uint64_t now) {
    unsigned before = f.order_size;
    (void)portable_file_setup_step(s, now);
    assert(f.order_size - before <= 2);
}
static void active(portable_file_setup *s) {
    start(s); step(s, 1010); step(s, 1020);
    assert(snapshot(s).state == PORTABLE_FILE_SETUP_STATE_ACTIVE);
    assert(zero(&s->descriptor, sizeof(s->descriptor)));
}
static void pairing(portable_file_setup *s) {
    f.status.state = RISC_SETUP_PAIR_CONFIRM;
    f.status.connection_generation = 5;
    f.status.pairing_generation = 7;
    f.status.pairing_number = 42;
    step(s, 1030);
    assert(snapshot(s).pairing_number == 42 && snapshot(s).pairing_generation == 7);
}
static void clean(portable_file_setup *s) {
    for (unsigned i = 0; i < 3 && (f.grant_live || s->phase); ++i) {
        unsigned before = f.order_size;
        (void)portable_file_setup_close(s);
        assert(f.order_size - before <= 1);
    }
    portable_file_setup_state state = snapshot(s).state;
    assert(state == PORTABLE_FILE_SETUP_STATE_OFF || state == PORTABLE_FILE_SETUP_STATE_UNAVAILABLE);
    assert(!f.grant_live && !f.native_live && !s->token && !s->grant.api);
    assert(!portable_file_setup_cleanup_only(s) && zero(&s->descriptor, sizeof(s->descriptor)));
}
static void terminal(portable_file_setup *s) {
    assert(snapshot(s).state == PORTABLE_FILE_SETUP_STATE_RETAINED);
    assert(zero(&s->descriptor, sizeof(s->descriptor)));
    assert(!snapshot(s).pairing_generation && !snapshot(s).pairing_number);
    unsigned before = f.order_size;
    portable_file_setup_config c = config();
    risc_bluetooth_session_descriptor_v1 d = descriptor();
    assert(portable_file_setup_step(s, 9000) == PORTABLE_FILE_SETUP_RETAINED);
    assert(portable_file_setup_close(s) == PORTABLE_FILE_SETUP_RETAINED);
    assert(portable_file_setup_confirm(s, 7, 42, true) == PORTABLE_FILE_SETUP_RETAINED);
    assert(portable_file_setup_start(s, &c, &d, 9000) == PORTABLE_FILE_SETUP_RETAINED);
    assert(!portable_file_setup_init(s, &runtime, owns, &f));
    f.owner = true; /* Restored ownership cannot undo the terminal fence. */
    assert(portable_file_setup_close(s) == PORTABLE_FILE_SETUP_RETAINED && f.order_size == before);
}
static void test_copy_confirm(void) {
    reset(); portable_file_setup s = {0};
    assert(portable_file_setup_init(&s, &runtime, owns, &f));
    portable_file_setup_config c = config();
    risc_bluetooth_session_descriptor_v1 d = descriptor(), original = d;
    assert(portable_file_setup_start(&s, &c, &d, 1000) == PORTABLE_FILE_SETUP_PENDING);
    memset(&d, 0xAB, sizeof(d)); memset(&c, 0xCD, sizeof(c));
    step(&s, 1010); step(&s, 1020);
    original.remaining_lifetime_ms -= 20;
    assert(!memcmp(&f.descriptor, &original, sizeof(original)));
    assert(zero(&s.descriptor, sizeof(s.descriptor)));
    pairing(&s);
    assert(portable_file_setup_confirm(&s, 8, 42, true) == PORTABLE_FILE_SETUP_INVALID);
    assert(portable_file_setup_confirm(&s, 7, 43, true) == PORTABLE_FILE_SETUP_INVALID);
    assert(!f.calls[CONFIRM]);
    assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_PENDING);
    assert(f.calls[CONFIRM] == 1 && f.confirmed_generation == 7 && f.confirmed_number == 42 && f.accepted);
    assert(!snapshot(&s).pairing_number && !snapshot(&s).pairing_generation);
    assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_INVALID && f.calls[CONFIRM] == 1);
    f.status.state = RISC_SETUP_READY; f.status.pairing_number = 0;
    f.status.flags = RISC_SETUP_ENCRYPTED | RISC_SETUP_AUTHENTICATED | RISC_SETUP_NUMERIC_CONFIRMED | RISC_SETUP_READABLE;
    step(&s, 1040); assert(snapshot(&s).provider_state == RISC_SETUP_READY);
    clean(&s);
    assert(f.order[f.order_size - 2] == CLOSE && f.order[f.order_size - 1] == RELEASE);
    unsigned before = f.order_size;
    step(&s, 20000); assert(portable_file_setup_close(&s) == PORTABLE_FILE_SETUP_OK && before == f.order_size);
    c = config(); d = descriptor();
    assert(portable_file_setup_start(&s, &c, &d, 20000) == PORTABLE_FILE_SETUP_PENDING);
    assert(f.calls[OPEN] == 1); clean(&s);
}
static void test_validation(void) {
    for (unsigned which = 0; which < 16; ++which) {
        reset(); portable_file_setup s = {0};
        assert(portable_file_setup_init(&s, &runtime, owns, &f));
        portable_file_setup_config c = config(); risc_bluetooth_session_descriptor_v1 d = descriptor();
        uint64_t now = 1000;
        switch (which) {
        case 0: c.instance_id = 0; break;
        case 1: c.setup_lifetime_ms = 0; break;
        case 2: c.setup_lifetime_ms = 300001; break;
        case 3: c.name[0] = 0; break;
        case 4: memset(c.name, 'a', sizeof(c.name)); break;
        case 5: d.remaining_lifetime_ms = 0; break;
        case 6: d.transport = RISC_SESSION_HTTPS; break;
        case 7: strcpy(d.url, "http:///empty"); break;
        case 8: strcpy(d.url, "http://user@host/"); break;
        case 9: strcpy(d.url, "http://host/#fragment"); break;
        case 10: strcpy(d.url, "http://host/white space"); break;
        case 11: strcpy(d.url, "http://host/\\path"); break;
        case 12: d.username[0] = 0; break;
        case 13: d.password[0] = '\n'; break;
        case 14: memset(d.session_id, 'x', sizeof(d.session_id)); break;
        case 15: now = UINT64_MAX; break;
        }
        assert(portable_file_setup_start(&s, &c, &d, now) == PORTABLE_FILE_SETUP_INVALID);
        assert(!f.order_size && zero(&s.descriptor, sizeof(s.descriptor)));
    }
}
static void test_cancel_expiry(void) {
    for (unsigned phase = 0; phase < 3; ++phase) {
        reset(); portable_file_setup s = {0}; start(&s);
        if (phase) step(&s, 1010);
        if (phase == 2) step(&s, 11000);
        clean(&s);
        assert(!f.calls[OPEN] && !f.calls[CLOSE] && f.calls[RELEASE] == (phase ? 1u : 0u));
    }
    reset(); portable_file_setup s = {0}; active(&s);
    f.poll_rc = RISC_SETUP_EXPIRED; step(&s, 11000);
    assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ENDING && !f.calls[STATUS]); clean(&s);
}
static void test_cleanup(void) {
    for (unsigned origin = 0; origin < 4; ++origin) {
        reset(); portable_file_setup s = {0};
        if (origin == 0) { f.open_rc = RISC_SETUP_CLEANUP_PENDING; start(&s); step(&s, 1010); step(&s, 1020); }
        else {
            active(&s);
            if (origin == 1) { f.poll_rc = RISC_SETUP_CLEANUP_PENDING; step(&s, 1030); }
            if (origin == 2) { f.status_rc = RISC_SETUP_CLEANUP_PENDING; step(&s, 1030); }
            if (origin == 3) { f.close_rc = RISC_SETUP_CLEANUP_PENDING; (void)portable_file_setup_close(&s); }
        }
        f.close_only = true; f.close_rc = RISC_SETUP_CLEANUP_PENDING;
        assert(portable_file_setup_cleanup_only(&s));
        unsigned before = f.order_size;
        portable_file_setup_config c = config(); risc_bluetooth_session_descriptor_v1 d = descriptor();
        assert(portable_file_setup_start(&s, &c, &d, 1050) == PORTABLE_FILE_SETUP_CLEANUP_PENDING);
        assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_CLEANUP_PENDING);
        assert(f.order_size == before);
        step(&s, 1); /* A changed caller clock cannot block a checked-close retry. */
        assert(f.order_size == before + 1 && f.last_close_token == f.open_token && !f.calls[RELEASE]);
        f.close_rc = RISC_SETUP_BUSY; (void)portable_file_setup_close(&s);
        assert(portable_file_setup_cleanup_only(&s));
        f.close_rc = RISC_SETUP_OK; (void)portable_file_setup_close(&s);
        assert(portable_file_setup_cleanup_only(&s) && f.grant_live && !f.native_live && !f.calls[RELEASE]);
        clean(&s);
    }
    reset(); portable_file_setup s = {0}; active(&s);
    f.close_rc = RISC_SETUP_PENDING;
    assert(portable_file_setup_close(&s) == PORTABLE_FILE_SETUP_PENDING);
    assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ENDING && !portable_file_setup_cleanup_only(&s));
    f.close_rc = RISC_SETUP_OK; clean(&s);
}
static void test_owner_loss(void) {
    for (unsigned operation = 0; operation < OPERATIONS; ++operation) {
        reset(); portable_file_setup s = {0};
        if (operation == ACQUIRE || operation == OPEN) {
            start(&s); f.lose_owner_at = (int)operation; step(&s, 1010);
            if (operation == OPEN) step(&s, 1020);
        } else {
            active(&s);
            if (operation == CONFIRM) pairing(&s);
            f.lose_owner_at = (int)operation;
            if (operation == POLL || operation == STATUS) step(&s, 1040);
            if (operation == CONFIRM) (void)portable_file_setup_confirm(&s, 7, 42, true);
            if (operation == CLOSE || operation == RELEASE) {
                (void)portable_file_setup_close(&s);
                if (operation == RELEASE) (void)portable_file_setup_close(&s);
            }
        }
        assert(f.calls[operation] == 1); terminal(&s);
    }
    reset(); portable_file_setup s = {0}; active(&s);
    f.owner = false; unsigned before = f.order_size; step(&s, 1040);
    assert(before == f.order_size); terminal(&s);
    reset(); memset(&s, 0, sizeof(s)); active(&s);
    f.close_rc = RISC_SETUP_CLEANUP_PENDING; (void)portable_file_setup_close(&s);
    f.owner = false; before = f.order_size; (void)portable_file_setup_close(&s);
    assert(before == f.order_size); terminal(&s);
}
static void test_open_results(void) {
    const int32_t errors[] = {RISC_SETUP_OK, RISC_SETUP_PENDING, RISC_SETUP_CONTEXT, 77, RISC_SETUP_CLEANUP_PENDING};
    for (unsigned i = 0; i < sizeof(errors) / sizeof(*errors); ++i) {
        reset(); portable_file_setup s = {0}; start(&s);
        f.open_rc = errors[i]; f.open_token = 0;
        step(&s, 1010); step(&s, 1020); terminal(&s);
        assert(f.calls[OPEN] == 1 && !f.calls[CLOSE] && !f.calls[RELEASE]);
    }
    const int32_t ordinary[] = {RISC_SETUP_FAULT, RISC_SETUP_EXPIRED, RISC_SETUP_BUSY, RISC_SETUP_INVALID};
    for (unsigned i = 0; i < sizeof(ordinary) / sizeof(*ordinary); ++i) {
        reset(); portable_file_setup s = {0}; start(&s);
        f.open_rc = ordinary[i]; f.open_token = 0;
        step(&s, 1010); step(&s, 1020); clean(&s);
        assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_UNAVAILABLE && !f.calls[CLOSE]);
    }
    for (unsigned i = 0; i < 2; ++i) {
        reset(); portable_file_setup s = {0}; start(&s);
        f.open_rc = i ? RISC_SETUP_EXPIRED : RISC_SETUP_FAULT;
        step(&s, 1010); step(&s, 1020);
        assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ENDING && s.token == f.open_token);
        clean(&s); assert(f.calls[CLOSE] == 1);
    }
    const int32_t contradictory[] = {RISC_SETUP_INVALID, RISC_SETUP_CONTEXT, RISC_SETUP_BUSY, 77};
    for (unsigned i = 0; i < sizeof(contradictory) / sizeof(*contradictory); ++i) {
        reset(); portable_file_setup s = {0}; start(&s); f.open_rc = contradictory[i];
        step(&s, 1010); step(&s, 1020);
        assert(s.token == f.open_token); terminal(&s);
        assert(!f.calls[CLOSE] && !f.calls[RELEASE]);
    }
}
static void test_native_results(void) {
    const int32_t results[] = {RISC_SETUP_INVALID, RISC_SETUP_CONTEXT, -123, RISC_SETUP_FAULT, RISC_SETUP_EXPIRED};
    for (unsigned operation = POLL; operation <= CONFIRM; ++operation) {
        for (unsigned i = 0; i < sizeof(results) / sizeof(*results); ++i) {
            reset(); portable_file_setup s = {0}; active(&s);
            if (operation == CONFIRM) pairing(&s);
            if (operation == POLL) f.poll_rc = results[i];
            if (operation == STATUS) f.status_rc = results[i];
            if (operation == CONFIRM) {
                f.confirm_rc = results[i]; (void)portable_file_setup_confirm(&s, 7, 42, true);
            } else step(&s, 1040);
            if (operation == CONFIRM && results[i] == RISC_SETUP_INVALID) {
                assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ACTIVE && !snapshot(&s).pairing_generation);
                clean(&s);
            } else if (i < 3) terminal(&s);
            else { assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ENDING); clean(&s); }
        }
    }
    reset(); portable_file_setup s = {0}; active(&s); pairing(&s);
    assert(portable_file_setup_confirm(&s, 7, 42, false) == PORTABLE_FILE_SETUP_PENDING && !f.accepted);
    assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ENDING); clean(&s);
    reset(); memset(&s, 0, sizeof(s)); active(&s); pairing(&s);
    f.confirm_rc = RISC_SETUP_BUSY;
    assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_PENDING);
    assert(snapshot(&s).pairing_generation == 7 && snapshot(&s).pairing_number == 42);
    clean(&s);
    reset(); memset(&s, 0, sizeof(s)); active(&s); pairing(&s);
    f.confirm_rc = RISC_SETUP_INVALID;
    assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_INVALID);
    assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ACTIVE && !snapshot(&s).pairing_generation);
    unsigned before = f.order_size;
    assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_INVALID && f.order_size == before);
    f.status.pairing_generation = 8; f.status.pairing_number = 123456;
    step(&s, 1040);
    assert(snapshot(&s).pairing_generation == 8 && snapshot(&s).pairing_number == 123456 && f.calls[CONFIRM] == 1);
    assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_INVALID && f.calls[CONFIRM] == 1);
    f.confirm_rc = RISC_SETUP_OK;
    assert(portable_file_setup_confirm(&s, 8, 123456, false) == PORTABLE_FILE_SETUP_PENDING && !f.accepted);
    clean(&s);
}
static void test_stale_status(void) {
    for (unsigned which = 0; which < 9; ++which) {
        reset(); portable_file_setup s = {0}; active(&s); pairing(&s);
        switch (which) {
        case 0: --f.status.pairing_generation; break;
        case 1: --f.status.connection_generation; break;
        case 2: ++f.status.connection_generation; break;
        case 3: ++f.status.pairing_number; break;
        case 4: f.status.struct_size = 0; break;
        case 5: f.status.state = RISC_SETUP_OFF; break;
        case 6: f.status.flags = 16; break;
        case 7: f.status.pairing_number = 1000000; break;
        case 8: f.status.state = RISC_SETUP_READY; f.status.pairing_number = 0; f.status.flags = 15; break;
        }
        step(&s, 1040); terminal(&s);
    }
    reset(); portable_file_setup s = {0}; active(&s); pairing(&s);
    assert(portable_file_setup_confirm(&s, 7, 42, true) == PORTABLE_FILE_SETUP_PENDING);
    step(&s, 1040); terminal(&s); /* Replayed accepted comparison. */
    reset(); memset(&s, 0, sizeof(s)); active(&s); step(&s, 1019); terminal(&s);
    reset(); memset(&s, 0, sizeof(s)); active(&s);
    /* The actual provider retains global counters across clean reopen. */
    f.status.connection_generation = 30; f.status.pairing_generation = 40;
    step(&s, 1030);
    f.status.state = RISC_SETUP_CONNECTED; f.status.connection_generation = 31;
    step(&s, 1040); assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_ACTIVE);
    f.status.state = RISC_SETUP_PAIR_CONFIRM; f.status.pairing_generation = 41; f.status.pairing_number = 0;
    step(&s, 1050);
    assert(snapshot(&s).pairing_generation == 41 && !snapshot(&s).pairing_number);
    assert(portable_file_setup_confirm(&s, 41, 0, true) == PORTABLE_FILE_SETUP_PENDING);
    assert(f.confirmed_generation == 41 && !f.confirmed_number && f.accepted);
    clean(&s);
}
static void test_grants(void) {
    reset(); portable_file_setup s = {0}; start(&s); f.acquire_ok = false;
    step(&s, 1010); assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_UNAVAILABLE);
    assert(zero(&s.descriptor, sizeof(s.descriptor)));
    unsigned before = f.order_size; step(&s, 1020); assert(f.order_size == before);
    f.acquire_ok = true;
    portable_file_setup_config c = config(); risc_bluetooth_session_descriptor_v1 d = descriptor();
    assert(portable_file_setup_start(&s, &c, &d, 2000) == PORTABLE_FILE_SETUP_PENDING);
    step(&s, 2000); step(&s, 2000); clean(&s);
    for (unsigned shape = 1; shape <= 4; ++shape) {
        reset(); memset(&s, 0, sizeof(s)); start(&s); f.malformed_acquire = (int)shape;
        step(&s, 1010); terminal(&s);
    }
    reset(); memset(&s, 0, sizeof(s)); start(&s); f.acquire_ok = false; f.malformed_acquire = 1;
    step(&s, 1010); terminal(&s);
    for (unsigned which = 0; which < 3; ++which) {
        reset(); memset(&s, 0, sizeof(s)); start(&s);
        if (which == 0) f.api.api_version = 2;
        if (which == 1) f.api.struct_size = 8;
        if (which == 2) f.api.close = NULL;
        step(&s, 1010); clean(&s);
        assert(snapshot(&s).state == PORTABLE_FILE_SETUP_STATE_UNAVAILABLE && !f.calls[OPEN]);
    }
    for (unsigned which = 0; which < 3; ++which) {
        reset(); memset(&s, 0, sizeof(s)); active(&s);
        if (which < 2) f.release_ok = false;
        f.malformed_release = which != 0;
        (void)portable_file_setup_close(&s); (void)portable_file_setup_close(&s);
        assert(s.grant.api == &f.api); terminal(&s);
    }
    reset(); memset(&s, 0, sizeof(s)); active(&s);
    f.close_rc = RISC_SETUP_CONTEXT; (void)portable_file_setup_close(&s); terminal(&s);
}
int main(void) {
    test_copy_confirm(); test_validation(); test_cancel_expiry(); test_cleanup();
    test_owner_loss(); test_open_results(); test_native_results(); test_stale_status(); test_grants();
    printf("portable file setup: %u deterministic cases passed\n", tests);
    return 0;
}
