# Settings 1.3.18 mobile lists

Add `--settings-list-scrolling` to the selected native X4 Settings build. It
requires the existing `--touch-scrolling` profile, including native time and
paper transitions. The resulting Settings version is 1.3.18. Existing recipes
without the flag retain their versions and behavior; no product composition
or publication is performed by this builder.

The root Settings menu and Time/Date field list use the shared bounded touch
controller. The rows follow the finger, decelerate after release, and stop at
both ends. A touch during momentum stops the list without activating a row.
The root footer is one Back button; Time/Date keeps explicit Cancel and Save.
Keyboard selection reveals its row. Each list retains its own offset across
nested editors, while opening a new Time/Date draft starts at its first field.
Fitting choice pages and the Clock Face grid retain their existing layout.

Only the most recent model position is rendered when the display becomes
available. Row and footer presses bind to the completed image at touch-down;
a newer frame completing during a press cannot change its target. Page
changes invalidate that identity, including a return to the same page while
another image is still in flight. Home, Back, Quick Controls and alarm modal
interruptions cancel pending list contact and momentum. The same viewport
clips text, fills, row arrows and hit testing; the scrollbar stays outside the
row text. RTC editing/validation, reader orientation, telemetry, automatic
Light admission and the native grant lifecycle retain their existing owners.

## Verification

Build the normal production Settings recipe with the added selection, then:

```sh
python scripts/test_touch_scroll_settings_lists.py \
  --target-dir build/settings-mobile-lists \
  --output-dir build/settings-mobile-lists-tests --regressions
python -m unittest discover -s tests -p 'test_touch_scroll_build.py'
python -m unittest discover -s tests -p 'test_native_settings_build_profile.py'
```

The fixture compiles the production Settings entry, controller and renderer
with the target's SDK and defines, including the selected idle helper. It
checks target source and SDK hashes before running. Native providers and
elapsed time are simulated; this is not physical panel performance evidence.
Normal and ASan/UBSan runs cover 480×800 and 400×600 logical screens, each with
normal and reversed reader orientation. They capture real MONO1 frames and
check clipping, bounds, finger/momentum frames, drag-vs-tap, held press identity,
BUSY coalescing, newer navigation, same-page reentry, nested editor returns,
static grids, Save/Cancel, modal interruption and retained provider errors.
The optional regressions run timezone and native RTC/custody cases, plus
automatic Light refusal, resume, retained-error and admission-gate checks. The
last group substitutes the deployment idle hook while keeping the real
controller, draft and calling stack. The legacy
RTC fixture omits only root return-app routing; the new list fixture exercises
the complete target policy. Obsolete paging and pre-first-image touch timelines
are replaced by the current scrolling and completed-frame identity cases.

The output contains an evidence JSON with source hashes and per-case results,
plus native PBM frames for visual inspection. Compare a flag-off ELF with its
same-source baseline separately when integrating into another product profile.
