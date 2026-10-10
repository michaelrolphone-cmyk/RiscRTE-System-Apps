# GT911 0.1.10: Clock and selected .57 Home qualification

## Result

- Exact selected .57 sparse Home: **36/36 PASS**.
- Legacy paper Clock compatibility and actual-provider rapid-tap controls:
  **40/40 PASS**.
- Unmodified GT911 0.1.9 control: **34 PASS, 6 reproduced FAIL**. Its six failures
  remain failures in the receipt and are retained beside the new evidence.
- Every matrix includes normal and ASan+UBSan. No physical hardware claim.

The approved provider emits removals, then additions, then existing-contact
motion. This worker changed tests only. System production, PortableTouch,
target build recipes, and workflows are untouched. Earlier original-failure
and GT911 0.1.9 evidence directories remain unchanged.

## Source and profile binding

Candidate driver: 0.1.10, SHA256
`ddb898395bcfb4ca126b284f01b288274da97f34efd2e2e63c9347dff8ed4cb5`.
It was read from `/workspace/shared/x4-gt911-down-before-move-20261010`.

Selected Home production was compiled directly from the frozen
`/workspace/shared/ui-qualified-raster-source-20261010/system` tree. Every
production `-D` flag in
`/workspace/shared/ui-decoupling-final-057-target/apps/default/build-evidence.json`
is retained, including sparse Clock, resident host/policy, Quick, Contexts,
BLE broadcast, crash reports, display settlement and raster snapshot. The
exact target invocation is `apps-default-command.json`; no target recipe ran
or changed during this host qualification. Both compile commands are retained.

The receipt hashes the full selected production input set, all staged headers,
actual GT911/physical-I/O fixture, host harnesses, and target configuration.
Runtime, display, alarm, storage, navigation and optional services are host
providers. Contexts is explicitly unavailable, BLE defaults off, and the crash
spool is a healthy empty namespace. The complete renderer/input feature set
remains compiled; these service states do not remove feature flags.

## Selected Home cases

Each case runs with immediate completion, 200 ms completion, and a permanently
pending display, in normal and ASan+UBSan builds:

1. Same-report second contact plus motion cancels without launch.
2. A later unchanged ready report advances the snapshot while preserving that
   cancellation.
3. A 2 ms swipe produces exactly one springboard handoff after settlement.
4. A 2 ms Home-block tap produces exactly one Points handoff after settlement.
5. An independent earlier one-finger swipe survives later multi-contact
   capture while waiting for settlement. The slow trace proves the later
   report was captured before the intended earlier swipe's handoff.
6. A refused launch requires a fresh neutral cycle and deliberate retry;
   exactly two attempts occur. Retry is scheduled after the observed refusal,
   rather than disappearing into a slow first-frame boundary.

Permanently pending display cases never hand off. The full selected profile
continues logical contact dispatch independently of pending pixels, then takes
its existing 10-second retention fence exactly once and pins its grants.
The multi-contact trace asserts both DOWNs and both UPs were dispatched within
200 ms of submission, before retention. Valid taps also dispatch both edges.
Every pending status read asserts submitted pixels have remained unchanged.

The test report cursor belongs to the physical provider and survives subscriber
close/reopen. Otherwise a refused resident launch could incorrectly erase the
physical UP and turn a legitimate retry into only MOVE events.

## Compatibility boundary

The unselected legacy profile intentionally waits for its first presentation.
Its permanently-busy cases prove queued capture, timeout and no launch, not
pre-first-frame logical dispatch. The selected Home results above establish
the decoupled model behavior. The legacy profile's rapid stationary tap does
not navigate; selected Home's same coordinate is a real Points block and must
navigate exactly once. Both original semantics are asserted.

## Commands

Selected profile:

    python3 scripts/test_paper_clock_gt911_selected.py \
      --source /workspace/shared/ui-qualified-raster-source-20261010/system \
      --product /workspace/shared/x4-gt911-down-before-move-20261010 \
      --target-command /workspace/shared/ui-decoupling-final-057-target/apps-default-command.json \
      --target-receipt /workspace/shared/ui-decoupling-final-057-target/apps/default/build-evidence.json \
      --sdk /workspace/shared/x4-057-ci-sdk-proof/target-final/sdk \
      --output-dir /tmp/clock-gt911-selected \
      --driver-version 0.1.10 \
      --driver-sha256 ddb898395bcfb4ca126b284f01b288274da97f34efd2e2e63c9347dff8ed4cb5 \
      --require-fixed

Compatibility profile:

    python3 scripts/test_paper_clock_gt911_reports.py \
      --product /workspace/shared/x4-gt911-down-before-move-20261010 \
      --sdk /workspace/shared/x4-057-ci-sdk-proof/target-final/sdk \
      --output-dir /tmp/clock-gt911-legacy \
      --driver-version 0.1.10 \
      --driver-sha256 ddb898395bcfb4ca126b284f01b288274da97f34efd2e2e63c9347dff8ed4cb5 \
      --require-fixed

Use the unchanged selected .57 product and omit the candidate version/hash and
`--require-fixed` to retain the 0.1.9 compatibility-control failures.
