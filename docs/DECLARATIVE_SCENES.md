# Optional semantic scene presenter 0.1.1

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

The 128 positive test executions (each repeated with ASan/UBSan) cover three profiles,
pending transfers, stale-frame input, event gaps, held touches, transient polls,
restoration, the shared keyboard, and terminal cleanup/native-retention faults.
A negative witness also rejects the old portrait 90 policy using independent
installed-panel raster coordinates. Full application/Runtime lifecycle and
alarm-transaction tests live in RiscRTE-Utilities.

Existing package/driver UI tests also retain their PortableApps include path;
this fixes a pre-existing missing-header build without changing their behavior.
