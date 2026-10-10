# Wi-Fi scan, saved profiles and shared keyboard repair

Working source checkpoint on the published universal-UI source 3a0145a.
This is not a release or a completed hardware fix.

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
- Sixteen production-controller workflows and fifty-profile transactional/fault
  tests pass normally and with ASan/UBSan. These use copied provider tables and
  synthetic inputs, not a physical radio or real credentials.
- Current shared-text Home/Back/client fixtures were recovered from the separate
  text-0.1.2 source checkout rather than retaining stale prefix assumptions.

## Remaining work

Native worker and actual-provider integration are still in progress. Shared SDK
resource contention requires explicit temporary BUSY deferral in the app and
adapter. This checkpoint is not the complete freeze fix. The earlier legacy
48-case fixture has three identical failures on the unchanged base (19,22,23);
no claim of full legacy regression passage is made. Combined target/runtime
qualification and source handoff follow the native integration.

Credentials remain plaintext app-owned storage with explicit Save and logical
Forget. No encryption, secure erase, real credentials or credential logging is
introduced. Existing manual entry, scans, status, security selection, connection,
cancellation, Back/Home, help, radio policy and scrolling must be retained.

## Resource-deferral checkpoint

The copied async provider suffix now carries a same-owner-turn service lease.
The app distinguishes KV BUSY (no I/O begun) from a failed or unconfirmed write,
keeps lookup cursors and explicit actions while waiting, and accepts Back without
unmapping a live radio operation. A successful lease is always paired before
any yield. Native/shared-adapter union tests are still being completed.

The old legacy fixture changed snapshots without emitting touch edges. The
current reducer correctly rejects that as lost-event corruption. Its fixture
now emits monotonic DOWN/MOVE/UP and button edges with matching snapshots; all
48 assertions across both UI profiles, both orientations and both compiler
modes pass on both the unchanged base and current code. Shared text has 132
actual Runtime executions per normal/sanitized mode, including masked sessions.
Secret copied text requests are explicitly wiped on every return path.
