# Remote Watch battery in resident Quick Actions (T061)

## Local implementation, not a published feature

This opt-in shared resident-paper-host implementation shows the last acquired
remote battery percentage and a separate current/not-current indicator. It is
not selected in any product, not a new target-name special case, and not linked
into foreground client apps. Existing feature-disabled builds are unchanged.

Original request, 2026-10-11 15:14:31 UTC, message
`Sentinel_5ff7f99af3f8819190aee5b0273784a5`:

> Add to the queue ahead of app refactors for shared ui components to add watch battery via bluetooth to the quick actions screen so I can see my watches battery level on my x4 (show last acquired value and an idicator if its presently acquired)

## Reviewable implementation choices

- The Watch battery summary opens a resident-owned page without acquiring a
  sensor capability or starting RF. Scan/Restart is explicit and bounded by the
  existing provider's 15-second passive scan. Stop, page Back and drawer close
  stop the scan; nothing restarts automatically or scans in the background.
- The user explicitly selects an observed address **and address type**. No first,
  nearest, named or strongest peer is selected automatically. A new random
  advertiser session may require reselection. This is not pairing or proof of
  ownership. Broadcast values are public and unauthenticated.
- Only a complete, unencrypted, valid snapshot with exactly one battery metric
  is selectable. Percentage zero is a valid reading. Multiple battery metrics
  are ambiguous, not a reason to choose an arbitrary metric.
- The selected peer's last acquired value stays in resident RAM across Stop,
  drawer dismissal and another explicit Refresh. Selecting another peer never
  relabels the old peer's cache. No reboot persistence or stable identity guarantee
  is claimed.
- Current means an active explicit scan most recently observed the selected
  identity's valid battery measurement with age at most 10 seconds. This local
  implementation constant is not a user policy. Polls are at most every 250 ms;
  it is a sampled freshness indication, not a connected/live-link guarantee.
  Presence-only advertisements do not refresh measurement age. Close, completion,
  invalid data or stale data clears current while retaining the last value.
- Bluetooth-off, invalid radio state and Airplane block Scan without enabling
  RF. Post-close failed/unconfirmed restoration invalidates radio confirmation
  and is shown as a warning, while safe cleanup still releases custody. A failed
  close/release retains the invocation and forbids ordinary callbacks.
- Physical Home/crown/handoff preserves terminal navigation, unlike page Back.
  Dismissed, closing, alarm-owned and obsolete queued intents cannot start RF.

These choices implement the requested behavior for local review. They are not
established user preferences or permission to operate a user's devices.

## Build and admission

Select `--quick-remote-battery` and an explicit numeric local candidate version
using `--quick-remote-battery-version`. The existing builder must also explicitly
select the resident host, Quick Actions, Quick radios and paper transitions.
The final version must be newer than the selected ordinary profile. `0.4.21`
is used only as a local test input, not a reservation or claim that it is unused.
No accepted version, payload or product source pin is changed.

The new `bluetooth.sensors@1` declaration is optional. An absent provider/grant
leaves the feature unavailable without preventing Home admission. An explicit
owner-granted binding at instance 0 is still required for actual acquisition;
manifest declarations and receipts do not manufacture grants. The canonical
sensor SDK is copied unchanged and hash-checked.

## Required paired provider repair

The consumer **must be paired with the patched sensor parser** in Drivers
`ba28f9bdfbe84d23de36350189cc2ec89fea1677`, based on
`a7ae39994603c4545a3c014f09051ecfc12084d2` through distinct test-only checkpoint
`fe310f01a90a616ce05be02b677debdf3cfcd607`.

Before the repair, one advertisement containing two BTHome service-data fields
with 40% and 80% silently reduced to the second field. The new parser marks
repeated BTHome service-data fields invalid, clears all readings and preserves
presence. A later single valid field recovers. The consumer cannot reconstruct
ambiguity that an older parser discarded. Passing tests against this patched
source do not qualify any older published sensor ELF.

## Local qualification

From System Apps:

- `python3 scripts/test_remote_battery.py`
- `python3 scripts/test_quick_remote_controls.py`
- `python3 scripts/test_quick_remote_adapter.py --runtime <Runtime repo> --drivers <Drivers repo> --utilities <Utilities repo> --x4 <X4 product repo>`
- `python3 scripts/test_remote_battery_roundtrip.py --drivers <patched Drivers repo>`
- `python3 scripts/test_quick_remote_battery_build.py`
- `python3 scripts/test_quick_remote_manifest.py --runtime <Runtime repo>`
- `python3 scripts/test_quick_remote_manifest.py --runtime <Runtime repo> --sanitize`
- `python3 scripts/test_quick_actions.py`

The adapter test compiles the real adapter/controller/session/renderer with
fake Runtime and sensor callbacks, both ordinary and actual atomic-queue
variants, normal and ASan/UBSan. It produces seven real software-rendered captures
and checks their equality across configurations. It does not run the full
resident event loop, physical RF or a firmware image. The manifest fixture
separately qualifies actual Runtime admission and app entry; neither boundary
substitutes for product/hardware qualification.

The cross-repository test exercises the actual telemetry codec, patched scanner
AD parser and consumer session. Driver suites also cover all 101 percentages,
address/type identity, invalid/encrypted/truncated data and duplicate service
fields in both orders. LeakSanitizer is disabled under this executor's ptrace;
ASan and UBSan remain enabled. No leak-check pass is claimed.

Feature-disabled host objects for quick_actions.c, quick_session.c,
quick_render.c and adapter.c are byte-identical to System base
`7270a10ed9cefba6ec1368060196a13bb3370985` using the recorded compiler/profile.
This is scoped host-object equivalence, not accepted firmware byte equivalence.

## Remaining acceptance

No Xtensa target toolchain was found in PATH or the inspected standard tool
locations. No target ELF, firmware build, remote CI, flash or hardware test ran.
A future product handoff must select exact paired System/Drivers commits, retain
all accepted-byte gates, generate separately versioned candidates and explicit
owner grants, validate memory/imports/lifecycle on the target, and qualify real
Watch-to-X4 reception plus UI/refresh/cleanup behavior. Publication is held.
