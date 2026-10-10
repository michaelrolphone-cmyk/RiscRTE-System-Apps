# Files BLE setup client

`PortableFileSetup.h/.c` is an allocation-free, foreground app client for the
canonical `bluetooth.session-setup@1` API. The copied header comes byte-for-byte
from RiscRTE-Drivers commit `09b56e84e723368d10c4f14cec494e4da0bbead0`; its path and
SHA-256 are recorded in `lib/PortableApps/SOURCES.json`. This client does not add
a Runtime ABI, start HTTP/Wi-Fi, acquire file exports, infer filesystem roots,
store credentials, or implement Bluetooth pairing.

## Integration

1. Zero-initialize one `portable_file_setup` for the current app invocation. Call
   `portable_file_setup_init` with Runtime's capability prefix and a pure cached
   owner predicate. The predicate must never perform provider/diagnostic I/O.
2. First establish an authorized, ready, finite HTTP(S) WebDAV session. In direct
   response to the user's setup action, build a canonical descriptor containing
   its URL, temporary username/password, session ID, and remaining duration.
   Call `portable_file_setup_start` with the exact nonzero authorized provider
   instance, a printable display name, and a distinct 1–300000 ms setup duration.
   Start copies the descriptor and performs no external I/O. Wipe the caller's
   temporary descriptor immediately afterward, including on rejected start.
3. Call `portable_file_setup_step` at least every 20 ms while active. Startup
   acquires a grant and opens in separate calls. Before open, the client deducts
   elapsed time from the copied HTTP duration; native clocks need not share its
   epoch. Open is attempted only once. As soon as it returns, every local
   descriptor byte is explicitly erased, even for a failed or BUSY result.
   The provider independently owns and enforces its copied finite deadline.
4. Read the pure local `portable_file_setup_get_status` snapshot for display.
   `RISC_SETUP_PAIR_CONFIRM` exposes one copied generation and numeric value.
   Render the number as exactly six digits, including leading zeros. Send that
   exact displayed pair to `portable_file_setup_confirm` only after explicit
   user accept/reject; never automatically accept or retry an earlier choice.
5. Close BLE before HTTP or the network on cancellation, expiry, transition,
   sleep, handoff, or app exit. Repeat bounded `portable_file_setup_close` calls
   until OFF or UNAVAILABLE. Native close must return OK before the next close
   call releases the Runtime grant. No pending state constitutes successful
   cleanup. Keep the client object, owner context, provider and dependencies
   alive until cleanup completes.

Active step performs at most one `poll(max_events=16)` and one `status` call.
Each acquire, open, confirmation, close and grant release is a single external
call with a pure ownership check before and after it. There are no client loops,
sleeps, allocation, logs, background work or automatic reopens. The canonical
provider's close itself has a bounded native shutdown phase (up to 300 ms).

## Public states and custody

| State | Meaning and allowed progress |
| --- | --- |
| OFF | Clean; no token or grant. A new explicit start is allowed. |
| STARTING | Copied finite descriptor awaiting acquire/open. Cancellation wipes it. |
| ACTIVE | One live token; poll/status and explicit comparison choice are allowed. |
| ENDING | Ordinary cleanup; only close/release phases progress this client. |
| CLEANUP_PENDING | Native release is unproven; retain token/module/dependencies and retry only the same close. |
| RETAINED | Terminal ownership/contract uncertainty; absolutely no further I/O or release. Retain the app invocation. |
| UNAVAILABLE | Clean unavailable/rejected attempt; user may explicitly retry with a new descriptor. |

`portable_file_setup_cleanup_only` is a pure predicate. Once a native call
returns CLEANUP_PENDING, it remains true through checked close and subsequent
grant release. The frontend must suppress every unrelated provider operation,
render call, and ordinary Runtime yield that polls providers during this phase.
Only the same checked close retry is appropriate. Provider0.1.1 requires the
validated scheduler-only platform clock suffix and cooperates once on every
refused native close, without invoking the ordinary diagnostic sleep path. Terminal retention is a separate state, checked before
any further app work; a false cleanup-only predicate does not override it.

The boolean owner predicate is the app's known ownership fence, not a new
native owner-loss ABI. Native FAULT/EXPIRED with a valid token leads to checked
close. It never means owner loss by itself. CONTEXT, explicit RETAINED, malformed grants/status,
unknown results, generation regressions, inconsistent comparison copies, a
changed connection generation, or a failed/ambiguous Runtime release retain
the invocation. A negative close other than the documented CLEANUP_PENDING is
terminal because native release has not been proved.

Canonical confirmation INVALID specifically means the displayed comparison
was rejected without changing the provider's pending comparison. The client
erases its old display, then refreshes through the next poll/status. A fresh
explicit user choice is necessary; it never silently retries acceptance.
Locally mismatched generation/number is rejected without any provider call.
Other unexpected INVALID responses to live poll/status remain terminal.

Canonical open INVALID with token zero is a known pre-claim rejection. Like a
tokenless BUSY/FAULT/EXPIRED, it releases the acquired grant and becomes clean
UNAVAILABLE. A failed open with a nonzero token still requires checked close.
An OK/PENDING open with token zero is ambiguous and terminal: there is no
safe close token, and the attempt is never repeated. Acquisition failure with
an empty output and a still-valid owner predicate is clean unavailable;
nonempty or malformed failed acquisition is retained.

No public status or diagnostic contains credentials. Pairing values are only
in the copied comparison fields and must never be logged. Cancellation,
terminal fencing and completed open explicitly wipe client-owned descriptor
bytes. Credential copies held by the HTTP session remain that owner's separate
responsibility and are erased when that session ends.

## Deterministic verification

Run:

```
bash scripts/test_portable_file_setup.sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash scripts/test_portable_file_setup.sh
```

The fake implements the actual canonical API table, with explicit token and
grant custody assertions. Cases cover copied input lifetime, pre-open duration
deduction, secret erasure, exact accept/reject matching, stale comparison
refresh, stale generations and connection identities, failed opens with and
without cleanup tokens, tokenless successful open fencing, native fault/expiry,
close refusal/retry, cleanup-only suppression, malformed results, owner loss
at every call boundary, grant release uncertainty, and explicit clean retry.
Tests bound external calls per public step and verify that terminal operations
perform no further I/O. Leak detection is disabled for ptrace-constrained test
hosts; this allocation-free client is still checked with ASan and UBSan.

These tests do not execute BLE/RF, exercise a device or OS numeric-comparison
dialog, prove native Wi-Fi/HCI coexistence, or qualify combined target memory
and performance. Product/build integration and publication are separate.
