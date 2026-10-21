#ifndef PORTABLE_MOUSE_CONSUMER_H
#define PORTABLE_MOUSE_CONSUMER_H
/* Opt-in app-local copied-event consumer, NOT a producer executor or UI input
 * binding. Build with PORTABLE_USB_MOUSE. All calls must be serialized with
 * the provider on the current native app owner task, within one invocation.
 * Never copy, persist, mutate, or reinitialize a live/retained object. */
#include "RiscStreamClientV1.h"
#include "RiscUsbMouseV1.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
 PMC_EMPTY=0, PMC_EVENT=1, PMC_BASELINE=2, PMC_RESET=3, PMC_PAUSED=4,
 PMC_INVALID=-1, PMC_UNSUPPORTED=-2, PMC_DENIED=-3, PMC_GAP=-4,
 PMC_MALFORMED=-5, PMC_UNAVAILABLE=-6, PMC_CLOSED=-7, PMC_RETAINED=-8
};
/* Every call initializes this copied result. Only PMC_EVENT contains an event;
 * deliver its relative counts exactly once, never from previous/current.
 * BASELINE replaces held state, not a click/press replay. RESET or an error
 * can release previous buttons: reconcile by (device,session), never by ORing
 * devices. No snapshot or reset reconstructs lost relative/click history. */
typedef struct {
 risc_usb_mouse_event_v1 event;
 risc_usb_mouse_state_v1 previous[RISC_USB_MOUSE_MAX_DEVICES];
 risc_usb_mouse_state_v1 current[RISC_USB_MOUSE_MAX_DEVICES];
 size_t previous_count, current_count;
} portable_mouse_frame;
typedef struct portable_mouse_consumer {
 const struct portable_mouse_consumer *self;
 const risc_runtime_api_v1 *runtime;
 risc_runtime_capability_v1 grant;
 const risc_usb_mouse_api_v1 *mouse;
 uint64_t invocation, instance, filter, subscription, sequence;
 risc_usb_mouse_state_v1 states[RISC_USB_MOUSE_MAX_DEVICES];
 size_t count;
 int result;
 bool initialized, focused, needs_snapshot, cleanup_required, fence_accepted;
} portable_mouse_consumer;
/* Init a fresh object once. No capability acquisition. stream_client is used
 * only as the existing provider-free invocation tag; no stream is opened. */
bool portable_mouse_init(portable_mouse_consumer *, const risc_runtime_api_v1 *,
                         uint64_t instance, uint64_t device_or_zero);
/* Explicit lifecycle operations, NOT UI callbacks: acquire/release may run
 * provider start/quiesce and have no app-local elapsed-time guarantee. Zero
 * instance requires a unique authorized provider. No implicit fallback. */
int portable_mouse_open(portable_mouse_consumer *, portable_mouse_frame *);
int portable_mouse_close(portable_mouse_consumer *, portable_mouse_frame *);
/* At most one next OR one snapshot plus one status per call; no poll, sleeps,
 * yields, raw I/O, allocation, callbacks or retry loops. Production scheduling
 * and serialization still need a separately qualified producer executor. */
int portable_mouse_next(portable_mouse_consumer *, portable_mouse_frame *);
/* Focus loss clears held state immediately without a provider call. Resume
 * requires a new baseline on next, discarding all unfocused queued history. */
int portable_mouse_focus(portable_mouse_consumer *, bool, portable_mouse_frame *);
/* RETAINED is terminal. Preserve this object, exact subscription and grant;
 * return promptly, do not retry cleanup/free/poll. A same-owner failure tries
 * Runtime's retain_invocation once. A stale invocation never fences a newer
 * owner and never dereferences its old borrowed provider pointer. */
#ifdef __cplusplus
}
#endif
#endif
