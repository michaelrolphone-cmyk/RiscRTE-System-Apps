# Capability-derived Springboard redraw scheduling

This changes a scheduling ceiling, not measured rendering or physical display
FPS. `PortableFrameCadence.h` is a shared pure policy: non-retaining RGB565 uses
20ms when refresh is unspecified. A useful nominal refresh uses rounded-up
milliseconds, bounded to at least17ms and never slower than the caller's legacy
interval. Retaining/non-RGB565 paths retain the legacy interval. Resolution and
board identity do not select cadence. Existing presentation admission/fallback
remains unchanged; slow/retaining test panels can still use the plain view.

`SpringboardPresentation.h` adds an optional **private linked-app** callback,
not a Runtime/provider ABI. Old struct prefixes and null callback retain40ms.
A positive requested interval no greater than40ms is accepted;0/larger values
fall back. The selected helper requests17–40ms, default20ms. Other private
clients may request faster periods, including1ms;17ms is helper policy, not a
universal private ABI rule. Poll budget is min(requested period,20ms).

Animation/motion and launch transitions still use elapsed time; no per-frame
physics increment was substituted. Touch acquisition cadence, display ownership,
completed-frame hit mapping and Runtime authority are unchanged.

`python scripts/test_frame_cadence.py --output <directory>` exercises policy
bounds plus128 actual-controller normal/ASan/UBSan lanes, including an exact
allocated old prefix, null/invalid/1ms private suffix, nominal refresh, retaining
fallback, gestures, scheduler stalls and exact launch identity. It records source
input hashes and dirty status rather than claiming an uncommitted tree was clean.
Modeled logical present intervals demonstrate17/20/40ms requests; these are not
physical throughput. A17ms request may complete later under work/scheduler cost.

Existing Nova fixture rate bounds now derive from the admitted private cadence.
Historical40ms stays unchanged; short animation frame-count bounds scale by the
ratio while exact launch/no-launch and cleanup assertions remain in place.
