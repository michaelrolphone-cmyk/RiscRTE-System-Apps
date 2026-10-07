# Capability-selected Nova7 E-Ink Springboard

Springboard 1.6.0 adds the supplied **NOVA-7 E-Ink — 480×800.html** presentation
as a private shared-app/client path. It reads `display.output@1` geometry,
MONO1 support and retained-image flag; no board name, register, pin or kernel
UI code selects the theme. Logical portrait retaining displays 400–600 pixels wide and
at least 600 pixels high use the static 3/2 alternating circular grid. Existing
compact fast RGB565 displays use their original animated Watch presentation;
landscape T5 displays retain their original path. Tests exercise the same app
and adapter C source, not a separate mockup implementation.

Only the deployment's admitted `portable_catalog` is shown. The five X4
identities are File Browser, Serial Monitor, BLE Scanner, Points in Time and
Settings. The launcher abbreviates labels where needed, preserving identities.
It does not install those apps or invent the prototype's Phone/Mail/etc apps.
Pages contain at most 15 real entries. Selection is inverse black/white with a
heavy outside ring; the footer contains page dots. Taps, neutral-guarded buttons
and page taps are discrete. There is no momentum, transition, launch pulse,
per-second redraw or other e-paper animation. Clock polling redraws only after
the displayed minute or validity changes. The initial frame requests a clean
refresh when the provider advertises that capability.

The minimal client acquires MONO1 directly, honors its stride/size and MSB-first
black-bit convention, and thresholds glyph coverage to one bit. It allocates
no RGB565 staging frame. An optional 48 KB MONO1 comparison buffer computes
aligned native damage on capable providers; allocation failure falls back to full frames. True Font Awesome glyphs, including the terminal icon,
are retained with OFL provenance. New 28px Orbitron 700 headings and 20px
Rajdhani 600 labels are reproducible with `scripts/generate_paper_fonts.py` from
the existing licensed inputs. Deployments must retain the added paper font
license notices emitted by the development builder.

`python scripts/test_springboard_paper.py` runs 55 ASan/UBSan cases: 0/1/5/17
apps, idle-frame suppression, single launch, inherited held contact, drag,
failed submit/launch, native partial damage, paging, invalid stride and failed touch polling. Each
checks frame/grant/subscription teardown. The existing Watch motion, lifecycle,
pixel and host tests remain separate regression gates. `PAPER_FRAME` optionally
writes the actual first MONO1 frame as PBM for visual inspection. Environments
which run under ptrace may need `ASAN_OPTIONS=detect_leaks=0`; bounds and UB
instrumentation stay enabled, and explicit lifecycle counts remain asserted.

The X4 provider actually exposes an 800×480 native MONO1 buffer with a 100-byte
stride while GT911 reports logical 480×800 touch. `--display-rotation 90` selects
the same software portrait mapping as Reader's GfxRenderer: `(x,y)` becomes
`(y, native_height-1-x)`. This is an explicit deployment mounting choice because
`display.output@1` does not encode mounted orientation. It does not request an
unsupported provider rotation or infer rotation for existing T5/Watch apps.
Tests compare all 384,000 pixels with the unrotated portrait reference and run
the same tap/failure/lifecycle scenarios against the native geometry.

The portable target builder accepts `--navigation` for X4's generic navigation
capability and does not need a device-specific app source. RTC display still
requires an explicitly selected time policy; otherwise the toolbar displays
`--:--`. This change does not establish that a complete X4 bundle is installed.
Settings, Points, File Browser and Serial Monitor need their own e-paper screens
and capability integration. The foreground alarm overlay also uses a native-format snapshot, large monochrome
controls, exact-token dismissal and byte-exact foreground restoration; tests
cover its real MONO1 lifecycle and retain the existing Watch alarm regression. No hardware
qualification, release, merge or device flashing is implied by these tests.
