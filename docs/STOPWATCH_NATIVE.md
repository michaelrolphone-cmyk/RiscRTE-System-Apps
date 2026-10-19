# Stopwatch native presentation ownership

`PORTABLE_STOPWATCH_NATIVE_UI` selects the private owner13 adapter. Existing
Frequency owner10, Files owner9, and Calculator owner8 remain unchanged. Owners11
and12 remain unimplemented here. There are no Runtime or T5 ABI additions.

The statically linked hooks in `PortableStopwatchNative.h` have hidden visibility:

- `portable_stopwatch_native_available()` observes whether the runtime surface is
  RGB565. Paper formats retain their original app and input paths.
- `portable_stopwatch_native_begin()` starts a new native presentation generation.
- `portable_stopwatch_native_layout(id)` identifies the visible control hit map.
- `portable_stopwatch_native_present(full)` submits an owner13 frame with that
  generation and layout identity.
- `portable_stopwatch_native_ready(generation, visible_sequence)` observes the
  completed frame and raw-provider sequence fence without polling or waiting.
- `portable_stopwatch_native_repaint()` observes whether an invalid native map can
  be restored without overtaking an outstanding frame, overlay, or handoff.
- `portable_stopwatch_native_back()` queues ordinary adapter Back. The next app
  poll owns provider drain, session suspension, and return.

A matching completed frame establishes the native input map. Inherited held
contacts and pre-visibility edges cannot activate controls. Same-layout timer
repaints preserve queued and held gestures. Changed layouts, foreign frames, and
Quick takeover invalidate the map without consuming the foreign owner's raw
input stream. Native controls must be repainted after overlay return. Once Quick requests the
screen, owner13 stops admitting new foreground snapshots until the existing
queue drains. This prevents fast running-timer refresh from starving Quick; modal
paint and retained-underlay restoration keep their ordinary frame admission.

A rejected Quick top-edge tap replays its original DOWN before its saved current
sample. Eligibility retains the original sequence, generation, contact identity,
and oriented coordinates. A later release cannot promote a pre-visibility DOWN
into a new native contact. Beginning again preserves the identity of outstanding
tokens and sealed raster snapshots. Re-entering the adapter requires an explicit
new native begin and a visible matching frame before controls can activate.

This System change does not implement Stopwatch timer behavior, app controls,
recipes, Utilities integration, or any released product. Physical-device display
and input qualification remain separate.

## Host validation

- `python3 scripts/test_stopwatch_native_input.py`: two normal/ASan+UBSan
  production-reducer executions, including runtime paper bypass, all foreign
  owners0..12, visibility/neutral fences, held/queued input, Quick replay,
  stale completion, snapshot faults, and generation exhaustion.
- `python3 scripts/test_stopwatch_native_adapter.py`: eight normal/ASan+UBSan
  real-adapter executions across direct/snapshot and Quick on/off. Includes
  pipelined snapshots, preserved immutable token identity, synchronous completion,
  raw contact custody, repaint fences, Quick snapshot priority, queued Back, module
  re-entry, and retained
  cleanup after provider failure. Four real adapter objects verify all seven
  native hooks remain hidden.
- `python3 scripts/test_stopwatch_native_optout.py`: exact preprocessed-token
  equality against System `c94f367f6ca3677954b580833f222ae16775c964` in 68
  feature-off compositions, covering every existing native selector, paper,
  Quick, and snapshot options. No Stopwatch hook is emitted.

Frequency, Files, and Calculator tests remain independent controls.
