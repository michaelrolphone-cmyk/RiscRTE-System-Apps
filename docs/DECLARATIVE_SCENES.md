# Optional semantic scene presenter 0.1.2

`Services/scene_host` is an ordinary, independently loadable `ui.scene@1`
provider. `Services/scene_profile` supplies independently identified compact-color
and portrait-monochrome presentation-policy providers. None is linked into the
Alarms application; headless deployments omit all of them.

The presenter owns pagination, word wrapping, focus, buttons, touch hit maps,
Back/Home controls, input mapping and rasterization. The Alarms declaration has
one logical time-editing scene. It becomes multiple physical pages at 240x240 and
one form at 480x800 without an application branch. Profile identifiers are in
product deployment configuration, not in the application contract.

## Lifetime and asynchronous display

Documents, navigation and events are bounded copied values. A session uses a
nonzero generation and increasing document revision. A tap is accepted only
against the revision/route/epoch that was visibly presented. Input is still
polled while a display transfer is active. Dirty updates coalesce; the display
provider owns queueing, waveform and physical-transfer policy.

Successful submit consumes the mutable surface lease. The presenter keeps only
the presentation token until completion or supersession; it must not release the
consumed surface. Close returns `AGAIN` while draining. A failed status, failed input
teardown or uncertain ownership returns `RETAINED`; ordinary service work stops
and the application retains its invocation instead of falsely unloading code.
The provider's quiescence check must also succeed before its own ELF is removed.

The logical route/focus snapshot is independent of these physical resources.
An unloaded controller can reconstruct a new presentation session from it.
Focus restoration recomputes the right page for the selected profile.

## Optional lifecycle suffix

`RiscSceneLifecycleV1.h` adds a size-gated `configure(context, session, flags)`
callback after the unchanged `risc_scene_api_v1` prefix. Consumers use
`risc_scene_lifecycle_get_v1` to reject absent, short, wrong-version or null-callback
extensions without accessing suffix storage. `RiscSceneV1.h` and
`RiscSceneStateV1.h` remain byte-for-byte unchanged. Legacy consumers never receive
the new flags or controls event; the feature is session-local and resets on close.

A successful `configure(..., 0)` enables lifecycle tracking alone. Snapshot adds
`RISC_SCENE_ACTIVITY` (4) for observed touch/navigation activity and consumes that
latch only after returning a successful snapshot. Reconfiguration preserves an
existing activity latch. `RISC_SCENE_INPUT_BUSY` (8) is a non-consuming level for
held contacts/buttons, queued or unacknowledged events, unsynchronized input and
a consumed suspend/controls boundary. Unknown configure bits are rejected.
Configure consumes inherited raw input and resets navigation; a subsequent bounded
input drain establishes synchronized state. No callback after native revocation
is followed by further provider I/O.

`RISC_SCENE_FEATURE_SHARED_CONTROLS` (1) opts into a deliberate top pull, which
emits `RISC_SCENE_CONTROLS_EVENT` (4) with zero node/action/value. Revision and
sequence follow the base event contract. The presenter only arbitrates input;
it never displays Quick Controls and does not know resident policy or products.
Thresholds reproduce the current resident adapter's presentation semantics:
start within the top 72/800 of logical height, move at least 24/800 downward,
stay within 1/12 of logical width horizontally and move down more than twice the
horizontal displacement. These are presenter implementation details, never app
configuration or persistence fields.

Recognition consumes the triggering gesture and suppresses inherited input until
an explicit input-context boundary or close. Gap/truncation cancellation drops an
undelivered request and rearms safely; no invisible request can leave input stuck.
The gesture is suppressed on keyboard scenes. Pending, superseded or stale frames
cannot turn old gestures into controls requests; opted-in sessions drain input
before observing a pending frame's completion. Existing keyboard custody and
portrait270/touch0 mapping remain unchanged.

## Current implementation envelope

One foreground client; fixed-size 8-route/24-node documents; ASCII prototype
glyphs; text, time, integer, boolean, action and link components. Pixel output
supports RGB565 and packed monochrome/gray surfaces. Portrait rotation and raw
input rotation are independent profile properties. No NOVA visual-parity claim,
Unicode font service, virtualized unbounded collection, remote presenter or
custom game/canvas API is made by this increment.

The compact profile uses RGB565; portrait-monochrome uses the existing 800x480
panel surface rotated to a 480x800 logical viewport. The grayscale profile is an
additional host test, not a separately shipped product selection.

## Build and test

With a Runtime checkout containing the optional scene SDK:

```
python scripts/test_scene_host.py --runtime ../Runtime
python scripts/test_scene_host.py --runtime ../Runtime --sanitize
python scripts/build_scene_services.py --runtime ../Runtime --output build/scene-packages
```

The target builder emits independent manifests and receipts. It checks exact
exports/imports and runs the existing strict ELF validator on all three target
ELFs. It does not link blanket libgcc helpers or relax the loader's validator.
A source-owned 32-bit bounded input calculation avoids unsupported helper ELF
constructs and unnecessary 64-bit division in the touch path.

The 161 positive test executions (each repeated with ASan/UBSan) cover three profiles,
pending transfers, stale-frame input, event gaps, held touches, transient polls,
restoration, the shared keyboard, optional lifecycle discovery and flags, shared-controls
gesture arbitration, and terminal cleanup/native-retention faults.
A negative witness also rejects the old portrait 90 policy using independent
installed-panel raster coordinates. Full application/Runtime lifecycle and
alarm-transaction tests live in RiscRTE-Utilities.

Existing package/driver UI tests also retain their PortableApps include path;
this fixes a pre-existing missing-header build without changing their behavior.
