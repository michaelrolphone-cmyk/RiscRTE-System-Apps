#pragma once
/* One separately loaded System-owned plain-text service. Requests and results
 * are copied; no app buffer, callback or pointer is retained after a call.
 * Serialized grant-owner calls only. A token is valid only until clean close.
 * Apps pause their presenter before open and resume it only after close OK.
 * This revision accepts printable ASCII, without email/URL/number modes. */
#include <stdint.h>
#define RISC_TEXT_ENTRY_CAPABILITY "ui.text-input"
#define RISC_TEXT_ENTRY_API_V1 1u
#define RISC_TEXT_ENTRY_BYTES 72u
#define RISC_TEXT_ENTRY_LABEL 40u
enum { RISC_TEXT_ENTRY_OK=0, RISC_TEXT_ENTRY_AGAIN=1, RISC_TEXT_ENTRY_INVALID=-1,
       RISC_TEXT_ENTRY_STALE=-2, RISC_TEXT_ENTRY_BUSY=-3, RISC_TEXT_ENTRY_UNAVAILABLE=-4,
       RISC_TEXT_ENTRY_RETAINED=-9 };
enum { RISC_TEXT_ENTRY_EDITING=0, RISC_TEXT_ENTRY_ACCEPTED=1, RISC_TEXT_ENTRY_CANCELLED=2 };
enum { RISC_TEXT_ENTRY_HARDWARE=1u, RISC_TEXT_ENTRY_PRESENTING=2u };
typedef struct {
    uint32_t api_version, struct_size, capacity, reserved;
    char label[RISC_TEXT_ENTRY_LABEL];
    char text[RISC_TEXT_ENTRY_BYTES];
} risc_text_entry_request_v1;
typedef struct {
    uint32_t struct_size, state, flags, revision;
    char text[RISC_TEXT_ENTRY_BYTES];
} risc_text_entry_state_v1;
typedef struct {
    uint32_t api_version, struct_size;
    void *context;
    int32_t (*open)(void *, const risc_text_entry_request_v1 *, uint64_t *session);
    /* Copies current state. ACCEPTED/CANCELLED remain readable until close.
     * One bounded input/presentation step; never sleeps or saves app data. */
    int32_t (*poll)(void *, uint64_t, risc_text_entry_state_v1 *);
    /* Cancels an editing session. AGAIN means keep grant/token and retry after
     * yield; RETAINED forbids further service I/O and app/host unmapping. */
    int32_t (*close)(void *, uint64_t);
} risc_text_entry_api_v1;
