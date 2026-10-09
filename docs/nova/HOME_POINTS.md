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
