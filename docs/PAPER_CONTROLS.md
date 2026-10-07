# Nova7 paper Home and quick controls

Clock 0.2.0 and Springboard 1.7.0 add the controls-first retaining-MONO1 client.
Selection uses the existing display capability geometry/format/retention check;
no board name, GPIO, panel transport, or radio enable is added to shared UI.
The supplied Nova7 paper mockup and actual native frame captures were inspected.
Watch's 240×240 RGB565 controller/session/rendering and legacy T5 remain separate.

## Physical controls

`PortableTouch` independently consumes primary touch-button events and the
snapshot button bit, with fresh neutral gating after startup, reset, polling
failure, queue gap, or overflow. Held Home does not repeat and never becomes a
coordinate tap. X4's center Home already comes from GT911's primary touch button;
GPIO3 is a different physical key. The provider need not change for center Home.

`--home-app default.elf` selects `PORTABLE_HOME_APP` as a direct root destination.
It bypasses an app's local Back ownership and its ordinary `--return-app` path.
Center Home also leaves QuickActions for that root. The clock omits this option,
so center Home keeps the already-visible clock.

A navigation `RISC_NAV_HOME` edge is handled separately as the crown-style key.
It closes QuickActions first; otherwise it goes to the explicit Home target.
Ordinary `RISC_NAV_BACK`, Confirm, and page keys retain their existing semantics.
The X4 product owns physical-key timing/mapping; generic code does not reinterpret
its legacy CONFIRM source or physical-page-pair trait.

When the clock is built with `--navigation`, a crown-style navigation Home press
on the face displays `SLEEP NOT AVAILABLE` and emits one diagnostic. This is
intentional: no X4 app sleep or wake capability is currently selected. Watch's
completed crown short press uses its Watch-local PMU/display preparation,
resume, and CPU/GPIO wake route. These are not portable X4 APIs, and this client
does not emulate sleep, turn off power, or claim crown sleep/wake parity.

## Static paper sheet

A downward drag beginning in the top 72 of 800 logical pixels opens a full sheet
when downward movement exceeds 24 pixels. The sheet has no slide/fade animation.
Rejected horizontal/upward gestures replay their initial down and current sample,
so the clock's existing general swipe-to-Apps gesture is preserved. A top-edge tap
is still an ordinary tap. Gesture coordinates normalize from capability-selected
portrait dimensions; X4 still uses native 800×480 MONO1 with logical 480×800 input.

The two-column layout uses the existing licensed QuickActions icons and paper
Orbitron/Rajdhani text. Frontlight, notification volume, Silent, DND, Airplane,
Wi-Fi, Bluetooth, and Torch use the same session/preference semantics as Watch.
No controls imply new grants:

- Frontlight and Torch require the advertised display brightness flag and callback.
- Without explicit `--quick-radios`, all radio tiles say `UNAVAILABLE`; no radio
  capability is acquired and no missing Wi-Fi app is launched.
- Volume and Silent share the existing notification volume preference; DND remains
  independent. Unknown or unreadable preference state is unavailable.
- No Low Power or Clean Refresh policy is invented from the mockup's extra tiles.

Swipe up, tap the close row, or use the crown-style key to dismiss. Center Home
returns to the root instead. Modal closure and alarm preemption restore the exact
completed native foreground buffer. Storage never runs while a frame is leased.
The modal stores native MONO1 bytes (48 KB on X4), not a 115 KB RGB565 assumption.
Only changes to visible state/time/battery render another frame. An idle sheet
has no animation or periodic identical presentation. Torch restoration and alarm
priority retain the shared tested lifecycle.

Clock and launcher display a real battery icon/percentage and charging status;
missing/invalid telemetry is explicitly unavailable. Battery changes can update
the static face without waiting for the minute to change.

## Build and exact grants

```
python scripts/build_paper_clock.py --navigation --alarm-client --quick-actions \
  --output-dir dist/paper-controls/default
python scripts/build_portable_springboard.py --display-rotation 90 --navigation \
  --alarm-client --quick-actions --wall-time --return-app default.elf \
  --home-app default.elf --catalog examples/x4-default-catalog.json \
  --output-dir dist/paper-controls/springboard
```

Both require existing `display.output@1`, `input.touch.raw@1`, `input.navigation@1`,
`alarm.service@1`, `rtc.clock@2`, `board.battery@1` (X4 selected instance 7), and
`storage.key-value@1` namespace/instance 1 with read/write access. The builder
records requirements; it does not edit product grants. Settings 1.3.2 and File Browser 1.5.2 also expose `--home-app`,
`--quick-actions`, and `--wall-time`. Their current ordinary return remains
`springboard.elf`; Home targets `default.elf`. The legacy File Browser
`--quick-controls` keeps its explicit Watch radio/Denver selection. Other app builders can
select `PORTABLE_HOME_APP="default.elf"` while preserving their existing Back
destination, then rebuild their shared adapter client. The clock instead selects
`PORTABLE_CROWN_SLEEP_UNAVAILABLE` until a real sleep hook is provided.

`--quick-radios` is a separate opt-in requiring declared `net.wifi@1` instance 15
and `bluetooth.hci@1` instance 16. It is not selected for the current X4 build.
QuickActions links `quick_actions.c`, `quick_render.c`, and `quick_session.c`;
radio selection additionally links `quick_radios.c`. All license notices are
copied into the output directory. No extra app is added to the X4 catalog.

## Verification

`python scripts/test_paper_quick_actions.py` runs 108 normal/sanitized cases with
the real clock/launcher and adapter, at portrait and native-rotated MONO1 geometry.
It checks edge-vs-body gestures, tap/rejected gesture replay, modal close, exact
foreground restoration, Silent/mute restoration, DND, frontlight commit,
unavailable radios/Torch, enabled Torch cleanup, storage errors, alarms, held Home,
direct Home vs ordinary Back, and crown dismissal/unsupported sleep reporting.
Real nested Settings editor and File Browser preview tests additionally verify
that Home leaves drafts unsaved, closes storage before launch, and never queues
a competing ordinary Back destination or redraws the old app.
Two additional normal/sanitized input fixtures exercise event/snapshot Home edges,
startup/reset, faults, queue gaps/overflow, non-primary buttons, and no fake taps.

Existing Watch QuickActions, QuickRadios, paper clock, paper launcher, paper Settings,
Watch Springboard, portable-app, and repository unit suites remain regression gates.
Target clock/launcher builds undergo ELF structural/import/export validation. This
is host and target-build evidence; actual button feel, panel latency and physical
sleep behavior require hardware qualification.
