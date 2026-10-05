# Portable Springboard client

Springboard remains `Apps/springboard.c`; there is no Watch fork. Version 1.3.3
includes the Reader ec0c09991f8f7babc69b3da0681b0b3f1e99c3ab slow-display
selection fix (1.3.2) and adds a compact static grid for smaller displays. The
existing large-screen renderer and animation remain available on their prior
interfaces. Home editing is offered only with actual storage support.

`lib/PortableApps` is a client library linked into existing application ELFs.
It implements the subset of existing T5 drawing, input, UI and battery getters
used by Springboard and the shared Utilities Battery app through canonical
RiscRTE runtime grants: display.output@1, input.touch.raw@1 and board.battery@1.
These T5 getters are hidden local symbols, not additional runtime exports or a
competing SDK. Original ABI headers are pinned byte-for-byte in SOURCES.json;
the new Portable headers are private library interfaces. Upstream licenses are
retained under the repository license. No hardware register, bus or pin appears
in the adapter.

The deployment links a bounded catalog of at most 17 explicit apps. It does not
pretend to discover installed files. Runtime boot policy must grant each app's
own required capabilities. A successful launch request returns through the
existing app lifecycle; there is no recursive loader or retained app state.
Unavailable storage/video returns no interface, and editing is disabled. Drawing
uses bounded RGB565 surfaces (200..1024 pixels per dimension), clipped primitives,
original compact glyphs and two simple symbols. Presentation yields and has both
an elapsed-time and iteration bound. Unsupported display formats fail closed.

Touch uses the canonical copied-event/snapshot interface. Each invocation needs
neutral input before arming; event gaps, failed polls, multiple contacts and
contact replacement discard gesture state. A completed stationary tap activates
an item. Upper-left is Back; Battery's upper-right is Update. Battery rows scroll
using the bottom Previous/Next controls. Subscriptions, held frames and grants are released on return and on
partial initialization failure. No driver/task pointer survives unload.

`python scripts/test_portable_apps.py --utilities /path/to/RiscRTE-Utilities`
executes the production apps and adapter under UBSan. Existing app fixtures and
target builders remain intact. Watch deployment builds link this library against
the real shared sources and separately validate target imports/exports. Runtime
and physical peripheral integration are distinct evidence, not implied by these
service-model tests.

Source audit records remain historical. Springboard is now an intentional
external development delta; do not overwrite it when synchronizing Reader.
Published-byte checks still require identical bytes for unchanged published
versions; strictly newer versions are reported as development builds, never
reported byte-identical to an older publication. Springboard: 1.3.1 -> 1.3.3.

The adapter preserves successful battery reads when SOC is missing or unprofiled: voltage and charging status remain valid, while `soc_percent` is UINT16_MAX. Deploy with Battery >=1.0.4, which renders this out-of-range value as Unknown; do not pair with earlier Battery builds. This is a paired client/app interpretation, not a change to the pinned SDK layout.

## Shared Settings: compact view and RTC editing (1.0.2)

`Apps/settings.c` remains the original shared app. `PORTABLE_SETTINGS_APP`
selects its portable callbacks, compact model and view; no Watch Settings fork,
firmware UI or new runtime ABI is introduced. Settings 1.0.1 -> 1.0.2 is recorded
in the current manifest/inventory; historical source/release snapshots stay
unchanged. Later shared Springboard rendering/input changes have their own
versioned provenance and must be included in the final paired build evidence.

The supplied 240x240 reference supplies the black/cyan grouping, forty-pixel
rows, vertical edge fade, Orbitron/Rajdhani typography, numeric +/-/segment
subpages and visible Back controls. Embedded raster subsets and both OFL
licenses are in `settings_fonts`; normal builds do not fetch or rasterize fonts.
The Settings viewport is centered on displays at least240x240.

Only Set Time and About are active rows. Time Zone, Time Format and RTC Basis
show the actual selected build policy as read-only information. Format is
12-hour AM/PM. About reports this built Settings version and actual display
geometry. No prototype84%,OS7.2.1,RV32IM,serial,radio,sound,brightness or fake
preference controls are shown. The existing Battery app remains separate.

### Navigation and editing

- Root Back is visible at x82..157,y192..217; it exits Settings. A completed
  rightward swipe of at least36px with horizontal dominance does the same.
- Vertical drags scroll the root or six-field editor. Direction locks after
  movement; a moved/canceled gesture cannot activate a row or Save.
- Set Time initially occupies x20..219,y124..163. The editor lists year, month,
  day, hour, minute and second. Tap a field for a numeric subpage; +/- and the
  segmented slider change the draft only. Its Back button returns to fields.
