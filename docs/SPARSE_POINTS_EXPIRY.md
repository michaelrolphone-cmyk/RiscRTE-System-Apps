# Copied Points refresh on a sparse timer

The 408-byte retained record contains a bounded catalog projection. A timer
reuses it while its time window covers the current native UTC sample. At its
exclusive expiry, after a clock rewind, or when a prior copy is unavailable,
the selected Points desk face refreshes through its already admitted alarm
service grant. Other desk faces and valid copied windows take the existing
timer path without a projection or refresh call.

Refresh runs before drawing and before sleep holds. It does not promote the
Runtime provider cohort or upgrade the adapter to foreground. TIMER already
owns display and alarm service with their required dependency closures. No
Home frame, preference read, touch/navigation/battery acquisition, radio policy,
or wake-light restoration is introduced.

The service getter can return its older successful projection during loading.
The adapter therefore requests reconciliation and pumps at most 64 cooperative
phases. It requires a new READY snapshot and a matching copied projection,
checks custody after every call, and stops on alarm ownership or ordinary
service errors. Terminal status/output uncertainty seals the silent fence.
The controller resamples native time and rejects a replacement that does not
cover that sample. A replacement gets a full desk frame; it cannot seed the
prior physical image from different labels or countdowns.

An ordinary copy/reconciliation failure renders the existing unavailable
Points state and may be retried on a later timer. The existing alarm and sleep
preparation/refusal paths remain responsible for due work and refusal handling.
This change does not reset a live service's failed RTC anchor or change its
recovery policy. Record schema, byte count, retained renderer, orientation and
foreground policies are unchanged.

## Verification

`scripts/test_sparse_points_expiry.py` compiles the actual Clock, adapter and
product sleep hook against strict native/provider doubles. It covers expiry,
rewind both inside and outside the copied window, empty prior state, stale
successful getter data while loading, exhausted phase budget, due/RTC errors,
malformed/future/mismatched projections, terminal outcomes at each operation,
both landscape directions, synchronous/asynchronous display and Quick/radio/
wake-light configurations. It then reboots with the actual refreshed 408-byte
payload, verifies reuse on the next minute, and refreshes at its next expiry.
Normal and ASan/UBSan pixels must match. An optional pre-fix worktree reproduces
the expiry/rewind failures with the same fixture.

Existing sparse-adapter lifecycle/opt-out byte checks, retained render/codec
checks, real Runtime demand activation/retention suites, catalog-service tests
and the full resident-host Xtensa structural/import checks are separate
qualification. These are host/target checks; hardware remains untested.
