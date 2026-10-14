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

Product audit: delivered Waveshare .32 BLE Scanner and Points executables are
byte-identical to Watch .29 shared-text callers, but their manifests and boot
policy omit ui.text-input. Product composition must restore these declarations
without replacing their executables or unrelated grants. Current Lists,
Contexts, Timecard and RF callers already use the shared service; old remaining-
keyboards inventories are historical.

Checkpoint stage: final sanitizer, lifecycle, target ELF and full-cohort package
qualification are in progress. No hardware validation or flash performed.