- The editor has separate CANCEL (x24..109,y200..225) and SAVE
  (x130..215,y200..225) controls. Cancel writes nothing. A failed/invalid RTC
  read is visibly unavailable; the editable2000-01-01 midnight placeholder is
  never silently written and never comes from host/compiler time.
- Year is bounded2000..2099. Month/year changes clamp an impossible day. Save
  computes weekday. Write/verification failures remain visible; no automatic
  retry or rollback is claimed because a failed transaction may partly apply.
- Generic `input.navigation@1` is optional through `PORTABLE_INPUT_NAVIGATION`.
  It requires its own explicit manifest/grant, full-table validation and reset.
  Neutral input is required at app/page boundaries. Up/Down select rows; in a
  numeric subpage they adjust the value; Confirm opens/returns/saves the
  selected control. Editor navigation includes explicit Save and Cancel.
  No physical rotary encoder, crown channel or pin is inferred.

The view consumes the same `portable_touch_read` sampler as Springboard. It
requires neutral before accepting a new contact, including on page changes;
held-entry, replacement, event gaps, faults and moved-away/back contacts cannot
become taps. `PORTABLE_TOUCH_ROTATION=180` is an explicit deployment transform,
not a persisted Settings rotation preference.

### Shared time policy, forward and inverse

The default shared build remains raw RTC wall time. Only the explicit
`PORTABLE_RTC_UTC8_DENVER` define selects fixed UTC+08 RTC storage and
America/Denver display. The exact verified Clock forward converter and calendar
header are retained in `time/denver`, with immutable source hashes and IANA2026e
provenance in `time/SOURCES.json`. `PortableTime.h` selects that converter or
identity behavior. It does not mutate global TZ state, synchronize the RTC,
create persistence, or assume all shared app deployments are Watches.

The selected Settings editor shows Denver local civil time. Save constructs
both possible raw storage candidates (local+14h and local+15h), then round-trips
each through the SAME forward converter. Zero matches rejects a spring gap or
out-of-RTC-range value without a write. One match is unique. Two matches require
an explicit SAVE FIRST MDT / SAVE SECOND MST choice; Back cancels that choice.
The2000-2006 and2007+ US rule periods are both supported. Storage dates outside
2000-2099 are rejected even if the requested local year appears in range.

After the explicit Save/choice, exactly one raw write is followed by at most
four readbacks within250ms. Comparison is in raw RTC coordinates and allows a
single legitimate ticking-second/date boundary. The existing `rtc.clock@2`
layout is copied verbatim in `PortableRtcClock.h`, with original types, source
provenance and32-bit target layout assertions. No alternative RTC SDK exists.

### Build and launcher integration

```
python scripts/build_portable_settings.py
python scripts/build_portable_settings.py --denver --touch-rotation 180
# Add --navigation only when a real provider is selected and granted.
```
 `--output-dir` can
isolate raw/profile artifacts. The builder emits settings.elf, settings.json,
settings-build-record.json and required font/source notices. It validates target
layout, imports/exports and the real structural ELF validator. This is a
development artifact, not a deployed or published application catalog.

The launcher must package the shared ELF and its manifest, add Settings to the
bounded catalog using solid:f013, and grant display.output@1,input.touch.raw@1,
rtc.clock@2 to the exact selected provider instances. Optional navigation adds
input.navigation@1. The Watch profile must pass --denver and the matching touch
transform and use the same forward converter for Clock/Springboard/Settings.
Keep the corrected NOVA clock as default; verify actual runtime launch, Save,
return and fresh Clock readback. Do not deploy the earlier raw-only checkpoint
under the Denver display policy. No pointer/frame/subscription/grant survives
app teardown.

Host tests cover the production app/view, read-only rows, Save/Cancel, all valid
calendar dates, unset/error reads, refused/short grants, ticking rollover,
write/readback failures, gestures, held navigation, slider editing, inverse
DST gaps/folds and range errors. Both raw and selected-policy hourly round trips
cover the supported century (876600 raw inputs; the first15 raw hours have no
supported Denver date). Host/target checks do not qualify physical hardware.

Persistent timezone/format/rotation controls remain blocked on an actual shared
preferences backend. Bootfs stays bootstrap-only; package files are not used as
mutable preferences. Existing time-zone app/catalog code remains the reuse path
for that later scope, including its separately identified action-edge fixes.

## 1.3.4 compact presentation

The compact static grid above remains the fallback. Fast compact displays now
use the shared [NOVA presentation](SPRINGBOARD_NOVA.md), with actual licensed
Font Awesome glyphs, elapsed-time spring/friction motion, cancellation-safe
copied touch input and optional explicit clock policy. The original two
handmade portable symbols have been replaced by the licensed raster subset.
The private presentation contract is linked into the app, not a runtime ABI.
