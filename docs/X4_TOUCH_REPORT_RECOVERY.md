# X4 touch report recovery

The X4 native shared adapter now treats `poll(false)` and `next(-1)` according to
`RiscTouchV1`: drain the bounded event stream, obtain an authoritative snapshot,
cancel derived gesture and Home state, and require neutral before a fresh action.
`next(-2)` (and unknown more-negative results), snapshot failure, and uncertain
subscription/grant cleanup remain terminal or retain their visible ownership.

This is report-loss recovery. It does not repair physical I2C, reset the GT911,
add multi-contact support, or establish a fix for battery-only startup. The
selected GT911 0.1.8 intentionally rejects two-contact reports. The 0.1.29 device
log establishes explicit application retention after touch, but lacks the first
operation needed to identify the actual trigger on that device.

With stage logging enabled, input loss emits its operation/result and
`action=cancel-gesture-wait-neutral`. Identical consecutive loss messages are
suppressed, and a healthy sample rearms the diagnostic latch. True touch faults
emit `action=retain-invocation` before the shared retention fence; cleanup failures
identify `unsubscribe` or `release`. The fence itself emits `stage=app-retain`
before Runtime revokes diagnostics. Native display failures preserve their detail.
No move-event logging was added.

## Qualification

Compatibility regressions also passed: 472 native app custody cases, four plain
stage-log configurations, and 36 paper/Watch touch-dispatch comparisons with
identical pixels, provider calls, and event outcomes.

`receipts/x4-touch-report-recovery.json` records 50 normal/ASan+UBSan cases and
validated development Xtensa HID ELFs. LeakSanitizer is disabled. The durable
runner is `scripts/test_x4_touch_recovery.py` and accepts explicit local Product,
Utilities, Runtime and provider-SDK paths; it does not fetch or publish anything.
Its optional `--target-compiler` uses the existing Xtensa GCC 8.4 toolchain.

- Product source: `9d40063` and its selected
  `minimal/drivers/x4pro_gt911/driver.c` SHA-256
  `1bcf7360a473fd20aef62bc974ec0bc87e78303015512098902e89826b64d322`.
- Shared adapter baseline: `7b175418e3063d9f4c571acf06c366144831b2af`.
- Actual HID app source: Utilities `849581973a48daf07770446712a1c1fd4dfbc77c`.
  Fixture checkout `2268c32f4a3d85d00ac73a744fd578599fa0a752` differs only in the
  test unsubscribe-refusal path, preserving the real provider token.
- Runtime SDK: the recovered 0.1.75 checkout. Exact header hashes are in the receipt.

The consumer layer covers two-contact, out-of-range, real provider queue overflow,
transient status-read failure, ambiguous ACK, true failed unlock, grant release
refusal, canonical `next(-2)`, and snapshot failure. After each recoverable error,
a held touch/Home is ignored for actions until release; a fresh neutral/down/up
and fresh Home press work, with both subscriptions cleanly released.

The composed layer links the unchanged production Touchpad/Buttons controllers,
full shared native adapter, and actual selected GT911. Only physical GPIO, I2C,
clock, display, Runtime and BLE transports are modeled. Each app covers all five
report-loss scenarios, a provider unlock failure injected at the shared consumer
read, an adapter unsubscribe refusal, and the existing HID checked close/release/
unsubscribe retries. Tests assert no phantom mouse/key actions during recovery,
no inherited Home launch, bounded repeated diagnostics, diagnostics before the
retention callback, and frozen providers/pixels/grants/free calls after retention.

The target builds validate Xtensa ELF structure and allowed imports/exports for
both native HID apps. They are development ELF evidence, without release-version
allocation, product admission, device installation, or over-the-air BLE testing.
Central integration must rebuild its selected final cohort after combining changes.
