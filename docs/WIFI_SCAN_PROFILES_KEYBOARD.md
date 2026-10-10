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
- A bounded eight-profile wrapper reuses the current atomic credential codec.
  Slot zero retains the legacy keys/default used by network tools. Other slots
  have independent selectors and never evict an existing network. Reads do not
  migrate/write, uncertain slots are not overwritten, and explicit per-profile
  forget retains the codec's existing tombstone/remanence guarantees.
- Current shared-text Home/Back/client fixtures were recovered from the separate
  text-0.1.2 source checkout rather than retaining stale prefix assumptions.

## Remaining work

The Wi-Fi application has not yet been wired to the new profile wrapper or
shared keyboard. Native SDK setup/cleanup is still synchronous and cannot be
forcibly cancelled safely. A bounded async lifecycle is being designed; no
pending operation will be treated as successfully cleaned up. The original
physical freeze has not been attributed, and no device or network is accessed.
Target builds, combined app tests and final source handoff follow integration.

Credentials remain plaintext app-owned storage with explicit Save and logical
Forget. No encryption, secure erase, real credentials or credential logging is
introduced. Existing manual entry, scans, status, security selection, connection,
cancellation, Back/Home, help, radio policy and scrolling must be retained.
