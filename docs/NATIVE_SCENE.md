# Opt-in native scene ownership (current composition)

`PORTABLE_NATIVE_SCENE` adds a private statically linked presentation/input gate;
no Runtime, provider, capability, T5 ABI, persistence or sleep policy changes.
Selector-off adapter statements are checked against the preceding source.
The initial composition requires native canvas, app-owned touch chrome and the
current custody fence. It supports full color frames and existing returning
foreground Light/alarm handling. Snapshot and Quick composition are explicitly
unimplemented for this new selector; their existing profiles remain unchanged.
Do not silently enable them without their sealed-frame/eligible-DOWN metadata.

`PortableNativeScene.h` describes the application interface. The optional
`PortableTimecardNative.h` is a current-profile source facade; it is not a claim
of binary or frozen c071 builder compatibility. Existing Timecard model, civil
ledger, checked AppData, frozen manifests and historical profiles are untouched.

## Ownership

- `begin` advances bounded generation and invalidates inherited scene/contact;
  it does not discard an outstanding display token or provider lease.
- `layout` changes only when the semantic hit map changes. The app's identity
  must include control geometry and semantic targets, not merely pixel equality.
- `present` tags the actual submission with generation/layout. The adapter's
  existing single in-flight token owns it; token values are opaque, not monotonic.
  UINT64_MAX is valid. Zero cannot represent an in-flight token in this adapter
  and therefore retains rather than fabricating completion or safe cleanup.
- Confirmed completion of the matching scene marks a pending observation. An
  old generation completion cannot authorize a newer hit map. Changed-map frames
  cannot vanish through pixel-equal damage elision. Same-map repaint preserves
  an already-owned gesture only while current scene/input authority remains.
- The next ordinary poll establishes the visibility sequence through the current
  reducer's reset/snapshot boundary. Known failed polling, terminal state or
  incomplete resynchronization defers it without a snapshot call. This avoids
  copying frozen Timecard's direct snapshot-after-failed-poll defect.
- `ready` and `contact_read` are provider-free observations. Contacts describe
  only the current ordinary poll, including valid/began/down/released/cancelled;
  consumed modal/navigation polls expose cancellation. A fresh eligible DOWN
  newer than the visible watermark must own a matching contact before release
  can activate anything. Invalid coordinates and inherited ineligible starts
  are cancelled, not turned into fresh gestures.
- Foreign frames and actual foreground suspension invalidate the scene. Alarm
  restore preserves the app pixels/model but does not by itself restore hit-map
  authority; a new app frame and neutral/fresh input are required. Light keeps
  the app's model/grants in RAM under the existing helper contract.
- `back` queues ordinary adapter Back; it does not launch/release directly.
  Physical Home still follows the existing terminal handoff path.
- Exhausted generation fails closed without wrapping. Terminal input/display/
  sleep/alarm custody preserves the existing no-later-provider-call fence.

## Owning qualification

`scripts/test_native_scene.py --runtime <current Runtime> --alarm-sdk <canonical
AlarmServiceV1/V2 directory> --baseline <be142 source> --output <directory>`
runs a pure state fixture and production adapter with synthetic display/raw
input/Runtime/Light/alarm-service media. `--sanitize` adds ASan/UBSan; ordinary
execution does not run heavy renderer or target builds. The runner stages one
canonical SDK identity rather than including duplicate Runtime definitions.

Cases cover delayed changed-map held input, stale completions, same-map gesture,
neutral/fresh edge, malformed polling before completion with no snapshot call,
begin/rebegin during failed polling with an outstanding token, begin after
retention, true next/snapshot retention, invalid geometry, physical Home, generation limit,
opaque maximum/zero tokens, actual foreground helper success/refusal/BUSY and
retained/failed checkpoints, and actual alarm modal ACK/restore/retained paths.
Legacy, native Wi-Fi, Settings, foreground and alarm selector-off preprocessed
adapter parity is checked against the supplied baseline.

These are owning source proofs, not target admission, full product qualification,
physical input usability, storage durability, actual sleep or hardware results.
A Timecard product profile still needs its authoritative model/storage integration,
current preference/AppData BUSY semantics, target memory/import tests and complete
existing cohort regression. Snapshot/Quick are not qualified by these tests.

## Direct-frame Quick composition

`PORTABLE_QUICK_NATIVE_GEOMETRY` explicitly selects full native display coordinates
and the provider's safe-content rectangle for shared Quick controls. Quick gets
raw panel input before an app's SafeViewport conversion, so a top-edge gesture
outside inset content can still open the sheet. Its drawn/hit geometry is the
existing native Quick controller/render contract; existing selector-off adapters
and the separate Home implementation are unchanged. Raster-snapshot composition
remains rejected, not silently downgraded.

An eligible original DOWN reserved by Quick can be replayed only on its same
visible scene generation/contact and after its original sequence watermark.
The original DOWN is delivered before the saved current edge; a changed map,
foreign frame, malformed report, outside-content start or terminal provider fault
cannot manufacture app eligibility. Modal and restored underlay submissions
invalidate the scene; completion of those bytes is not app map authority. The app
must repaint its current map and observe neutral input before a fresh action.

A retained native Quick display submission returns before yielding, releasing or
restoring resources. This is opt-in composition fencing, not a change to Runtime
custody or a reason to erase an app draft. The focused adapter fixture executes
actual Quick gesture reduction, native 466x466 rendering, Back/close/underlay
restoration and failed-submit fencing with synthetic media. It is not device
or final product admission; current Timecard separately declares the existing
`storage.preferences` capability for Quick writes.

Quick's non-radio profile retains its original WI-FI SETUP action: the actual
native tile closes/restores the overlay and requests `wifi_settings.elf` once.
It does not claim or require a Wi-Fi enable-toggle capability. The focused fixture
checks that destination through real shared Quick reduction and handoff. Native
capability-safe rendering only disables the Wi-Fi tile for unavailable radio
controls when that radio profile is actually selected.
