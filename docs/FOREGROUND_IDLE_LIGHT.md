# Returning foreground idle: shared adapter contract

This opt-in path keeps the same native app invocation, model and modal editor
in RAM while entering returning Light sleep. It never synthesizes Home, Save,
Cancel or app exit, writes a draft, or selects Deep. Builds without
`PORTABLE_FOREGROUND_IDLE` retain the existing paths.

## Selection and hooks

Select `PORTABLE_FOREGROUND_IDLE`, `PORTABLE_APP_SLEEP_LOCAL` and
`PORTABLE_NATIVE_CUSTODY_FENCE`. Do not combine with X4 idle, resident host/client,
Desk Clock, or the dedicated awake USB-transfer owner. The latter already rejects
local sleep. The shared code is `lib/PortableApps/src/foreground_idle.inc`, reached
from ordinary input polling and the existing paused text-editor policy bridge.

Implement the declarations in `lib/PortableApps/include/PortableForegroundIdle.h`:

- `portable_foreground_idle_configure(rt)` runs once after settled foreground
  provider setup, before polling. Read the confirmed shared timeout into a cache;
  do not write preferences. Return 0 for available, -2 for retained, -3 for clean
  transient BUSY/IO, other negative values for unavailable. Unavailable leaves the
  UI live with automatic idle disabled. A transient result retries only at safe
  ordinary checkpoints or fully paused modal boundaries, after service readiness,
  using 250ms exponential recovery backoff capped at 4s. The backoff never changes
  the saved/cached idle duration or input activity. Confirmed Save cancels retries;
  corrupt/invalid/unavailable configuration is not retried.
- `portable_foreground_idle_timeout_ms()` is a pure cached query. Zero or a value
  over `INT32_MAX` defers automatic idle. The adapter invents no fallback duration.
  A deployment can explicitly choose a missing-key fallback; valid saved values
  win and unavailable/invalid reads must not be represented as confirmed defaults.
- `portable_foreground_idle_timeout_changed(ms)` changes only that cache. The
  Settings timer controller calls it after a confirmed idle-timer Save, never
  after Cancel, failed Save or a deep-timer Save. It does not reset input activity.
- `portable_app_foreground_idle_sleep(rt, display, gauge)` must enter only returning
  Light. Return 1 after wake, 0 after clean refusal with all hardware restored,
  -1 for failure, -2 for retained/uncertain custody. Negative and unknown results
  preserve the invocation with a silent terminal retention fence, not normal
  cleanup. The adapter retains the original result locally.
- Optional `PORTABLE_FOREGROUND_IDLE_WORK_GUARD` selects the pure RAM query
  `portable_app_foreground_idle_inhibited()`. Use it for legitimate app work not
  represented by the common owner gates. It must not stop or restart that work.

Use the complete canonical Runtime SDK, including `RiscRuntimeV1.h` and
`RiscRuntimeCheckpointV1.h`. The helper also needs `RiscRuntimeSleepV1.h`.
Stage these into the actual copied include directory: placing an earlier `-I`
directory alone cannot override old quoted neighbor headers. This contract does
not add a Runtime ABI. The reviewed base is System
`6e0e4c9b64047ff7be43e8d26ec8ecd588e4c19e` and Runtime
`273bdbccf65647a803ab0586a418f8283732baec`.

## Ownership and eligibility

The boundary requires no mutable frame, presentation token or raster recording;
no held/pending input, navigation action or launch handoff; no modal Quick Controls
or alarm ownership; and no selected atomic preference transaction. It defers for
active continuous audio/radio capture, context capture, selected Wi-Fi/update/file
sharing work, unsafe file handles, loaded catalog storage and configured BLE
advertising. Dedicated USB transfer remains explicitly awake. These gates are not
an assurance for unknown app-specific workflows: select the work-guard hook when
needed and qualify that workflow before opting the app in.

At each attempt, the adapter runs Runtime's service checkpoint and rechecks owner
eligibility. BUSY defers without stopping work. Ordinary app touch subscriptions
and grants close before the helper; navigation foreground claims detach and reset,
but the navigation grant/API remains live. Model/private grants remain live.

After 1 or 0, touch is reacquired and snapshotted, navigation claims restore and
reset, input snapshots are discarded, render history invalidates, and the next
idle interval begins. A held touch or navigation key cannot become an immediate
app action. The selected key provider must retain a release/fresh-press gate across
its Light transaction. Keep that provider instance reachable through a live grant
or dependency while the helper releases its temporary grant. The adapter's live
navigation reference suffices only if the deployment's dependency graph actually
retains the key provider.

The helper must recheck late physical input and hardware readiness; shared polling
alone cannot close that race. After every borrowed provider call, the adapter
checks Runtime liveness before the next operation. A callback that returns success
while retaining custody is terminal. No drawing, polling, release, diagnostic or
normal fini follows the retained boundary.

For a modal text editor, the existing text service checkpoint owns pausing and
resuming the same draft, layer and focus state. Policy runs only while the host is
settled and fully paused. The app's touch remains detached throughout the helper;
the host restores its own input/presenter on resume. Retention from that host also
uses the silent fence. No editor acceptance, cancellation or storage write is
part of sleep.

## Reproducible validation

Run `python3 scripts/test_foreground_idle.py --runtime <canonical-runtime>
--output <evidence-directory>`. It compiles the actual adapter and Settings model
with a canonical staged SDK, then runs all scenarios normally and with ASan/UBSan.
Hardware, Runtime transport and the Light helper are doubles. The modal test
executes the actual app-side bridge, not a real text provider plus hardware.

Coverage includes timeout boundary/wrap, disabled/invalid/unavailable settings,
confirmed timer Save versus cancel/failure/deep-row isolation, dirty nested Settings
state, repeated wake/refusal, held wake navigation/touch, mutable frame deferral,
active audio/radio/Wi-Fi and app work, service BUSY, and terminal failures during
configuration, service checkpoint, input detach, helper execution, input restore
and modal-host handoff. Tests assert no draft writes, launch, audio/radio stop,
resource cleanup or ordinary provider calls after retention. Recovery cases cover
BUSY-to-success, work/frame/service deferral, paused-only modal retry, retention,
permanent-unavailable stop, capped no-hot-loop backoff and confirmed-Save override.

Additional regressions: `scripts/test_portable_apps.py` passes with
`ASAN_OPTIONS=detect_leaks=0` in the ptrace-based runner. The independent production
text-host checkpoint suite passes 136 host/client runs normally and sanitized when
the two pre-existing `usb-queue` and `usb-continuity-overflow` cases are excluded.
Both failures reproduce unchanged at the pinned System base; this patch does not
alter that host or its tests. Do not describe the complete text suite as passing.

Not established here: physical Badge wake timing/current draw, actual key-provider
lifetime, selected display restoration, or integrated text-provider/Light hardware
round trips. Those require deployment/helper and hardware qualification. Returning
Light does not close the separate Deep/checkpoint recovery problem for arbitrary
foreground apps.
