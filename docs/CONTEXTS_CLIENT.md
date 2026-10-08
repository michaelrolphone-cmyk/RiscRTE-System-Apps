# Contexts client and presets

`PORTABLE_CONTEXTS_CLIENT` selects one ordinary `contexts.service@1` grant in
the common app adapter. The service belongs to Utilities; Runtime retains no
room, classifier, preference or user-interface policy. The copied SDK header
must match Utilities byte-for-byte in a deployment.

Monitoring has an ordinary enable/disable control. Missing or unreadable
`contexts_on` never starts capture and never creates a preference. Audio/RF
source owners export their own validated saved models through an explicit
handoff; the service has no access to their storage namespaces. A failed export
requires explicit retry. Signature-based room/event matching is currently
available; temporal/neural import remains separate unfinished work.

The adapter pumps Contexts only at a completed foreground frame. Foreground
audio, RF, Wi-Fi, update and BLE applications reserve their corresponding
resources. Background RF requires Wi-Fi enabled, Bluetooth disabled and airplane
mode disabled; a refused native lease never suspends another owner. The low
battery edge suppresses background capture. Alarms reserve their outputs before
the next output phase, then pause Contexts. Quick Controls, explicit sleep,
ordinary idle sleep, app-data operations and failed cleanup pause the same
background providers. Background capture does not keep the watch awake.

`PortableBackgroundServices.h` closes Contexts before BLE and stops immediately
if cleanup is unconfirmed. `PortableBroadcastAppData.h` preserves its original
API and backend results while applying this shared fence when Contexts is
selected. After a native retained result, no additional storage or radio I/O is
permitted. Ordinary app exit releases the client grant after its capture is
paused; the next app resumes permitted observation from the boot-session models.

Eight namespace-1 records, `ctx_p0` through `ctx_p7`, hold independent 64-byte
presets. Explicit little-endian fields, a schema, reserved-byte checks and CRC
protect each whole record. Saving requires exact readback, including uncertain
write outcomes. Corrupt or unreadable records are never treated as empty.
Each preset binds source, slot and the exact saved room name. Replacing or
renaming a model cannot silently inherit another room's actions.

Selected actions are sleep mode, idle/deep timers, brightness, alert volume,
DND, sound/vibration mode and a product-provided stable watch-face ID. A
confirmed source-local room entry is claimed by the service before any settings
write. Pause, ambiguity, temporary loss of observation and ordinary app switches
do not replay the entry. Manual changes therefore persist for that entry.
A real transition through another confirmed room, including one without a
preset, permits application on return. A cold boot starts a new observation
session; models are re-exported and rooms must be confirmed again.

Preference application stops at the first unverified write. It reports partial
success without replaying already-written fields. Low battery suppresses preset
power fields and reports partial application when that changes the selected
action set. Contexts' editor and foreground capture/network apps defer automatic
preset writes. Unread records, duplicate room mappings, ambiguous results and
disagreement between audio and RF room labels cause no preference changes.

The deployment supplies `PORTABLE_CONTEXT_FACE_COUNT` and
`PORTABLE_CONTEXT_FACE_NAMES`. Without a catalog, the generic client reports zero
supported faces. Watch's context deployment uses its own stable face table; no
Watch face name is hard-coded in Utilities.

Verification:

- `scripts/test_context_preferences.py`: actual encoding/storage helpers,
  512 single-bit corruptions, malformed fields, missing data and uncertain saves.
- `scripts/test_context_client.py`: policy, radio/low-battery coexistence, claim
  custody, manual overrides, partial saves, ambiguity and retained cleanup.
- `scripts/test_context_adapter.py`: actual common adapter, Quick Controls,
  alarms/cues, ordinary exit, native retained sleep and idle sleep.
- Existing BLE/app-data, portable app/catalog and repository unit regressions.

Focused suites run normally and under ASan/UBSan. They qualify software ordering
and bounded copied data; physical audio/RF recognition remains hardware work.

## Capture servicing

The opted-in adapter calls the appended capture-only method during raster work,
pending display transfers and input polling. Deliberate waits are split into
at most 8 ms slices. These checkpoints drain only audio already owned by the
service; they do not read preferences, open providers, sample RF or apply presets
while a frame is borrowed. Wi-Fi and update invocations suppress background audio
because their synchronous operations cannot promise this cadence.

A capture cleanup refusal uses the Runtime terminal invocation fence before any
more display, input, service or fini calls. Host tests cover raster, submission
and input failures plus the intentional-wait bound. Actual hardware timing is
not inferred from these tests; the service independently invalidates and closes
an audio stream when its 32 ms queue deadline is missed.
