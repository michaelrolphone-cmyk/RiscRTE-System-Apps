# Portable raster snapshot experiment (not selected by product builds)

This branch measures and prototypes the remaining CPU-raster scheduling gap.
The production checkpoint remains `739e5055272ad7e589ec4b0c82224210005fc431`.
No product build enables `PORTABLE_RASTER_SNAPSHOT`. It is not an adoption or
complete universal-decoupling claim.

## Reproduced delay

The actual adapter and provider-shaped input queue run a dense immediate-mode
frame. Fixture clock injection charges 0, 1 or 2 ms per 512 primitive pixel
visits; it does not modify production clock logic. Input edges arrive at 20/25
ms independently of the display. At 1/2 ms cost, touch is captured at 20 ms but
logical touch and navigation dispatch wait until 471/941 ms for RGB565 and
3021/6041 ms for MONO1. Zero-cost controls dispatch at 21 ms. Physical host CPU
measurements are recorded separately and are not target-device measurements.

## Prototype

The owner task copies a display command snapshot during the existing draw API
calls, then returns immediately to its normal app/controller loop. Polling
replays bounded row bands without calling app controllers from inside raster
work. A frame snapshot owns copied text, stable compiled glyph identity,
command order, clips and orientation. A sealed snapshot replays into app-owned pixels. The provider lease is acquired
only for the final byte copy and submission; submitted pixels retain their
original immutable-custody rules. Explicit frame drain remains a synchronous boundary.

The first experiment covers the portable core drawing API, paper text/circles,
and the Springboard presentation operations. The fixture mutates a borrowed
text buffer immediately after its draw call. Complete output buffers are
compared byte-for-byte against the original renderer. Allocation failure at a
command boundary materializes the complete recorded prefix synchronously,
then executes the current and later commands in the original immediate path.
Nothing is silently dropped; this compatibility fallback may delay dispatch.

## Adoption gates still open

- Complete Nova, Settings, Wi-Fi and Quick Actions primitive/path coverage,
  including direct internal framebuffer reads and writes. These must be
  materialization barriers or fully owned snapshots, never stale raw pointers.
- Real app scenes, all fonts/icons, all supported rotations, dynamic clipping,
  text truncation/fit semantics, operation return values and repeated clear.
- Modal alarm/Quick Actions, retained owner handoff, sleep overlay final settle,
  frame copies/history seed, transition timeline, custody and fault behavior.
- Background state callbacks while a resumable software raster holds a lease;
  existing `surface.frame` exclusions need a safe immutable-snapshot distinction.
- Memory peak and allocation/capacity failure behavior, plus bounded real CPU
  time for command generation and each replay slice.
- Target performance and power: row slicing adds scheduling overhead. Both
  model latency and complete-frame time must be measured, not just touch
  capture. The first zero-cost prototype was too slow for the existing rapid-
  input fixture; larger work-only caps retained the 2 ms elapsed-time ceiling
  and restored latest-frame convergence without relaxing input assertions.

No new public API replaces an existing one. Existing nullable external frame
copy hooks remain unchanged. Emulator/direct-display paths are outside this
private adapter renderer and keep their established cadence and ownership.

## Intermediate modal checkpoint

The recorded primitive set now includes Nova, Settings, legacy Wi-Fi and paper
Quick controls. Full graphics byte comparisons pass; 24 alarm and 36 applicable
low-battery fixture cases pass, as does the repaired paper updater matrix.
Review reproduced and repaired no-op settled-state loss, failed legacy fini
cleanup, Quick interrupted-background replacement, clean-submit metadata, and
sticky readiness before direct resident/desk frame access. Direct Watch Quick
rendering uses an explicit materialization barrier.

This intermediate checkpoint still holds a provider lease across replay bands.
The original Quick brightness/storage assertions reject that when an action
arrives during partial replay. This is an open adoption blocker, not a reason
to weaken those assertions. The next isolated experiment moves replay pixels
to app-owned memory and acquires the provider only for final copy/submission.

## Offscreen replay checkpoint

Replay bands now expose app-owned memory with frame identifier zero; no provider
lease crosses back into application or service work. A native terminal callback
keeps that memory pinned and does not restore stale surface state. The actual
Quick/Alarm fixture retains all its original no-frame assertions. The adapter
text-host handoff drains a sealed snapshot and discards an unsubmitted recording
before transferring display/input ownership; host painting cannot resume the
old app raster. Resumption starts a fresh app frame.

Normal and ASan/UBSan tests cover no-op settle, failed legacy cleanup, immediate
clear during partial replay, sticky readiness, Watch direct Quick overlay,
interrupted Quick restoration, storage actions during partial replay, actual
Quick open/close, node/allocation/acquire failures, offscreen OOM recovery,
clipped-begin byte equality, native terminal custody, and pending/unsubmitted
text-scene handoffs. A clipped initial clear intentionally retains the original
immediate path because it must preserve unknown provider pixels outside the
clip. All later clipped commands remain recorded normally.

Selected dimensions require a 115,200-byte Watch RGB565 replay buffer and a
48,000-byte X4 MONO1 buffer. The measured dense graphics command set adds 4,000
and 4,200 host bytes respectively; the Settings scene adds 1,800 bytes. The
Xtensa compiler confirms commands are 184 bytes without scrolling and 200 bytes
with scrolling. These are incremental app allocations, not total firmware heap
or physical-device free-memory measurements. A stress case exceeding 4,096
commands peaks at 819,200 host bytes, materializes all commands synchronously,
and remains byte-identical to immediate rendering. It does not silently drop
features, but its low-memory/capacity fallback does not guarantee low latency.

For normal replay, input at 20 ms reaches the model at 23 ms under injected
1/2 ms per 512 pixel visits. Exact selected-dimension buffers match the original
renderer. Input arriving at the final copy reveals a remaining boundary: an
injected copy cost of 1 ms per 4,096 bytes delays dispatch by 28 ms on Watch and
11 ms on X4. Actual host copy measurements are separately reported and are not
device timing. The final copy remains synchronous because provider ownership
must not cross into arbitrary service operations. Target-device copy timing,
heap availability, and alternative provider-supported transfer custody remain
adoption gates.

Four selected X4 target configurations (Home, Springboard, Settings and Files)
compile with the prototype enabled and pass existing ELF/import/relocation
checks. This does not select the prototype for any product or change the fast
panel default. Remaining direct Watch Quick rendering and explicit lifecycle
scenes use compatibility barriers, so universal completion is not claimed.
