# Refined foreground Home

The foreground Home uses the supplied 480×800 reference: Orbitron time, next
Point card and countdown, two following Points, and Files, Points, Contexts and
Settings dock buttons. The copied catalog still contains all four projected
events. Labels retain all 31 printable bytes through measured wrapping. Saved
12/24-hour preference applies to every time, with unpadded hours.

`paper_home_reference.inc` is foreground-only. The retained landscape Points
renderer and its snapshot, orientation and sleep paths remain separate. The
existing release/neutral/movement gates also apply to dock taps; confirmed taps
invert only their target, settle the frame, then use the existing resident host
launch path. Quick Actions remains owned by that host.

Fonts are the exact licensed Orbitron and Rajdhani inputs from the reference.
Dock SVG paths and the independent full-screen reference are checked in, with
hashes in `home_fonts/REFERENCE.json`. Regenerate native one-bit masks with
`scripts/generate_home_reference_assets.py` (Pillow and Inkscape).

Run `scripts/test_home_reference.py` with the selected utilities, Runtime SDK,
driver SDK and product sleep source. It exercises the actual Clock/adapter
controller with synchronous/asynchronous presentation, dock taps, swipes,
cancellation and held contacts, normally and with ASan/UBSan. It also compares
native renders with the supplied reference and checks full labels, hour format,
press inversion and target paths. `test_home_points.py` retains projection and
legacy native Home coverage; `test_home_catalog_render.py` covers the retained
landscape raster and codec.

The status-icon row has no fabricated active radio/vibration indicators. Those
require an admitted cached status interface; unknown states stay blank. Hardware
qualification is separate from these host-render and target-ELF checks.
