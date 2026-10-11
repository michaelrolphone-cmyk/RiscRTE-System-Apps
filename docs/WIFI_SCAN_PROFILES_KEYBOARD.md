# Wi-Fi scan, saved profiles and shared keyboard repair

Software-qualified source increment on published universal-UI source 3a0145a.
The coherent product union and physical-device verification remain separate.

## Implemented and locally tested in this checkpoint

- A delayed observer queries display completion before rejecting an aged token.
  An injected 12-second Wi-Fi setup call reproduces the old false timeout even
  though the display has completed. Only verified COMPLETE can recover; pending,
  callback failure, FAILED and SUPERSEDED remain fail-closed. The actual
  controller/adapter/provider/CPU-port/native path passes normal and ASan/UBSan.
- The shared text service negotiates optional display masking through a new
  size-checked suffix. Plain clients and the existing Home-reason suffix retain
  their layout and semantics. Secret text never enters the scene document;
  only an equal-length asterisk string is rendered. The caller still receives
  its copied plaintext result. Older providers are refused for masked sessions.
- A growing persisted profile index reuses the atomic credential codec. Slot
  zero preserves the legacy default keys. Other slots have independent selectors.
  There is no fixed network-count ceiling beyond backend storage and the uint32
  index space. The UI pages eight names; lookup reads one profile per iteration.
  New slots reserve a checked count before save, so interrupted saves are reusable.
- The actual Wi-Fi controller now uses the shared keyboard, with masked passwords
  and checked close/release before committing a copied result. It supports saved
  network paging, explicit Save/Forget, scan selection, async start/poll/cancel,
  and keeps pending cleanup distinct from failure or confirmed quiescence.
- Fifty-six production-controller workflows per compiler mode and fifty-profile
  transactional/fault tests pass normally and with ASan/UBSan. These use copied provider tables and
  synthetic inputs, not a physical radio or real credentials.
- Current shared-text Home/Back/client fixtures were recovered from the separate
  text-0.1.2 source checkout rather than retaining stale prefix assumptions.

## Qualification and remaining limits

The production app, provider, CPU port and native worker pass 58 cross-layer
cases in each of normal, ASan/UBSan and ThreadSanitizer builds. The native core
passes 56 cases per sanitizer/stage-disabled mode and three target profiles.
The final Wi-Fi 1.1.21 and text-input-host 0.1.3 target builds pass structural,
import/export and manifest checks. See `evidence/wifi-repair-20261010.json`.

The earlier legacy fixture failures were traced to snapshot-only input and
corrected below. Independent review found and reproduced two defects: a queued
Scan survived result selection, and unavailable async admission could fall
back to synchronous setup. Both original reproducers now pass after the fixes.
Native worker-start failure also preserves the advertised suffix and returns
UNAVAILABLE without ownership or SDK work. No physical scan, connection, radio
timing or hardware freeze-causality claim is made. A vendor call may still take
time to return; copied owner callbacks remain responsive and cancellation waits
for its checked cleanup rather than killing the worker or releasing live state.

Credentials remain plaintext app-owned storage with explicit Save and logical
Forget. No encryption, secure erase, real credentials or credential logging is
introduced. Existing manual entry, scans, status, security selection, connection,
cancellation, Back/Home, help, radio policy and scrolling must be retained.

## Resource-deferral checkpoint

The copied async provider suffix now carries a same-owner-turn service lease.
The app distinguishes KV BUSY (no I/O begun) from a failed or unconfirmed write,
keeps lookup cursors and explicit actions while waiting, and accepts Back without
unmapping a live radio operation. A successful lease is always paired before
any yield. Shared-adapter normal/sanitized regressions pass; coherent product integration
uses the same lease and cleanup contracts.

The old legacy fixture changed snapshots without emitting touch edges. The
current reducer correctly rejects that as lost-event corruption. Its fixture
now emits monotonic DOWN/MOVE/UP and button edges with matching snapshots; all
48 assertions across both UI profiles, both orientations and both compiler
modes pass on both the unchanged base and current code. Shared text has 132
actual Runtime executions per normal/sanitized mode, including masked sessions.
Secret copied text requests and polled results are explicitly wiped on every
return path. All direct consumers of the changed helper must be rebuilt in the
coherent source union.

## Final integration contract

- Build Wi-Fi 1.1.21 and text-input-host 0.1.3 together with the published Wi-Fi
  provider 0.2.1 and the coordinated async-native Runtime successor. The app
  manifest newly needs ui.text-input@1; give it that existing host grant.
- The legacy Wi-Fi prefix remains ABI-compatible. A genuinely prefix-only old provider can still use synchronous operations; it
  cannot provide the new latency guarantee. A provider advertising async support
  that returns UNAVAILABLE is rejected cleanly, never downgraded to synchronous
  SDK setup after that admission failure.
- Native setup/result/cleanup executes on a worker with copied request/results.
  Cancellation is accepted promptly, but pending SDK work is not killed or
  declared quiescent. Back/Home waits for confirmed cleanup before release.
- BLE lifecycle and telemetry broadcast actions wait for Wi-Fi quiescence under
  the existing boolean HCI ABI. Existing HCI packet traffic is unchanged.
  Opening radio controls drains Wi-Fi first and preserves the requested action.
- The profile collection grows until storage or the uint32 index space is full.
  Eight names are a display page, not a network-count limit. Slot zero remains
  the legacy default used by other network tools. Save/Forget are explicit.
- Credentials remain plaintext app-owned records; masking only controls display.
  No real credentials, device trace, network connection or hardware action was
  used in qualification. Do not claim physical scan duration from host models.

Native/provider prerequisites and their clean build recipes are maintained with
those repositories. Run scripts/test_wifi_workflow.py, test_wifi_adapter_deferral.py,
test_wifi_scan_latency.py, test_text_input_host.py and test_text_input_runtime.py
with explicit source/SDK paths. All tests exercise production source; the SDK,
clock, touch, display and storage boundaries are synthetic where documented.
