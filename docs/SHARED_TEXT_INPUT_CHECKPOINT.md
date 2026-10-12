# Optional shared text-input checkpoint

The text host 0.1.7 adds a size/tag-checked suffix after the complete existing
long-text API. Old plain, masked, extended and long request/result shapes,
flags, offsets and callback signatures are unchanged. Existing clients never
call the new methods and retain their original behavior.

The suffix lets a foreground application preserve resident low-battery and
idle-Light policy while an uncommitted draft stays open. No Runtime extension,
new capability grant, product-specific keyboard, or font change is needed.
The scene presenter adds an opt-in same-session input-continuity feature.
The application owns policy; the text host owns every editor field and key.

## Contract

1. Open the ordinary shared text session, then call checkpoint_enable before
   its first poll. It requires the existing scene lifecycle and checkpoint
   extensions plus POLICY_INPUT_CONTINUITY. Missing support returns UNAVAILABLE; callers must close cleanly
   and surface a retry/error rather than silently deferring power policy.
2. Continue ordinary text polling. checkpoint_snapshot returns separate
   PRESENTING, INPUT_BUSY, ACTIVITY and PAUSED flags. Activity is latched until
   successfully copied. Scene touch/navigation and optional USB activity/held
   keys are included. Presentation and logical input remain independent.
3. Feed those flags through PortableTextInputResidentCheckpoint.h at the existing
   resident POLL cadence and application-specific inhibit rules. A pending frame
   or held input defers policy without blocking text input. Only an exact OK POLL
   reply of POLICY_REQUEST | INPUT_CONTINUITY grants a same-session promise.
4. Before POLICY, pause_mode chooses continuity only for that promise. Otherwise
   pause_for_focus_change releases all touch/USB subscriptions and navigation,
   including on old native owners. Both paths re-poll/recheck input; activity,
   a changed draft or Save/Cancel/Home returns BUSY. After BUSY, clear the stale
   request and report activity through a fresh guarded POLL. AGAIN retains custody.
5. Continuity pause returns mutable surfaces and completes submitted display work,
   retaining bounded passive touch/USB queues, navigation and copied editor state.
   No input is serviced while paused. Full pause instead relinquishes every input
   owner, permitting the selected GT911 touch-power no-subscriber gate to admit
   actual Light sleep. Never release a live paused text grant/session.
6. The native promise is latched for one POLICY dispatch and prohibits sleep/focus
   transfer even if the idle deadline or timer preference changes meanwhile.
   Ordinary resume preserves queued edges. Full-detach resume neutralizes inherited
   input as before, preserving draft/layer/caps. Both paths request visual REDRAW.
   A promised-continuity POLICY unexpectedly returning REDRAW is a contract failure
   and retains custody. The post-POLICY REDRAW bit alone never authorizes continuity.
7. Home/EXIT closes the paused editor without resuming it or committing a
   draft. Close is valid while pause is pending or complete. Acceptance still
   requires the ordinary close/release/application-resume sequence before
   any app persistence. Every retained/lost-context result fences later calls.

PortableTextInputCheckpointClient.h is an optional companion to the unchanged
PortableTextInputClient.h. It validates suffixes, snapshots and transitions and
provides fail-closed liveness handling. The public RiscTextEntryV1.h copies in
sdk/app and lib/PortableApps/include are identical.

## Native-owner compatibility

PortableResidentPolicyInput.h defines INPUT_CONTINUITY as the existing
CONFIGURATION_CHANGED bit only on an OK POLL reply paired with POLICY_REQUEST.
Selected old native owners return only POLICY_REQUEST or zero there; existing
scene/native client POLL helpers ignore the extra bit and continue fully detaching.
The bit retains its original meaning on POLICY and other replies. The Runtime
already validates/forwards it, so no wire layout or native firmware change is needed.
New clients fall back to full detach with an old owner or any absent/ambiguous
promise. The new native owner consumes the promise once, clears it on inhibited
POLL/child launch, and cannot enter sleep on the promised dispatch.

Only selected X4 Home is rebuilt. Its exact historical source/flag/font closure is
reproduced before overlaying resident_policy.inc, resident_shell.inc and the new
header. Watch's selected boot has no resident_shell and its native Clock is unchanged.
Independent boot/wake and deferred-radio fixes remain in that frozen Home profile.

The existing native REDRAW reply identifies an attempted idle-sleep branch, also
covering early refusal and input-cancelled settling; it is not proof of hardware
sleep or focus acquisition. Full-detach paths preserve predecessor neutralization
for all those outcomes. A finer confirmed-focus signal remains follow-up work.

## Motivation and qualification

The Lists scene editor previously performed resident POLL/POLICY while settled.
A first modal migration bypassed that loop and could indefinitely defer idle
Light or low-battery policy. Merely closing and reopening the old shared API
would lose keyboard layer/caps and drop events during the detached interval.
The selected Watch navigation driver's foreground() also resets its held-button
state, so routine continuity retains that claim. A real sleep owner may replace
it, and FOCUS_CHANGED resumes with the original neutralization. Legacy scene
clients do not opt in and keep their original release/neutralize semantics.

Queues remain bounded by the existing transports. Overrun/GAP/ambiguous snapshots
follow the existing fail-closed neutralization rather than inventing missing
keystrokes. The guarantee covers ordered events delivered within those bounds;
it does not establish physical sampling latency or recover pre-subscription events.
The selected Watch FT6336U driver samples current contact state in poll, and the
X4 GT911 driver reads and acknowledges a ready report in poll. Neither installs
asynchronous physical edge capture; snapshot returns cached state. A physical
press/release completed wholly between provider polls cannot be reconstructed
by this software checkpoint. Watch buttons also use polled debounce. These
transport limits are recorded separately from the delivered-edge tests.

Tests cover opt-in/ABI compatibility, short providers, activity and queue races,
held touch/USB input, copied-state/layer/caps preservation, pending pause, paused
close, missing support, invalid or contradictory snapshots, and faults/liveness
loss at every callback boundary. Application tests cover real scene/text hosts,
Home during policy, repeated handoffs, saving only after clean closure, and
original non-keyboard render/storage parity. Real Runtime/provider-graph tests
cover repeated pause/resume and paused close across eager/demand activation and
actual app unloads, alongside all existing text regression modes.

Run from a clean source tree:

```sh
python scripts/test_text_input_review_checkpoint.py --runtime ../Runtime \
  --output ../evidence/checkpoint --sanitize
python scripts/test_text_input_handoff_continuity.py --runtime ../Runtime \
  --output ../evidence/continuity --sanitize
python scripts/test_text_input_host.py --runtime ../Runtime \
  --output ../evidence/legacy --sanitize --decoupled-attention
python scripts/test_text_input_runtime.py --runtime ../Runtime \
  --output ../evidence/runtime --sanitize --scene-compact
```

The new checkpoint suite respects platform-default LeakSanitizer in CI. Local
ptrace runs use the invoking ASAN_OPTIONS=detect_leaks=0; pre-existing legacy
host/runtime runners retain their documented local leak-detection override.
Host tests and target validation do not establish physical-device sleep, battery,
USB or display timing. No hardware was flashed or binary delivered by this work.
