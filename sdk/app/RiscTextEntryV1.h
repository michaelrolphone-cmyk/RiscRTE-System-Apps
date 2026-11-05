#pragma once
/* One separately loaded System-owned plain-text service. Requests and results
 * are copied; no app buffer, callback or pointer is retained after a call.
 * Serialized grant-owner calls only. A token is valid only until clean close.
 * Apps pause their presenter before open and resume it only after close OK.
 * This revision accepts printable ASCII, without email/URL/number modes. */
#include <stdint.h>
#include <stddef.h>
#define RISC_TEXT_ENTRY_CAPABILITY "ui.text-input"
#define RISC_TEXT_ENTRY_API_V1 1u
#define RISC_TEXT_ENTRY_BYTES 72u
#define RISC_TEXT_ENTRY_LABEL 40u
enum { RISC_TEXT_ENTRY_OK=0, RISC_TEXT_ENTRY_AGAIN=1, RISC_TEXT_ENTRY_INVALID=-1,
       RISC_TEXT_ENTRY_STALE=-2, RISC_TEXT_ENTRY_BUSY=-3, RISC_TEXT_ENTRY_UNAVAILABLE=-4,
       RISC_TEXT_ENTRY_RETAINED=-9 };
enum { RISC_TEXT_ENTRY_EDITING=0, RISC_TEXT_ENTRY_ACCEPTED=1, RISC_TEXT_ENTRY_CANCELLED=2 };
enum { RISC_TEXT_ENTRY_HARDWARE=1u, RISC_TEXT_ENTRY_PRESENTING=2u };
/* Optional result reason, returned only for a negotiated request. Back/Escape
 * remain plain CANCELLED; Home keeps the copied draft text and this flag. */
#define RISC_TEXT_ENTRY_HOME_CANCEL 4u
#define RISC_TEXT_ENTRY_REQUEST_HOME_REASON 1u
/* Negotiated display-only masking. Results still copy the caller's text. */
#define RISC_TEXT_ENTRY_REQUEST_MASKED 2u
#define RISC_TEXT_ENTRY_MASKED_TAG 0x544d534bu
#define RISC_TEXT_ENTRY_MASKED_VERSION 1u
#define RISC_TEXT_ENTRY_HOME_REASON_TAG 0x54484f4du
#define RISC_TEXT_ENTRY_HOME_REASON_VERSION 1u
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

/* Size-safe additive capability suffix. Old request/result layouts and all
 * callbacks are unchanged; old clients send reserved=0 and receive old flags. */
typedef struct {
    risc_text_entry_api_v1 base;
    uint32_t home_reason_tag, home_reason_version;
} risc_text_entry_api_v1_home_reason;
static inline int risc_text_entry_home_reason(const risc_text_entry_api_v1 *api) {
    if(!api || api->api_version!=RISC_TEXT_ENTRY_API_V1 ||
       api->struct_size<sizeof(risc_text_entry_api_v1_home_reason))return 0;
    const risc_text_entry_api_v1_home_reason *ext=(const risc_text_entry_api_v1_home_reason *)api;
    return ext->home_reason_tag==RISC_TEXT_ENTRY_HOME_REASON_TAG &&
        ext->home_reason_version==RISC_TEXT_ENTRY_HOME_REASON_VERSION;
}

/* Masking follows the existing home-reason prefix. An older provider must
 * never receive a secret request as unmasked plain text. */
typedef struct {
    risc_text_entry_api_v1_home_reason base;
    uint32_t masked_tag, masked_version;
} risc_text_entry_api_v1_masked;
static inline int risc_text_entry_masked(const risc_text_entry_api_v1 *api) {
    if(!risc_text_entry_home_reason(api) ||
       api->struct_size<sizeof(risc_text_entry_api_v1_masked))return 0;
    const risc_text_entry_api_v1_masked *ext=(const risc_text_entry_api_v1_masked *)api;
    return ext->masked_tag==RISC_TEXT_ENTRY_MASKED_TAG &&
        ext->masked_version==RISC_TEXT_ENTRY_MASKED_VERSION;
}

/* Opt-in long plaintext drafts. Existing non-control filename bytes are
 * preserved losslessly; keyboard keys remain printable ASCII and unsupported
 * preview glyphs are replaced only in the scene copy. Bounds count bytes.
 * The original 72-byte requests/results and all
 * existing suffix offsets stay unchanged. A client must validate this suffix
 * before calling these typed entrypoints; old providers reject no new data. */
#define RISC_TEXT_ENTRY_EXTENDED_BYTES 128u
#define RISC_TEXT_ENTRY_EXTENDED_TAG 0x54455854u
#define RISC_TEXT_ENTRY_EXTENDED_VERSION 1u
typedef struct {
    uint32_t api_version, struct_size, capacity, reserved;
    char label[RISC_TEXT_ENTRY_LABEL];
    char text[RISC_TEXT_ENTRY_EXTENDED_BYTES];
} risc_text_entry_request_v1_extended;
typedef struct {
    uint32_t struct_size, state, flags, revision;
    char text[RISC_TEXT_ENTRY_EXTENDED_BYTES];
} risc_text_entry_state_v1_extended;
typedef struct {
    risc_text_entry_api_v1_masked base;
    uint32_t extended_tag, extended_version;
    int32_t (*open_extended)(void *, const risc_text_entry_request_v1_extended *, uint64_t *);
    int32_t (*poll_extended)(void *, uint64_t, risc_text_entry_state_v1_extended *);
} risc_text_entry_api_v1_extended;
static inline const risc_text_entry_api_v1_extended *risc_text_entry_extended(const risc_text_entry_api_v1 *api) {
    if(!risc_text_entry_masked(api)||api->struct_size<sizeof(risc_text_entry_api_v1_extended))return NULL;
    const risc_text_entry_api_v1_extended *ext=(const risc_text_entry_api_v1_extended *)api;
    return ext->extended_tag==RISC_TEXT_ENTRY_EXTENDED_TAG&&
        ext->extended_version==RISC_TEXT_ENTRY_EXTENDED_VERSION&&
        ext->open_extended&&ext->poll_extended?ext:NULL;
}

