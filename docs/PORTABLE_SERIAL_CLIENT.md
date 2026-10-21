# Bounded portable serial client candidate

`PortableSerialClient.h` and `PortableSerialClient.c` implement the missing
app-local client used by the Runtime serial-stream witness. This is a library,
not a delivered serial monitor, flashing tool, default application, installed
provider or product selection. It performs no hardware discovery or host
polling itself. No Runtime/provider ABI changes are made.

## Exact source contract

- System base: `0651a55513b0aa0dff1b6a9072303def08a97378`.
- Runtime SDK and integration witness: `6a3b3d46f5b2857eacc6e9863799dec1f466b036`.
- Native Reader serial declarations: `c98e4222048244b1e729869ed47aa99130025fd3`.

`lib/PortableApps/SERIAL_CLIENT_PROVENANCE.json` locks every copied declaration
to its original path, commit and SHA-256. The six copied headers are byte-identical,
not a new ABI or a broad historical SDK overlay. The existing System Runtime
header is not changed; tests stage the exact pinned canonical Runtime headers.
The T5 serial and stream declarations supply compatible configuration types
only. Their legacy getters and transfer interfaces are never called.

## Selection and invocation ownership

Build the source with `PORTABLE_SERIAL_STREAMS`, the PortableApps include path
and the NativeApps include path. Without the opt-in, init returns UNSUPPORTED
without making any external call.

1. Initialize a fresh client once in the active owner invocation. The default
   initializer requests instance zero, which means exactly one authorized
   provider, never the first or last provider. Use `portable_serial_init_instance`
   to select an exact nonzero declared capability instance.
2. `portable_serial_refresh` acquires that capability and copies its complete
   inventory for presentation. A failed or malformed snapshot is IO, not an
   empty set or a detach. The prior complete snapshot remains unchanged.
3. `portable_serial_open(client, index, config)` explicitly selects an entry
   from that snapshot. If no snapshot is loaded, it takes one first. The exact
   provider device and generation are passed to Runtime. An invalid or absent
   index does not fall back to another device. Provider-side validation closes
   the enumeration/open race.
4. Tick checks that same device/generation through the broker before touching
   RX/TX. Unknown inventory pauses movement. Confirmed absence or a changed
   generation closes once and reports DISCONNECTED. There is no automatic
   reconnect or device reselection.
5. Clean close releases the session before the grant. Only a fresh explicit
   open can reconnect. Copied objects are rejected; a fresh Runtime table must
   match the original invocation context before borrowed provider-table access.
   Runtime remains authoritative for grants and opaque handles.

All public struct fields are implementation state or read-only status; callers
must not mutate, copy, persist or reinitialize a live/retained object. The object
and Runtime table must remain alive for the invocation. Init deliberately
supports the existing target witness's uninitialized stack object, so calling
init again is not a reset/cleanup API.

## Transfers, framing and budgets

Framing accepts 300–3000000 baud, 5–8 data bits, parity NONE through SPACE,
one or two stop bits and no flow control, matching the current serial witness.
The provider can still reject supported-looking framing. A failed framing or
control-line operation closes the session once, rather than assuming physical
state or silently retrying. DTR/RTS state changes only after confirmed success.

`portable_serial_queue` copies one line of at most 510 bytes and appends CRLF.
It rejects another line while one is outstanding. Tick makes at most one write
and one read, each at most 512 bytes, with no waits/yields/retry loops. Partial
progress preserves order. Counters saturate and count accepted bytes, not
physical transmission or a flush guarantee. Cancel discards only unaccepted
local bytes. A queue expires after 2000 ms without accepted write progress,
using caller-supplied same-boot wrapping monotonic milliseconds.

The default allocated stream-control budget is 1000 ms per public operation;
the setter accepts 1–1000 ms. Configure/control/check allocate the rounded-up
half to the call and reserve the rest for at most one checked error close.
The pinned Runtime broker may give its open budget again to an internal grant
rollback close. Therefore open supplies the rounded-down half; a total budget
of one millisecond rejects open locally before acquiring anything. Existing
session controls can still use one millisecond, but an error that requires a
close when no budget remains must retain custody. Tests simulate the broker's
double allocation and assert totals never exceed the configured limit.

These are cooperative callback budgets, not preemption, physical USB timing
qualification or an end-to-end wall-clock bound on capability acquire/release.
Runtime detects callback overruns and fences the invocation.

## Retention and failures

RETAINED is terminal even when open returns no app session token. Malformed
outputs, counts, control statuses, uncertain close/release and stale contexts
with custody latch `cleanup_required`. No later client method makes an external
call, clears the queue/custody evidence, retries or releases anything. Return
promptly from the invocation, using Runtime's `retain_invocation` fence when
needed; do not free the client, poll providers or attempt recovery in place.

A failed capability acquire with an empty output can be a healthy refusal
(wrong/ambiguous instance or no authorized provider). A provider-free Runtime
getter verifies the same live invocation before reporting DENIED. If that
verification fails, or the failed acquire supplies malformed/nonempty output,
custody is retained. This check is not performed after an explicit RETAINED.

## Reproducible software checks

From System, with the exact Runtime checkout:

```sh
python3 scripts/test_portable_serial_client.py --runtime /path/to/RiscRTE --output build/serial-normal
python3 scripts/test_portable_serial_client.py --runtime /path/to/RiscRTE --output build/serial-sanitized --sanitize
```

In a ptrace-constrained runner where LeakSanitizer cannot start, prefix the
sanitized command with `ASAN_OPTIONS=detect_leaks=0`. ASan and UBSan remain active;
this does not constitute leak checking. The evidence records these options.

Each run checks source hashes, strict C compilation, invalid arguments, table
prefixes, complete/partial/malformed inventories, explicit instance/index,
unknown-inventory recovery, partial reads/writes, bounded queues, cancellation,
wrap-safe stalled deadlines, saturating counters, detach/reconnect, stale
objects/invocations, failed/retained control and cleanup, malformed results and
no external call after retention. Opt-out and C++ header checks also run.

Finally the unchanged pinned Runtime integration runner loads the production
client and software serial provider through the real broker, graph, module and
byte queues: 11 modes under both eager and demand activation, 22 cases per run.
Retained cases prove no fini/quiesce/stop and exact queue custody. Logs, commands,
input hashes and clean/dirty checkout state are written to the output directory.

These checks do not qualify an X4 USB/controller/PHY handoff, power/VBUS, a
physical UART/CDC device, product installation, flashing or monitor UI behavior.
