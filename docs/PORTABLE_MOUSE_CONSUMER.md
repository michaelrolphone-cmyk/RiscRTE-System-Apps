# Copied USB mouse consumer candidate

**Unvalidated checkpoint, 2026-10-21:** implementation/qualification was stopped
for durable GitHub capture. Only the focused strict-C unit fixture was executed.
The integration fixtures are uncompiled drafts and their runner is absent; the
commands in the qualification section are therefore not runnable end-to-end yet.
See `MOUSE_CONSUMER_CHECKPOINT_2026-10-21.md` for exact state and remaining work.

`PortableMouseConsumer` is an opt-in, app-local lifecycle and copied-event
helper above the existing `usb.hid.mouse@1` ABI. It is **not an integrated
mouse feature**, a new provider, a cursor, or a producer executor. No application
manifest, product selection, accepted binary, Runtime implementation or provider
source changes accompany it.

## Placement, ownership and exact inputs

- System base: `f9d55dbeb941ba40fb965f53172ce3e1b85af376`.
- Native Runtime: `6a3b3d46f5b2857eacc6e9863799dec1f466b036`.
- Recovered Reader evidence head: `59177418ea746c7605b7b5a5aec501e70bdae80f`.
- Mouse production source: `c334919a05d393b7b3a536fceb117cfbb573a4ef`.
- Existing [Reader PR468](https://github.com/michaelrolphone-cmyk/T5S3-Reader/pull/468)
  and its parent [PR467](https://github.com/michaelrolphone-cmyk/T5S3-Reader/pull/467)
  remain open draft work, preserved read-only. Their age and empty review/assignee
  lists are not evidence of abandonment or an ownership handoff.

System is the existing home of native app-local helpers (`lib/PortableApps`),
including `PortableSerialClient`. This helper remains unbound there rather than
choosing a product owner or modifying the upstream PR stack. Product placement,
PR ownership coordination and promotion into current Drivers packaging still
require reconciliation before publication/integration.

`MOUSE_CONSUMER_PROVENANCE.json` pins every included provider declaration. The
four USB headers are new byte-identical copies; three transitive provider headers
and two native stream declarations already match. The legacy
`T5ProviderCapabilityApi.h` is unchanged and its getter is never called. Current
native apps instead use `RiscRuntimeV1.acquire/release` with opaque invocation
grants. The existing System Runtime header already has the needed prefix;
tests also compile against the complete exact Runtime SDK, without replacing
the System header or overlaying a historical SDK on product builds.

## Contract and use

Build the helper with `PORTABLE_USB_MOUSE` and the PortableApps include path.
Without that opt-in, init returns UNSUPPORTED without any external call.

1. Initialize a fresh, exclusively owned object once during the active native
   invocation. It cannot be copied, persisted, reinitialized while live/retained,
   or have its fields changed by callers. Its grant and subscription cannot be
   released by another owner. The object and Runtime table live until invocation
   teardown or retained custody.
2. Explicitly open with the configured capability instance and optional device
   filter. Instance zero means a unique authorized provider, not first-match
   fallback. Runtime's existing manifest/grant policy remains authoritative.
3. The existing `stream_client` getter provides a Runtime-minted unique
   invocation identity. It is used only as a provider-free identity check,
   never to open or operate a stream. The helper rechecks it before borrowed
   provider access and after acquire, including failed acquire. It does not
   mistake a still-live invocation for proof that a released grant is valid.
4. Subscribe, then snapshot at the ABI's maximum four-state capacity. Failed
   capacity/snapshot attempts return GAP with no partial state. Only a fully
   validated snapshot establishes the baseline. No retry loop is hidden in a
   public call.
5. Each next step takes at most one copied event or one snapshot, plus one local
   provider status read. A successful EVENT contains exactly one event. Deliver
   its relative x/y/wheel/pan counts once; do not accumulate or replay them from
   state arrays. Filtered subscriptions may legitimately skip global sequence
   values, but zero, duplicate and regressing sequence records are rejected.
6. Identity is `(device, session)`; composite-device interfaces are separate.
   The previous/current copied states expose held-button reconciliation. REPORT
   transitions must agree with the prior mask. DISCONNECTED delivers releases
   once. Missing sessions in a replacement baseline release their old held
   state. A BASELINE is not a synthetic press/click event.
7. GAP, malformed records and unavailable provider state clear assumptions
   immediately. GAP recovery occurs on a later next step. A successful snapshot
   discards the subscription's queue and replaces the baseline; no lost relative
   movement, scroll or click history can be reconstructed. Malformed snapshots
   stay unready even if a faulty provider acknowledged its GAP.
8. Focus loss produces an immediate previous-to-empty RESET without a provider
   call. Unfocused next calls read nothing. Focus regain requires snapshot,
   which discards queued pre-focus events. Repeated same-focus calls do not
   reset or replay state. Product focus policy is deliberately not selected.
9. Close clears held input, unsubscribes once, then releases the native grant.
   Failed unsubscribe preserves both tokens and never calls release. A failed
   or malformed release preserves the original grant, permanently ending all
   borrowed-pointer access. Subsequent calls make no external calls.

The helper checks structure prefixes, callbacks, nonzero identity, reserved
fields, filters, duplicate snapshot sessions/interfaces, event kinds and button
transition consistency. Provider context may be null, as in the real provider.
Native apps remain trusted code: validation is not an arbitrary-pointer sandbox.

## Retention and execution boundaries

Uncertain cleanup is terminal. For a confirmed current owner the helper attempts
`retain_invocation` once, retaining images and dependencies without more provider
calls. A stale invocation is rejected before dereferencing the borrowed table
and never fences the newer owner. After RETAINED, return promptly; do not retry,
free, reinitialize, poll, or attempt recovery in place. A failed fence is not
proof that cleanup completed. Keep exact custody evidence.

The non-I/O promise applies only to copied `next`/`snapshot`/`status`, focus and
local reconciliation on the existing ABI. **Open and close are lifecycle work,
not UI callbacks.** Native acquire may activate/service providers; release may
synchronously quiesce the graph and its lower dependencies. There is no helper
wall-clock guarantee for those operations.

All calls must run on the owner task serialized with the provider executor,
including any producer. The helper never invokes mouse `poll`, Runtime yield,
raw HID, scheduler sleep, callbacks or a worker task. Obtaining a native grant
does not schedule the recovered mouse provider: its descriptor has no generic
poll/service suffix. A safe producer executor is still missing. Mouse poll has
up to 20 ms report work, a 1 ms scheduler yield, separately bounded 100 ms raw
HID descriptor/control calls (possibly two during setup), and inherited lower
cleanup bounds. None fits an assumed UI callback budget merely because event
consumption is cheap.

The unchanged raw HID close still invokes a void host release. Its true return
cannot prove physical quiescence. Existing checked-host/controller work,
lower-host quarantine and retained graph custody remain mandatory and separate.
No X4 VBUS/source-route/current-limit/role safety, physical interoperability,
input routing, native package admission or product acceptance is claimed.

## Reproducible qualification

Run with the exact clean upstream checkouts:

```sh
python3 scripts/test_portable_mouse_consumer.py --runtime /path/to/RiscRTE \
  --reader /path/to/Reader-mouse --output /path/to/evidence/normal
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_portable_mouse_consumer.py \
  --runtime /path/to/RiscRTE --reader /path/to/Reader-mouse \
  --output /path/to/evidence/sanitized --sanitize
```

The runner verifies exact source locks and ABI bytes, compiles strict C and C++
header/opt-out checks, runs fault-injection unit coverage and the software native
graph composition fixture, and records input hashes, compiler commands and
logs. The production provider is unchanged; any HID/clock fixture and manual
poll scheduling are test-only. LeakSanitizer is disabled in ptrace-constrained
runners; ASan/UBSan success is not leak or hardware qualification. No target
binary, target timing, remote CI, publication, radio or physical USB test is run.
