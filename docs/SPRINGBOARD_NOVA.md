# Shared compact Springboard (1.3.4)

This is a presentation of the existing `Apps/springboard.c`, not a new app or
firmware UI. `Apps/SpringboardPresentation.h` is a private, app-local drawing,
contact and clock contract. The ordinary app supplies a null weak implementation;
the linked portable client supplies the compact implementation. No pinned SDK,
runtime symbol, capability ABI, hardware register or pin changes.

## Reference and real inventory

The authoritative supplied reference is **NOVA App Picker — 240×240.html**,
8,516 bytes. The earlier 7,225-byte radial/54-corner mockup was superseded.
The app follows the new 22 px corner treatment, intersected horizontal/vertical
edge masks (0 → 40% at 14 px → opaque at 46 px), 54 px honeycomb spacing,
44 px circular tiles, 24 px glyphs, up to 1.12 center scale, max-axis/130 edge
shrink, nearest label and top clock. Text uses actual Orbitron 500 / Rajdhani 600
raster subsets. Application identities always come from `installed_apps_get`;
the prototype's 24 Phone/Mail/etc entries are never admitted or implemented.
The portable deployment catalog remains bounded to 17 real entries. One and two
apps work naturally; zero apps has no motion or launch. The shared presentation
also pages larger host inventories in groups of 19 using the bottom page counter
or ordinary directional navigation.

`lib/PortableApps/fonts/icons.inc` contains genuine glyph rasters extracted from
Reader's Font Awesome 7 Free Classic SD font assets. `fonts/SOURCES.json` records
the official Font Awesome commit, Reader commit, source hashes and generated
hashes. The renamed **RiscPortableIcons** and **RiscPortableText** assets carry
their SIL OFL licenses. Unknown icon IDs return unavailable; no handcrafted
symbol is represented as a Font Awesome glyph. `scripts/generate_portable_fonts.py`
regenerates the subsets from the audited inputs. Portable development outputs
include the copyright/license notices and provenance under `licenses/portable-apps`;
deployment packagers must retain them.

## Presentation gating and fallback

The portable client enables NOVA only for compact dimensions (one dimension
below 400), advertised refresh of at least 20 Hz, non-retaining RGB565 output,
and no known typical presentation latency above 50 ms (zero is unknown and
allowed). Unknown/slow/retaining displays
use the existing static layout. The established paper renderer and its existing
slow-display/video selection are retained. Rendering is bounded to 25 presents
per second and stops when settled; input/clock polling continues. Presentation
failure exits safely and cannot queue a launch. Owned surfaces, subscriptions
and capability grants are released on exit and partial initialization failure.

## Motion and interaction

`springboard_motion.h` uses elapsed milliseconds, not frame counts. The allowed
pan interval on each axis is derived from the outermost actual icon centers.
Dragging beyond it follows a continuous, analytically integrated rubber band,
with at most 40 px overscroll per axis. Regrabbing a moving overscrolled surface
does not recompress/jump the displayed position; inward drag follows the finger.

Interior friction is 2.4/s, increasing toward 12/s within 50 px of an edge.
Outside the boundary the spring constant is 180/s² with damping 25/s. Once
slow enough, the nearest-icon spring uses 110/s² and damping 21/s, preserving
position and velocity when capture begins. Physics takes steps of at most 4 ms,
with at most 25 steps per call; a gap over 250 ms cancels momentum and tap state.
A subpixel tolerance shared by edge detection and acceleration prevents an
indefinite floating-point edge stall. These parameters are explicit constants
for tuning, not measured physical-panel timing claims.

A fresh tap selects, pulses and queues exactly the admitted app, then returns
through the normal loader lifecycle. A new touch interrupts a pending launch.
Back returns from the app; the runtime's configured default controls what
reappears. Launch/incompatibility errors remain visible and do not loop requests.

The first already-held single contact may be adopted **only for dragging** so a
clock-to-launcher transition can preserve the finger. Its release cannot launch.
A fresh neutral interval and down are required for tapping. Gaps, faults,
replacement (including same-ID UP/DOWN), transient multi-touch, queued movement
away/back, invalid final UP coordinates, exhausted event budgets and long loop
gaps cancel or suppress tap eligibility. Shared Settings/Battery keep the normal
neutral guard; they never adopt held input as an action. `PORTABLE_TOUCH_ROTATION`
is an explicit deployment choice of 0 (default) or 180 degrees, applied to copied
coordinates after raw stream/snapshot validation. There is no injected touch,
new provider ABI, retained pointer or invented crown/rotary input.

## Clock policy

The compact toolbar always formats valid time as 12-hour AM/PM. Without an
explicit deployment time policy it shows `--:--`, not raw RTC time presented as
local time. `PORTABLE_RTC_UTC8_DENVER` selects the reviewed shared
`PortableTime.h` forward converter and read-only `rtc.clock@2` grant. This is the
same temporary fixed-UTC+08 storage → America/Denver display policy as the Clock
build, with exact source/provenance retained under `lib/PortableApps/time`.
The toolbar never writes RTC state. Settings owns its explicit inverse/gap/fold
workflow separately.

## Evidence and limits

`python scripts/test_springboard_nova.py` compiles the production shared app and
adapter under UBSan: 65 lifecycle/input/catalog/rotation/time-policy scenarios,
plus deterministic spring tests, 19,000 randomized actual-layout releases and
300 count/path/rate combinations at 30/60/120 Hz. The latter asserts no more than
2 px matching-time drift; observed maximum in this revision is below 0.84 px.
Randomized releases settle within 2.3 s and remain within 40 px per-axis bounds.
Independent ASan/UBSan review also exercised the actual app and adapter.

`python scripts/build_portable_springboard.py` builds a zero-catalog development
ELF and validates Xtensa structure/imports/exports; `--denver --rotation 180`
exercises the explicit deployment profile. Deployment must link its own admitted
catalog. The local available compiler is GCC 14.2; the repository CI still owns
pinned GCC 8.4 verification and unchanged released-app byte parity.

Visual evidence is produced from actual C RGB565 frames, including an animated
spring sequence. The uploaded HTML's reference panels are offline SVG
reconstructions from its original JavaScript equations and original SVG paths
with the exact provided fonts, not browser screenshots. CSS font line-box
placement is approximated in those reconstruction panels. Prototype glyphs and
24-entry inventory differ deliberately from the real FA/catalog implementation.
No physical Watch frame rate, touch alignment or installation is claimed by
host/ELF evidence. Clock fade/handoff and bundle wiring are deployment work.

## Focused icon, development 1.5.0

The nearest/labelled icon now receives a modest focused-only size increase.
See [production before/after pixels and regression coverage](SPRINGBOARD_FOCUS.md).
Touch geometry, other icons and the existing launch pulse are unchanged.
