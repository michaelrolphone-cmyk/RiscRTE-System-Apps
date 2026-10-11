# Copied NOVA reading presentation (scene-host 0.3.5)

`RiscSceneReadingV1.h` appends a size/tag-checked optional suffix after the
existing page-refresh/checkpoint interfaces. Their binary prefixes, geometry,
revision, pause/resume and clean-refresh behavior are unchanged.

Clients request `reading_geometry`, render document content into the returned
MONO1 dimensions (one means white), and submit it with a copied presentation
record. The provider owns NOVA typography, icon masks, chrome, screen-space
geometry, touch arbitration, display acquisition and refresh policy. No panel
identity, GPIO, waveform or driver-selection knowledge belongs in clients.

At 480x800 the page bitmap is 480x680, inset 48px from the top. Edge taps use
30/40/30 percent zones. Center toggles controls through an intent. Chrome is
an overlay, not a new pagination viewport. The ten scrub segments emit their
midpoints in 0..1000. A vertical gesture in scrolling mode emits one bounded
pixel delta on release, encoded as `value - 1024`. It does not start an idle
animation or a periodic refresh timer.

A null bitmap plus zero length updates only presentation metadata while
retaining the copied document pixels. This permits control/bookmark changes
without reopening the book or rasterizing fonts. Initial publication requires
pixels. Invalid flags, missing actions, stale revisions, malformed strings and
wrong pixel lengths are rejected. The provider freezes the model and pixels
for in-flight raster work. Inversion invalidates with a clean refresh.

`present_preview` atomically copies a components document and one bounded font
sample. Only its designated TEXT_PREVIEW node consumes the pixels; all other
layout and controls remain declarative. It cannot alias the caller's buffer.

Shared components added: progress card, cover/metadata row, paired statistics,
dismissible row and text preview. ROOT_HEADER hides only the visual back
button; the physical back action remains available. Existing component kinds
and symbols retain their numeric values. DOCUMENT opts into compact document
progress and reference rounded-square steppers without altering other apps.

Reader glyphs come from Font Awesome Free by Fonticons, Inc., CC BY 4.0, as
supplied in the owner's Reader HTML. Reproduction is in
`scripts/reader_reference_icons.py`. Existing Orbitron/Rajdhani sources and
licenses remain unchanged.

Validation: `test_scene_components.py --runtime <RiscRTE>` includes real
provider tests for edge/center input, all ten scrub targets, overlay actions,
scroll release, immutable bitmap copies, preview validation, inversion and
idle pause/resume. It also runs the legacy page and checkpoint tests.
Host execution and structural ELF validation do not qualify physical hardware.

## Reading links (scene 0.3.6)

`RiscSceneReadingLinksV1.h` appends an optional size/tag/version-checked suffix.
`present_linked` copies the reading document, bitmap and up to 64 link rectangles
in one revision. Rectangles use the page bitmap's coordinates, not physical panel
coordinates. The presenter adds the reading top offset, clips against overlaid
chrome, and emits node 9 VALUE events with the supplied action and region index.
The application resolves destinations from its matching immutable link snapshot.
No URI, application pointer or callback is stored by the presenter.

Text links take precedence over generic edge/center tap zones. Toolbar controls
and scroll/swipe gestures keep precedence over links. Held contacts spanning a
revision change are discarded. Link geometry survives checkpoint pause/resume
and is cleared by the legacy present function. All older API prefixes, refresh
semantics and static-page behavior remain unchanged. Older readers work with
this provider; older providers lack the optional link-touch extension.

`test/scene/reading_links_test.c` drives raw touch events through the real
presenter, including copied bounds, toolbar clipping, gestures, stale contacts,
checkpoint retention, invalid geometry and legacy fallback.