/* Opt-in longer printable-ASCII drafts. Capacity includes the NUL terminator;
 * pass 241 for a 240-byte message. The complete extended ABI remains a frozen
 * prefix, so providers/clients predating this suffix continue to interoperate.
 * A missing or malformed suffix is unsupported, never a truncating fallback. */
#define RISC_TEXT_ENTRY_LONG_BYTES 256u
#define RISC_TEXT_ENTRY_LONG_TAG 0x544c4e47u
#define RISC_TEXT_ENTRY_LONG_VERSION 1u
typedef struct {
    uint32_t api_version, struct_size, capacity, reserved;
    char label[RISC_TEXT_ENTRY_LABEL];
    char text[RISC_TEXT_ENTRY_LONG_BYTES];
} risc_text_entry_request_v1_long;
typedef struct {
    uint32_t struct_size, state, flags, revision;
    char text[RISC_TEXT_ENTRY_LONG_BYTES];
} risc_text_entry_state_v1_long;
typedef struct {
    risc_text_entry_api_v1_extended base;
    uint32_t long_tag, long_version;
    int32_t (*open_long)(void *, const risc_text_entry_request_v1_long *, uint64_t *);
    int32_t (*poll_long)(void *, uint64_t, risc_text_entry_state_v1_long *);
} risc_text_entry_api_v1_long;
static inline const risc_text_entry_api_v1_long *risc_text_entry_long(const risc_text_entry_api_v1 *api) {
    if(!risc_text_entry_extended(api)||api->struct_size<sizeof(risc_text_entry_api_v1_long))return NULL;
    const risc_text_entry_api_v1_long *ext=(const risc_text_entry_api_v1_long *)api;
    return ext->long_tag==RISC_TEXT_ENTRY_LONG_TAG&&
        ext->long_version==RISC_TEXT_ENTRY_LONG_VERSION&&
        ext->open_long&&ext->poll_long?ext:NULL;
}

/* Opt-in resident-policy handoff. The complete long/plain/masked ABI remains
 * a frozen prefix. Enable immediately after open, before the first input poll.
 * Snapshot reports logical activity/held input independently of presentation.
 * Pause releases display ownership, retaining navigation claims, bounded
 * passive input subscriptions and copied draft/layer/caps/focus state. Routine policy
 * resume preserves queued edges; FOCUS_CHANGED discards intervening-focus
 * events and neutralizes held input after actual sleep. It never accepts or
 * saves a draft. BUSY means input changed; poll again instead of dispatching.
 * AGAIN retains exact custody and requires another pause call. After OK the
 * client may checkpoint, then resume or close; close never requires resume.
 */
#define RISC_TEXT_ENTRY_CHECKPOINT_TAG 0x54435031u
#define RISC_TEXT_ENTRY_CHECKPOINT_VERSION 1u
#define RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING 1u
#define RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY 2u
#define RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY 4u
#define RISC_TEXT_ENTRY_CHECKPOINT_PAUSED 8u
#define RISC_TEXT_ENTRY_RESUME_REDRAW 1u
#define RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED 2u
typedef struct {uint32_t struct_size,flags;} risc_text_entry_checkpoint_state_v1;
typedef struct {
    risc_text_entry_api_v1_long base;
    uint32_t checkpoint_tag,checkpoint_version;
    int32_t (*checkpoint_enable)(void *,uint64_t);
    int32_t (*checkpoint_snapshot)(void *,uint64_t,risc_text_entry_checkpoint_state_v1 *);
    int32_t (*pause)(void *,uint64_t);
    int32_t (*resume)(void *,uint64_t,uint32_t);
    /* Legacy/idle policy without an explicit continuity promise must release
     * all input subscriptions and navigation before native power preparation. */
    int32_t (*pause_for_focus_change)(void *,uint64_t);
} risc_text_entry_api_v1_checkpoint;
static inline const risc_text_entry_api_v1_checkpoint *risc_text_entry_checkpoint(const risc_text_entry_api_v1 *api){
    if(!risc_text_entry_long(api)||api->struct_size<sizeof(risc_text_entry_api_v1_checkpoint))return NULL;
    const risc_text_entry_api_v1_checkpoint *ext=(const risc_text_entry_api_v1_checkpoint *)api;
    return ext->checkpoint_tag==RISC_TEXT_ENTRY_CHECKPOINT_TAG&&
        ext->checkpoint_version==RISC_TEXT_ENTRY_CHECKPOINT_VERSION&&ext->checkpoint_enable&&
        ext->checkpoint_snapshot&&ext->pause&&ext->resume&&ext->pause_for_focus_change?ext:NULL;
}
