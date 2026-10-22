#ifndef PORTABLE_SERIAL_CLIENT_H
#define PORTABLE_SERIAL_CLIENT_H
/* App-local, opt-in stream client, not a monitor application or raw serial ABI.
 * Build PortableSerialClient.c with PORTABLE_SERIAL_STREAMS. Use only on the
 * owner task, within one invocation. Init a fresh object once; never copy,
 * reinitialize a live/retained object, mutate its fields, or persist handles. */
#include "RiscStreamClientV1.h"
#include "RiscSerialStreamSessionV1.h"
#include "RiscSerialPortV1.h"
#include "T5SerialPortApi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PSC_CHUNK RISC_STREAM_CHUNK_V1
#define PSC_QUEUE_CAPACITY PSC_CHUNK
#define PSC_STALL_MS 2000u
enum {
 PSC_OK=0, PSC_AGAIN=1, PSC_EOF=2,
 PSC_INVALID=-1, PSC_DENIED=-2, PSC_LIMIT=-3, PSC_UNSUPPORTED=-4,
 PSC_IO=-5, PSC_DISCONNECTED=-6, PSC_CANCELLED=-7, PSC_BUSY=-8,
 PSC_CLOSED=-9, PSC_TIMEOUT=-10, PSC_RETAINED=-11
};
typedef struct portable_serial_client {
 const struct portable_serial_client *self;
 const risc_runtime_api_v1 *runtime;
 risc_stream_client_v1 streams;
 risc_runtime_capability_v1 grant;
 risc_serial_device_v1 inventory[RISC_SERIAL_INVENTORY_MAX_DEVICES];
 size_t inventory_count;
 uint64_t instance,device,generation;
 risc_stream_session_t session;
 risc_stream_client_handle_t rx_stream,tx_stream;
 t5_serial_config_t config;
 uint64_t rx_bytes,tx_bytes; /* Saturating accepted-byte counters, not flush. */
 uint8_t queue[PSC_QUEUE_CAPACITY];
 size_t queued,sent;
 uint32_t queued_at,control_budget_ms;
 int result;
 bool initialized,stream_mode,connected,cleanup_required,halted,inventory_loaded,dtr,rts;
} portable_serial_client;
/* Init does not acquire a provider. Zero instance requires exactly one
 * authorized provider, as defined by Runtime; it never picks a provider. */
bool portable_serial_init(portable_serial_client *,const risc_runtime_api_v1 *);
bool portable_serial_init_instance(portable_serial_client *,const risc_runtime_api_v1 *,uint64_t instance);
/* Total allocated stream-control budget per public operation, including its
 * one optional error close, is 1..1000 ms. Open needs at least 2 ms because
 * Runtime may spend the same sub-budget again on its internal open rollback.
 * No retries, sleeps or yields. These are cooperative callback budgets, not
 * preemption or a wall-clock bound on capability acquisition/release. */
bool portable_serial_set_control_budget(portable_serial_client *,uint32_t milliseconds);
/* Acquire the selected capability and copy a complete, validated inventory.
 * Failed/partial/malformed snapshots return IO and leave the prior snapshot
 * untouched. No endpoint is opened and no device is automatically selected. */
bool portable_serial_refresh(portable_serial_client *);
/* Explicit index into the last successful snapshot. If none is loaded, take
 * one snapshot first. Open passes its exact device AND generation to Runtime;
 * it never refreshes/rebinds an already selected index or retries on detach. */
bool portable_serial_open(portable_serial_client *,size_t index,const t5_serial_config_t *);
bool portable_serial_configure(portable_serial_client *,const t5_serial_config_t *);
bool portable_serial_control(portable_serial_client *,bool dtr,bool rts);
/* Copy one bounded NUL-terminated line plus CRLF. Only one outstanding line.
 * now_ms is caller-supplied same-boot uint32 monotonic time; wraps are allowed.
 * A stalled queue expires 2000 ms after last accepted progress. Cancellation
 * discards only unaccepted app bytes, never bytes already accepted by Runtime. */
bool portable_serial_queue(portable_serial_client *,const char *line,uint32_t now_ms);
size_t portable_serial_tick(portable_serial_client *,void *rx,size_t capacity,uint32_t now_ms);
void portable_serial_cancel(portable_serial_client *);
/* Idempotent after a clean close; reconnect requires another explicit open.
 * On cleanup_required: return promptly from the invocation, requesting its
 * retain_invocation fence if needed. Never close/release/retry/free this client
 * or poll/yield providers. This client never attempts cleanup after RETAINED. */
bool portable_serial_close(portable_serial_client *);
#ifdef __cplusplus
}
#endif
#endif
