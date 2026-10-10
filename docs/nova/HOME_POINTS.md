# Native X4 Home Points

The supplied NOVA-7 E-Ink Home design reserves everything below the clock's
474-pixel divider for the next Point, its local time and countdown, elapsed
progress, and two subsequent events. The earlier Clock implementation replaced
that panel with a launcher icon and navigation instructions and never loaded
Points data. The tagged native sparse Clock now restores the panel and opens
`points_in_time.elf` when it is tapped. Swiping opens Springboard; the shared
adapter still owns the top pull-down, Home and crown actions.

The view uses the Watch's copied `nova_points_state` contract and the pinned
Utilities `PointsUtcSchedule.h` helpers. Its snapshot comes from the same checked
native sample and selected timezone as the clock above it. Countdown and progress
use elapsed UTC seconds. Local hours are presentation values only. Duration ends
are displayed as BACK, independent of end-notification settings; service warning
cues are never mistaken for schedule rows. Custom labels remain complete and fit
within the panel's columns.

On foreground redraw the Clock reads `points_utc_cfg` and `points_utc_meta`
through explicit KV instance 5 and closes that grant before rendering or polling.
Missing records use the shared virtual defaults, as in the current Points app
and Watch Home. A present empty catalog stays empty; invalid or unavailable
catalogs show POINTS UNAVAILABLE. Invalid custom metadata uses generic custom
labels. There are no writes, service occurrence reads, or new RTC recovery paths.
Unconfirmed grant acquisition, context loss or release fences the invocation.

The retained timer-only desk-clock path never calls the Home loader or model.
The deployment keeps 14 declared requirements; the existing storage requirement
permits a second explicit grant for KV5, giving 15 configured grants. The default
legacy Clock and Watch Springboard builds remain byte-for-byte unchanged.

`scripts/test_home_points.py` builds and exercises the actual Clock controller,
shared adapter and X4 sleep client against strict provider doubles, normally and
under ASan/UBSan, with and without Quick Actions. It checks Points taps, retries,
swipes, cancelled and initially-held contacts, minute updates, missing/invalid
records, custody failures and timer wakes with no Points/KV5 activity. Tests also
check projection behavior across the native epoch limit and DST gaps/folds,
reviewed pixel crops from the actual one-bit framebuffer, and the selected
Xtensa ELF's imports, exports, structure and corrupt-file rejection.

This is host and target-artifact evidence. Physical device behavior has not been
verified by these tests. Source/design provenance is in home-points-sources.json.

## Interactive reference correction

The original restoration used enlarged generic 28/20-pixel glyphs. That did not
match the supplied interactive design's type weights, baselines or hierarchy.
The native Home now has its own raster assets, generated at the actual physical
sizes from the official Google Fonts files: Orbitron 900 at 52/100 pixels;
Orbitron 700 at 20/24/28; Rajdhani 700 at 22/26/28/30; Rajdhani 600 at 22; and
Orbitron 600 at 18. Baselines, tracking, complete-label fitting and kerning are
explicit. The SVG's butt-ended dial marks, elapsed-minute weights, divider,
rounded progress track, segmented real battery data and upcoming-row columns
are preserved in one-bit output. Font licenses, source hashes and repeatable
generators are included.

The interactive targets now work against the real application/controller:

- Dial: opens Springboard.
- NEXT panel: opens Points in Time using the existing live catalog route.
- Entire top strip, including the upper-left corner: requests the existing
  Quick Controls owner on the next poll.

Home recognizes the adapter's combined began/released replay for a stationary
top tap only after a neutral sample. Initially held contacts and cancelled taps
do not activate the strip. The source-defined target rectangle inverts while
pressed. Launch waits for that feedback frame to finish, using the shared
readiness contract; input continues to be polled. Feedback redraws use the
copied Points snapshot and introduce no additional KV reads. Real error notices
remain visible. Prototype-only radio/quiet status icons are omitted because
Home does not have verified telemetry for them, and the instructional footer
stays removed as requested.

`test_home_points.py` covers normal and ASan/UBSan execution, synchronous and
asynchronous frames, exact pixel inversion of each pressed target, top-tap
entry/dismissal, cancelled and initially held top contacts, launch retry, timer
custody and UTC projection. `compare_home_reference.py` independently compares
six native type/layout regions with the actual supplied SVG rendered using the
correct fonts and the same fixture schedule. The native black-pixel overlap is
84–97%, compared with 14–41% before; one-bit rasterization accounts for residual
pixel differences. This quantifies typography/layout agreement, not physical
panel qualification or invented status parity.

Actual before/after and pressed-state images are under
`docs/evidence/home-parity/`. Legacy default Clock and Watch Springboard ELF bytes
match the shared readiness baseline `4b72a54`; Home changes are opt-in only.
The delivered 0.1.9 image was not modified or published again.
