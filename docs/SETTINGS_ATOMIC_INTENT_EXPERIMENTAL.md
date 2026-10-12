# Experimental Settings preference intents

Based on System `a5bd60fc5b7e7b62e3e08531860ce7e8ec855b89`. This increment
adds an explicit, default-off `--experimental-atomic-settings` selection for
X4 native-time resident Settings with host-owned policy and the read-only
Home/desk-lock row. It is not a product configuration, boot activation,
source-selection policy, SD merge policy, firmware delivery or hardware result.
Watch and ordinary flag-off builds retain their original write behavior.

## Selected controller boundaries

`settings_activate` captures a namespace-1 Core revision and the exact expected
record **before** draft loads at entry to the editor. Every read used by that
editor, including Set Time's timezone and RTC basis, uses that captured revision.
The time-zone editor resets an earlier unconfirmed draft on re-entry; stale
saved drafts are never rebased implicitly. A concurrent namespace writer makes
the Save stale. A new editor entry is required to obtain a new revision.

Only these explicit Settings actions submit a complete canonical value:

- Time format (`time_format`)
- Time-zone selection (`time_zone`)
- Clock face (`desk_clock_face`)
- Desk orientation (`desk_direction`)
- UI flip (`reader_flip_ui`)
- Reader language (`reader_language`)
- Idle sleep timer (`sleep_idle`)
- RTC basis (`rtc_basis`), after explicit confirmed Set Time and independently
  checked physical RTC/native-clock completion

An explicit same-value/default Save records an intention and complete value too.
Passive loads, virtual defaults, browsing, editor changes and Cancel do not.
This differs deliberately from some legacy same-value no-op optimizations.

The exact acquisition is `storage.key-value.atomic@1`, instance 1. Its state
must report namespace 1 and the copied-async/native-durable flags and sufficient
bounded capacities. A clean missing endpoint leaves editing unavailable. A
malformed/uncertain grant retains custody. The Settings KV1-shaped view reads
only the same Core and rejects every put. No selected Save falls back to a legacy
writer. Remaining legacy KV1 acquisition is for the existing shared read-only
paper preference consumers; Runtime same-Core read routing remains required.

## Copied operation and semantic encoding

`PortablePreferenceAtomic.h` owns bounded copies of the expected record,
complete desired record and versioned `PFI0001` intent. The atomic provider
copies the key, mutation and intent before accepting a token. Nothing on the
native worker borrows UI storage. One complete map change and one semantic
intent publish in the same native Core transaction.

PFI0001 is exactly 176 bytes, zero-initialized, with little-endian words:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 8 | `PFI0001` plus NUL |
| 8 | 1 | Origin: explicit USER = 1 |
| 9 | 1 | Operation: complete PUT = 1 |
| 10 | 1 | Expected presence (0/1) |
| 11 | 1 | Reserved zero |
| 12 | 16 | NUL-terminated logical preference key, zero padded |
| 28 | 4 | Expected byte count, 0..64 |
| 32 | 4 | Desired byte count, 1..64 |
| 36 | 8 | Captured local Core revision |
| 44 | 64 | Exact expected bytes, zero padded |
| 108 | 64 | Complete desired bytes, zero padded |
| 172 | 4 | CRC32/IEEE of bytes 0..171 |

The local revision is a concurrency fence, not a cross-source identity or merge
policy. Expected presence/value and the logical preference key carry the
semantic comparison. Non-user origin enum values AUTOMATIC=2, ROLLBACK=3 and
BOOT=4 are reserved and explicitly rejected by this selected UI helper. They
are not authority to submit those operations. Generic preference put helpers
are unchanged and do not synthesize intents.

## Asynchronous completion and custody

The foreground controller submits once and services input while collecting.
Busy before acceptance can retry the same copied operation at the same revision;
no failure, stale or uncertain operation is replayed automatically. Once accepted,
repeated Confirm does not submit again. Back/Home requests cancellation and
collects the actual terminal receipt. If cancellation loses the publication race,
the controller reports the successful local save. Native success explicitly does
not confirm an SD checkpoint.

Resident checkpoints are blocked while the atomic controller owns a submission.
A malformed receipt, provider retention, a failed cancel/collect, or an accepted
but ambiguous submission retains the token and complete invocation. The early
finalizer attempts cancel/collection before any ordinary teardown; a still-pending
close retains custody instead of releasing its grant or freeing state. Checked
capability release must clear the grant.

## Set Time is not a cross-device transaction

Before touching a physical clock, selected Settings checks whether its captured
Core revision is still current. This avoids a *known* stale draft, but cannot
lock a concurrent writer out while physical hardware is changing.

The existing checked RTC write/readback and native seed/readback execute first.
The selected clocks-only helper acquires no KV writer. Only after both clocks
are independently verified and their helper is cleanly closed does the controller
submit the complete `rtc_basis` mutation and intent against its original revision.

- RTC failure: native clock and basis are not attempted
- Native failure after RTC success: RTC may already be changed; basis is not attempted
- Basis stale/cancelled/failed/uncertain after verified clocks: both clocks may
  already be changed; the UI reports their verified outcomes and the separate
  basis failure explicitly
- Successful basis receipt: all three outcomes are confirmed independently;
  no claim of physical/native/Core atomicity or SD durability is made

There is no automatic clock rollback or basis retry.

## Remaining namespace-1 controllers

This increment does **not** claim a complete namespace-1 migration. Remaining
explicit writers include Home/shared Quick brightness/restore metadata, tone,
DND, contexts and radio controls; Battery BLE broadcast; BLE sensor aliases,
Bluetooth enable and its rollback; and Contexts settings. Automatic low-battery
consumed crossings/rearming and Home/foreground context rules remain distinct
non-user operations and unchanged. Namespace-1 product activation must wait
until those boundaries and origins are handled, with existing semantics and
no generic-put loophole. Runtime must fence unselected legacy writes and keep
all readers on the same Core. Source-conflict resolution remains unselected.

## Reproducible checks

Build with the usual X4 resident Settings flags plus
`--experimental-atomic-settings`. Use the canonical Runtime SDK and single-job
Xtensa GCC 8.4.0 toolchain. The build emits explicit atomic selection evidence.

- `python scripts/test_settings_atomic.py --target TARGET --output OUT`
- `python scripts/test_settings_atomic.py --target TARGET --output OUT --sanitize`
- `python -m unittest discover -s tests -q`

The live-controller fixture enters the actual Settings API with real input/view
code, checked RTC/native helpers, and scheduled atomic provider doubles. It
covers every selected key, same-value intentions, intervening same/unrelated
writers, stale Set Time before and during clock writes, pre-submit cancellation,
accepted cancellation and commit races, repeated confirmation, Busy, capacity,
I/O, uncertainty, malformed receipts, missing/wrong capability, custody retention,
close while pending, checked release, and explicit automatic-origin rejection.
Host simulation and target ELF/import checks do not qualify hardware or product
activation.
