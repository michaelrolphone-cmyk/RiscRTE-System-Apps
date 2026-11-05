# Timecard native presentation ownership

`PORTABLE_TIMECARD_NATIVE_UI` selects private presentation owner14. Existing
owners0–13, Quick, Alarm, retained display custody, and public Runtime/T5 ABIs
remain unchanged. The selector alone does not activate the native input path.
Paper formats preserve their existing controller and input path.

## Private hooks

`PortableTimecardNative.h` exposes eight statically linked, hidden hooks:

- `portable_timecard_native_available()` reads the runtime RGB565 selection.
- `portable_timecard_native_begin()` starts a new presentation generation without
  replacing outstanding provider tokens or sealed snapshots.
- `portable_timecard_native_layout(id)` identifies the semantic hit-target map.
  A changed ID cancels inherited contact; unchanged IDs preserve held scrolling
  and queued contacts across same-map repaint. Keep the ID stable throughout an
  owned scroll, and change it when the semantic control map changes.
- `portable_timecard_native_present(full)` submits owner14 with its generation
  and layout identity.
- `portable_timecard_native_ready(generation, visible_sequence)` reads visible
  ownership and the raw-provider sequence boundary without polling or waiting.
- `portable_timecard_native_contact(out)` reads the current app poll's ordered
  `valid/began/down/released/cancelled` sample and oriented coordinates. Read it
  once after each poll. Held movement remains available beyond tap slop; valid
  neutral samples support picker settling. A modal/navigation-consumed poll
  yields cancellation rather than replaying a previous release. The hook never
  reads ahead, consumes an edge, or polls the provider.
- `portable_timecard_native_repaint()` reads whether an invalidated map can be
  restored without overtaking frames, overlays, or a return handoff.
- `portable_timecard_native_back()` queues existing adapter Back; the next poll
  owns provider drain, session suspension and return.

Only matching completed frames establish input readiness. Pre-visibility and
inherited held contacts cannot activate controls. Malformed/reordered provider
edges, contact replacement, and layout changes cannot become valid release
edges. Foreign frames invalidate the native map while leaving their raw contact
stream in the existing provider's custody.

Quick top-edge rejection replays the original eligible DOWN before the saved
current sample. Generation and original raw sequence fences prevent a later UP
from promoting an old DOWN. Quick capture clears native ownership. Once Quick
requests the screen, scrolling cannot indefinitely replace pending app snapshots;
modal paint and retained-underlay restoration still use normal frame admission.
A matching owner14 completion is required even when restored pixels are byte
identical, preventing RGB damage elision from stranding an invalid native map.

For canonical Utilities API1 headers, the Timecard-selected Quick path retains
the legacy both-output contract without interpreting the sleep-resume callback
suffix as an output descriptor. Tagged UTC and all selector-off branches remain
unchanged.

This System adapter does not implement Timecard tracking, controls, recipes, or
Productivity integration. It changes no released product or physical fixture.
Physical display/input qualification remains separate.

## Host validation

Run from the System repository:

```sh
python3 scripts/test_timecard_native_input.py
python3 scripts/test_timecard_native_adapter.py
# When the canonical Utilities checkout is available:
python3 scripts/test_timecard_native_adapter.py --alarm-api1 ../utilities/lib/Alarm/include/AlarmServiceV1.h
python3 scripts/test_timecard_native_controls.py
python3 scripts/test_timecard_native_optout.py
```

- Input: two normal/ASan+UBSan production-reducer executions, including paper
  bypass, all foreign owners0–13, visibility/neutral fences, same-map input,
  cancellation, Quick replay, stale completion, faults and generation exhaustion.
- Adapter: sixteen normal/ASan+UBSan real-adapter executions across direct and
  snapshot rendering, Quick off/on, and standalone/combined owners9–14. Includes
  held scrolling, current-poll contact, read-only getters, cancelled/reordered
  release rejection, identical-pixel recovery, queued/held contacts, immutable
  tokens, pipelined snapshots, foreign raw custody, Quick snapshot priority,
  Back, module re-entry, failure and retained cleanup. Four compiled adapter
  compositions verify all eight hooks remain hidden. The canonical API1 option
  repeats all sixteen executions with a real sleep-resume callback suffix,
  verifies the callback is never invoked as a capability read, and compiles
  both paper-capable and color-only Quick inclusion branches.
- Controls: byte-identical comparison of 351 existing PortableApps files against
  `ff6151285ee41bdbeff75ebd31ee2621d524e899`, excluding only the four adapter/Quick files
  with gated additions. Thirty-six normal/ASan+UBSan executions cover unchanged
  Calculator, Files, Frequency, LoRa, Points and Stopwatch reducers, plus actual
  Frequency/Points/Stopwatch adapters with Timecard selected but inactive across
  direct/snapshot and Quick off/on.
- Opt-out: exact preprocessed-token equality against that same pinned main in
  84 feature-off compositions, including every existing native selector, paper,
  Quick and snapshots. No Timecard hook is emitted. Only path spelling is
  normalized; no source tokens are removed.

Adapter/control test runners discard each successful compiler output after
execution and retain bounded logs; reducer binaries and parity evidence are
small. These checks do not build firmware or publish artifacts.
