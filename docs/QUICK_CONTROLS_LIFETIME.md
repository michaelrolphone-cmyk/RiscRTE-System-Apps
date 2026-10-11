# Retaining paper QuickControls lifetime and display updates

The shared paper sheet stays open while the user inspects it, including after
finger release, minute changes, delayed panel completion and more than one
minute without input. The prior shared 60-second modal cancellation now applies
only to the animated Watch sheet. This is selected by the existing paper UI
capability, so it also covers paper apps without manual-only sleep.

The shared adapter also records its rendered UI state before submitting the
frame. Asynchronous presentation services raw input while the panel is busy;
recording the state after completion incorrectly acknowledged input that had
never been rendered. New brightness changes now remain dirty and receive the
next frame after the in-flight frame completes.

## Verification

`test_sparse_clock_navigation.py` builds the actual sparse Clock controller,
shared adapter, gesture/session code and X4 local sleep source against a target
Clock's SDK. It rejects mismatched source, SDK, sleep-owner or ELF hashes before
running the host fixtures. It uses the raw touch and navigation capability
interfaces, with no direct mutation of modal state.

- 144 normal/ASan/UBSan exact-profile cases include the existing navigation,
  cold RTC recovery, lifecycle and retained-custody checks.
- Five released-finger inspection cases each wait 90 seconds across two minute
  updates and verify every control row remains present. Each runs with delayed
  synchronous and asynchronous completion, then closes by center, crown, Back,
  close tap or upward swipe, restores the saved Clock pixels, and verifies a
  fresh swipe still launches Springboard.
- The brightness regression moves twice during a busy presentation and
  releases. Hardware and read-back-verified stored brightness both reach 100%;
  the final complete image must exactly match the sequential reference.
- A further 20 normal/ASan/UBSan inspection cases omit manual-only sleep to
  exercise the same paper lifetime rule under the ordinary app idle policy.
- The existing 124 paper QuickControls/Home cases and 104 Watch adapter cases,
  plus pure-controller, session and Home edge/rearming tests, pass.
- The target Clock ELF passes the pinned Xtensa structural and import/export
  checks with Runtime SDK `2abc312217fadb04ac0d9c994ecb38edc15edf6a`, X4 sleep
  source `e2281ade9eab04e8252c9861cda01a228d613859` and Utilities alarm SDK
  `637e13b0bce62ad49b756bec2468a6271d163fc7`.

Both new regressions were reproduced using unchanged System
`31d781b5a73999b8306c67eaeda3ce98b9b5f7ab`: the inspection control-pixel assertion
fails when the timeout restores Clock, and the busy brightness image differs
from the reference despite hardware and stored brightness reaching 100%.

These are production-source tests with host provider doubles and a validated
target ELF. They do not execute the product BIN or claim physical panel timing.

## Reproduction

Build the sparse Clock using `scripts/build_paper_clock.py` with the selected
Runtime/driver SDKs, X4 sleep source, quick-actions, quick-radios and tagged alarm
profile. Then run:

```sh
python3 scripts/test_sparse_clock_navigation.py \
  --system-source /path/to/system \
  --candidate-clock /path/to/built/default \
  --x4 /path/to/x4 --runtime /path/to/runtime \
  --output-dir build/quick-lifetime-tests
python3 scripts/test_paper_quick_actions.py
python3 scripts/test_quick_actions.py
```

The runner writes evidence JSON, per-profile logs and sequential/busy brightness
pixel captures. Its additional `auto-idle` profiles deliberately remove the
manual-only compiler option while keeping the same tested production sources.
