# T046 shared keyboard checkpoint

Base: System 2d8200c3ddab95bfa5516790a09f70401ff36622 (X4 .115 scene).

The existing ordered touch reducer is unchanged. A fresh 48-case actual text-owner
and scene matrix passes 200 characters each at 200/133 ms cadence, repeated Q
and alternating Q/W, on 240x240, 410x502 and rotated 480x800 logical displays,
with 0/16/120/2300 ms presentation completion. Events arrive on an independent
modeled clock; this is not physical touch-controller or hardware latency proof.

A new negative-control HID fixture reproduces lost input in the baseline: 16
same-key DOWN/UP pairs exactly fill the valid 32-event queue. The old owner
mistook a full read budget for overflow. Actual provider overflow has an explicit
next() == -1 signal. The fix accepts valid batches and separately tracks pending
input for checkpoint arbitration. Explicit gap, detach, focus, neutral, cancel
and retention fences remain. Full-batch passes with the fix and the existing
text-owner/client suite passes (use --decoupled-attention).

Compact displays now adapt the supplied NOVA-7 style retained in the portrait
renderer: licensed Orbitron/Rajdhani fonts, outlined rounded keys, circular
header controls and inverse Done. Key topology/coordinates remain unchanged;
compact Clear is restored as an explicit control. Portrait raster is unchanged.

Product audit: Waveshare .32 BLE Scanner already contains the shared caller but
omits its manifest dependency and boot grant. Points in Time in both color
products is an older executable despite Watch's shared-input declaration.
The product composition restores the qualified Points shared caller and modal
policy, preserving its scheduling and explicit Save semantics; the Wave build
uses the existing native-height adapter and matching contact transform.
Other current Lists, Contexts, Timecard and RF callers already use the service.

Final source qualification: 197 presenter/profile cases, 48 timed typing cases
normal and Linux ASan/UBSan, 104 normal/sanitized handoff cases, 75 unchanged
portrait cadence/raster comparisons, and the complete text/Runtime suites pass.
See evidence/summary.json for exact counts and source-bound build records.
All three complete product stores pass Runtime admission. Native firmware and Home are preserved. Points in Time is the only
application executable replaced, on Watch and Wave only. Physical hardware validation remains
pending; no flash performed. These product cohorts have no USB keyboard provider
and select the on-screen fallback. The same text host accepts an attached HID
keyboard when composed with its existing optional USB dependency manifest.
