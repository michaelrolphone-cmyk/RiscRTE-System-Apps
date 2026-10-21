#pragma once
/* Invocation-local, explicitly requested passive remote battery scan. No
 * storage, connection, pairing, Bluetooth setting or automatic scan policy.
 * An address is only an observed identity, never authenticated ownership. */
#include "RiscRuntimeV1.h"
#include "RiscBluetoothSensorsV1.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PQB_MAX_CANDIDATES 32u
#define PQB_POLL_EVENTS 16u
#define PQB_FRESH_MS 10000u

typedef enum {
 PQB_IDLE, PQB_DISABLED, PQB_UNAVAILABLE, PQB_SCANNING, PQB_WAITING,
 PQB_CURRENT, PQB_STALE, PQB_COMPLETE, PQB_ERROR, PQB_RETAINED
} pqb_status;

typedef struct {
 uint8_t address[6],address_type;
 char name[32]; /* NUL-terminated printable ASCII, untrusted display label. */
 bool battery_valid; /* Latest snapshot has exactly one complete valid metric. */
 bool last_valid;
 uint8_t last_percent;
} pqb_candidate;

typedef struct {
 pqb_status status;
 bool active,retained,selected,last_valid,current;
 bool restoration_failed; /* Safe close, but prior radio state failed/unconfirmed. */
 uint8_t selected_address[6],selected_address_type,last_percent;
 uint32_t count;
 /* Private lifecycle and copied snapshot state. Do not write these fields. */
 const risc_runtime_api_v1 *runtime;
 const risc_bluetooth_sensors_v1 *api;
 risc_runtime_capability_v1 grant;
 uint64_t token;
 bool grant_owned;
 pqb_candidate rows[PQB_MAX_CANDIDATES];
 bool row_seen[PQB_MAX_CANDIDATES],row_duplicate[PQB_MAX_CANDIDATES];
 pqb_status row_status[PQB_MAX_CANDIDATES];
} pqb_session;

/* Zero-initialize before first use. Both reset functions refuse to discard
 * active, retained, token or grant custody. A successful reset forgets RAM data. */
bool pqb_init(pqb_session *s);
bool pqb_reset(pqb_session *s);
/* Only call for a user's explicit Scan/Refresh. allowed is the caller's
 * confirmed Bluetooth policy; false never acquires or changes a provider.
 * A permitted explicit attempt clears the prior restoration warning.
 * Uses bluetooth.sensors@1 instance 0 and its bounded 15-second scan. */
bool pqb_start(pqb_session *s,const risc_runtime_api_v1 *rt,bool allowed);
/* Call regularly while active. At most 16 provider events and 32 copied devices
 * per call. current is the last poll's freshness snapshot, not an ongoing link.
 * Completion/error closes and releases; it never restarts. */
bool pqb_poll(pqb_session *s);
/* Explicit address + type selection from copied rows only. No RF I/O. Never
 * matches a name, nearest peer, first row or a previous address's replacement.
 * Re-selecting the same identity preserves its cache; another identity never
 * inherits that cache. A stale valid row may be selected but is not current. */
bool pqb_select(pqb_session *s,const uint8_t address[6],uint8_t address_type);
/* Also the sole cleanup retry after retained custody. A failed close forbids
 * release; a failed release keeps the original grant. Ordinary I/O remains
 * forbidden until this succeeds. Cache and selection survive close/failure.
 * After successful close, status is checked before release. Restoration failure
 * or unreadable status sets restoration_failed/PQB_ERROR but safe cleanup still
 * returns true; it does not retain custody or automatically enable Bluetooth. */
bool pqb_close(pqb_session *s);
/* Copies at most capacity rows, returning copied count. NULL out queries count.
 * No provider I/O, even after cleanup fails. */
uint32_t pqb_candidates(const pqb_session *s,pqb_candidate *out,uint32_t capacity);
#ifdef __cplusplus
}
#endif
