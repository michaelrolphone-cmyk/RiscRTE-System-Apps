# Shared Nova utility UI prerequisite

`PORTABLE_NOVA_UI` is an explicit app-side presentation profile for 240 × 240
RGB565 displays. It uses Settings' actual Orbitron/Rajdhani raster inputs,
cyan `#19e3ff`, dark teal outlines and rounded controls. There is no new Runtime
UI API, rendering callback, service I/O, allocation, storage or hardware policy.
Other dimensions are rejected before painting. Legacy Reader app paths are
unchanged. App owners must advance every newly built bundle's version.

PortableNovaUi provides clipped text, rounded buttons, cards, a 44 × 44 Back
button and a bounded hit helper. The generic list bridge uses two large rows.
Vertical swipes navigate pages and a right swipe requests app-owned Back.
Nested apps must own return routing; the crown is a button, not a rotary input.

PortableNovaKeyboard maps all 95 supported printable ASCII characters exactly
once, in 12 pages of eight characters. Controllers own entry bounds, password
masking, draft cancellation and explicit persistence. Nova Wi-Fi and Points
editors use this shared alphabet, with 49 × 44 character targets.

`catalog-icons.json` is the explicit cross-repository application icon contract.
Every app has a unique genuine Font Awesome glyph. The generator checks the
upstream codepoint CSV and includes the complete set; no missing glyph can be
replaced by a handmade substitute. Alarms uses bell, Countdown hourglass, Points
calendar-day and Calculator calculator. Launcher motion, geometry and colors
are left intact.

The retained foreground alert uses the same theme with bounded Dismiss/Retry
buttons. Exact-token acknowledgment, presentation barriers, uncertain output
retention, radio cleanup and sleep semantics are preserved.

## Evidence

The baseline contact sheet at [screens/before.png](screens/before.png) comes
from actual app/adapter source with deterministic fake peripherals, not a
substitute renderer. It exposes the six legacy white surfaces, missing plus
symbols and two missing launcher glyphs. This is host source-render evidence,
not Xtensa ELF execution or physical qualification. Individual app PRs provide
final after renders and controller coverage.

`python scripts/test_nova_ui.py` tests primitives, keyboard coverage and all
22 plain/ASan+UBSan retained-modal scenarios. `test_nova_apps.py --utilities`
adds real touch-through-adapter controller tests for updated utility sources.
Existing legacy portable, Settings, Wi-Fi, alarm and transition regressions
remain separate required gates. LeakSanitizer alone is disabled locally under
ptrace; hosted defaults are unchanged. No release, merge or device operation.

## Current-source reconciliation (2026-10-05)

The shared profile is reconciled with System Apps main `7723df2`, retaining
the newer portable update services, audio session lifecycle, alarm pump fix,
and all eleven registered distinct glyphs. The prior nine-app icon correction
is already on main; this change adds the missing shared utility presentation.
The optional profile does not enable it for other apps or change their versions.

The audio/retained-alarm fixture runs all thirteen cases under both legacy and
Nova rendering, plain and ASan/UBSan (52 executions). In particular, idle alarm
checks retain live audio, due alarms suspend it before output ownership, and
uncertain cleanup retains the invocation without storage calls. Local runs use
the caller's ptrace workaround; hosted sanitizer policy is no longer overridden.

## Non-modal cue output reservation

Matched service0.4.0 adds the copied CUE state. The client treats it as output
ownership, so current app audio suspends before the cue opens the speaker, but
drains one short cue with the existing frame intact rather than showing an
alarm modal. Stale Back/touch input is consumed through cue cleanup. A64-phase
bound catches an unresponsive cue; stop-only and resource retention remain
unchanged. Subsequent simultaneous cues get a new foreground-call budget.

The actual service/client24-case integration verifies live-audio preemption,
no modal/frame replacement and failed preemption retention. The shared audio
fixture additionally covers successful cue drain, stuck-cue cleanup and failed
app-audio closure in both rendering profiles and sanitizer modes. The matching
Utilities source is pinned byte-exact at `d5a8b206` (service0.4.0, PR22).
