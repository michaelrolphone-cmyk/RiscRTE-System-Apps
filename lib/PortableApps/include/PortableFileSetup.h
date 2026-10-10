#pragma once
#include "RiscRuntimeV1.h"
#include "RiscBluetoothSessionSetupV1.h"
#ifdef __cplusplus
extern "C" {
#endif

/* App-local, allocation-free consumer of the canonical setup API. This is not
 * a Runtime extension. The caller supplies an already-ready authorized HTTP(S)
 * descriptor and an exact declared instance. No Wi-Fi, HTTP, export or storage
 * operation is started here. All calls belong to one foreground owner task. */
typedef enum {
    PORTABLE_FILE_SETUP_STATE_OFF,
    PORTABLE_FILE_SETUP_STATE_STARTING,
    PORTABLE_FILE_SETUP_STATE_ACTIVE,
    PORTABLE_FILE_SETUP_STATE_ENDING,
    PORTABLE_FILE_SETUP_STATE_CLEANUP_PENDING,
    PORTABLE_FILE_SETUP_STATE_RETAINED,
    PORTABLE_FILE_SETUP_STATE_UNAVAILABLE
} portable_file_setup_state;
typedef enum {
    PORTABLE_FILE_SETUP_OK = 0,
    PORTABLE_FILE_SETUP_PENDING = 1,
    PORTABLE_FILE_SETUP_UNAVAILABLE = 2,
    PORTABLE_FILE_SETUP_CLEANUP_PENDING = 3,
    PORTABLE_FILE_SETUP_INVALID = -1,
    PORTABLE_FILE_SETUP_RETAINED = -2
} portable_file_setup_result;
enum {
    PORTABLE_FILE_SETUP_OWNER_LOST = -100,
    PORTABLE_FILE_SETUP_CONTRACT = -101,
    PORTABLE_FILE_SETUP_CLOCK_CHANGED = -102
};
typedef struct {
    uint64_t instance_id; /* Nonzero, exact manifest instance; no discovery. */
    uint32_t setup_lifetime_ms; /* 1..300000; capped by HTTP remaining duration. */
    char name[21]; /* 1..20 printable ASCII bytes; contains no credentials. */
} portable_file_setup_config;
typedef struct {
    portable_file_setup_state state;
    uint32_t provider_state, flags, remaining_ms;
    uint32_t pairing_generation, pairing_number;
    int32_t error;
} portable_file_setup_status;
typedef struct portable_file_setup {
    /* Private implementation storage: zero-initialize; never copy, overwrite,
     * persist or reinitialize while live or retained. Keep it until app exit. */
    const struct portable_file_setup *self;
    bool (*owner_ok)(void *);
    void *owner_context;
    bool (*acquire)(const char *, uint32_t, uint64_t, risc_runtime_capability_v1 *);
    bool (*release)(risc_runtime_capability_v1 *);
    risc_runtime_capability_v1 grant;
    risc_bluetooth_session_setup_v1 api;
    risc_bluetooth_session_descriptor_v1 descriptor;
    portable_file_setup_config config;
    portable_file_setup_status view;
    uint64_t token, started_ms, last_ms;
    uint32_t phase, connection_generation, pairing_generation, confirmed_generation;
    bool cleanup_mode, unavailable_after_close;
} portable_file_setup;

/* init performs no external I/O. owner_ok is a pure cached predicate, never a
 * provider or diagnostic query; checked before and after every external call.
 * False permanently fences ALL I/O, including close/release. */
bool portable_file_setup_init(portable_file_setup *, const risc_runtime_api_v1 *,
    bool (*owner_ok)(void *), void *owner_context);
/* Explicit user start only. Copies the finite descriptor without external I/O.
 * now_ms is a same-boot monotonic clock; sample the ready HTTP remaining duration
 * immediately before start. step deducts acquisition delay before native open.
 * Caller must wipe its own temporary descriptor separately after this copy. */
portable_file_setup_result portable_file_setup_start(portable_file_setup *,
    const portable_file_setup_config *, const risc_bluetooth_session_descriptor_v1 *,
    uint64_t now_ms);
/* Call at least every 20 ms while active. One acquire OR open OR close/release
 * phase, or one poll(max 16 events) plus one status. No loops/waits. A consumed
 * open attempt immediately wipes local credentials, including failed open.
 * Native FAULT/EXPIRED with a token moves to ENDING for checked close. */
portable_file_setup_result portable_file_setup_step(portable_file_setup *, uint64_t now_ms);
/* Supply the generation AND six-digit number copied from the same displayed
 * status, and this user's explicit accept/reject choice. Never automatically
 * accept. Local mismatches cause no I/O. Native confirmation INVALID discards
 * the displayed comparison; the next step refreshes it and a new explicit
 * choice is required. CONTEXT/malformed results permanently fence all I/O. */
portable_file_setup_result portable_file_setup_confirm(portable_file_setup *,
    uint32_t pairing_generation, uint32_t displayed_number, bool accept);
/* One bounded cleanup phase per call; retry on later foreground iterations.
 * Only native close OK permits release. CLEANUP_PENDING retains dependencies
 * and permits only this same checked close retry. A failed/ambiguous release is
 * terminal and never retried. OFF/UNAVAILABLE are clean; only then may caller
 * close HTTP/network, sleep, launch, return, or discard this client. */
portable_file_setup_result portable_file_setup_close(portable_file_setup *);
/* Both are pure local reads. During cleanup_only suppress ordinary provider
 * I/O, render calls and Runtime yields that poll providers. Use only checked
 * close and an independently verified scheduler-only yield until clean. */
void portable_file_setup_get_status(const portable_file_setup *, portable_file_setup_status *);
bool portable_file_setup_cleanup_only(const portable_file_setup *);
#ifdef __cplusplus
}
#endif
