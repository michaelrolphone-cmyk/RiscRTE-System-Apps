# Opt-in BLE Touchpad presentation ownership

`PORTABLE_BLE_TOUCHPAD_NATIVE_UI` adds a statically linked, hidden client API in
`PortableBleTouchpadNative.h`. It does not add a Runtime capability, T5 export,
provider, product build selection, or BLE policy. Native owner **7** is separate
from Buttons owner 5 and other app owners. The implementation is independent of
Scanner owner 6.

## Caller contract

- Call `portable_ble_touchpad_native_begin()` at a new invocation/ownership
  boundary. It invalidates old input identity without clearing a provider-owned
  display token or an immutable sealed raster snapshot.
- Call `portable_ble_touchpad_native_layout(id)` before drawing. The identity
  must change when hit geometry, control availability, or actions change. Keep
  it unchanged for telemetry and pressed-state repainting with identical hits.
- Call `portable_ble_touchpad_native_present(full)` for the matching completed
  image. Direct and raster-snapshot frames retain their own generation/layout;
  stale tokens and completions cannot activate a later layout.
- `portable_ble_touchpad_native_ready(&generation, &visible_sequence)` is a
  read-only observation. It never polls, snapshots, consumes an input event or
  waits. It returns false until that layout's matching frame completes. Once
  visible, the same mapping remains ready while its own repaint is pending.
  The first-visibility watermark stays unchanged across those repaints, so
  queued contacts are not discarded by new display completions.
- The Touchpad application's independent raw subscription owns multi-contact
  reduction and HID output. On a new generation it must reject queued events
  through the returned provider watermark and require a neutral contact state
  before adoption. `ready()` is a presentation fence, not a replacement for
  that subscription's sequence, cancellation, neutral, release or cleanup
  rules. HID releases and cleanup must continue even when presentation is not
  ready. The adapter's own single-contact reducer gates only app chrome.

Foreign/ordinary sealed or submitted frames invalidate the native hit mapping.
Touch/provider faults fail readiness closed. Native generation exhaustion fails
closed instead of recycling an identity.

The selector is color-only at runtime. On MONO1, begin/layout do nothing,
`ready()` is false, presentation takes the ordinary owner-0 route, and input and
idle handling keep their existing paper behavior. With the selector absent no
Touchpad API or reducer is compiled.

## Verification

Run:

```sh
python3 scripts/test_ble_touchpad_native_input.py
python3 scripts/test_ble_touchpad_native_optout.py
python3 scripts/test_ble_buttons_native_input.py
```

The first runner executes production reducer and adapter tests normally and
under ASan/UBSan, with direct and immutable snapshot presentation. It covers
pending repaint contacts, complete queued contacts, delivered pending samples,
held release, multiple inherited contacts, layout/reset fences, stale tokens,
foreign owners, pipelined snapshots, synchronous completion, provider faults,
generation exhaustion, and failed-display custody. Both real adapter objects
are compiled and all four API symbols must have hidden visibility.

Eight MONO1 runs compare the actual pixel hash and input-provider call counts
with the selector on/off, direct/snapshot, and normal/sanitized. The second
runner checks exact preprocessed-token equality in 28 selector-off compositions
against pinned System main `0898ae346645473a2ee19201b60d1d92bcd40fe7`, including
existing Buttons, Quick, paper, idle, native and snapshot selections. CI runs
these checks and the existing Buttons reducer regression.

These are deterministic host-provider tests. They do not claim BLE peer,
physical device, hardware, release, deployment, or complete-system qualification.
The failed outstanding display test deliberately retains custody. LeakSanitizer
is disabled because it cannot run under the sandbox tracing environment; clean
paths assert that every tracked grant, subscription, frame and allocation is
released. AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.
