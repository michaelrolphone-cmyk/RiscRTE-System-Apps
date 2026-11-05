# Frequency native presentation ownership

`PORTABLE_FREQUENCY_NATIVE_UI` selects private owner10. Owner8 remains Calculator;
owner9 is not implemented by this change. There are no Runtime or T5 ABI changes.
The statically linked hooks are hidden symbols:

- `portable_frequency_native_available()` reads the runtime pixel format. Only
  RGB565 selects the native presentation; paper keeps the original app path.
- `portable_frequency_native_begin()` starts a native presentation generation.
- `portable_frequency_native_layout(id)` changes the visible hit-target identity.
- `portable_frequency_native_present(full)` submits an owner10 frame.
- `portable_frequency_native_ready(generation, visible_sequence)` only observes
  readiness and the provider sequence fence. It never polls, snapshots or waits.
- `portable_frequency_native_repaint()` only observes whether invalidated native
  pixels can be restored without overtaking frame, overlay or handoff custody.
- `portable_frequency_native_back()` queues the adapter's existing Back event.
  The following app poll owns ordinary drain, session suspension and return.

A completed matching frame establishes visibility. Inherited held contacts and
pre-visibility edges cannot activate controls. An unchanged native repaint keeps
queued/held input actionable. Layout changes and foreign/overlay frames invalidate
the mapping, including while an older native frame remains pending. Quick capture
cannot retain a Frequency gesture. After overlay dismissal, the app repaints its
native map before controls become ready. Foreign owners retain their raw input
stream; a rejected Quick top-edge tap replays down and release in order for native
app chrome. Sealed raster snapshots retain their
original owner, generation and layout through replay and reset.
Quick replay retains the original DOWN's sequence, generation, contact identity
and oriented coordinates. Eligibility is captured before reservation; a later
release cannot turn a pre-visibility contact into a fresh native gesture.

This change does not modify audio behavior, Utilities, recipes or released
products. Physical display and audio-device qualification are separate.

## Host validation

`python3 scripts/test_frequency_native_input.py` runs the production reducer and
real adapter with direct/snapshot presentation and Quick on/off, normally and
under ASan/UBSan (ten executions).
Coverage includes queued/held input, stale callbacks, immutable reset identity,
pipelined snapshots, foreign owners0/8/9, synchronous completion, queued Back,
pre-visibility DOWN/post-visibility UP, changed-layout replay cancellation,
Quick takeover/return while held, provider failure and cleanup. Four real adapter
objects verify all seven hooks stay
hidden with snapshot and Quick options independently selected.

`python3 scripts/test_frequency_native_optout.py` compares every preprocessed
token against System `4b11af9e44bc7ec71a7a63d14d898b6141c616b4` across 40
feature-off compositions, including Calculator. No Frequency API is emitted.

Existing Calculator reducer and feature-off tests remain independent controls.
