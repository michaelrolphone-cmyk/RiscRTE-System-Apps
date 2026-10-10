# Optional shared-host frontlight tone

The selected X4 host profile is `paper_clock@0.3.24`. It builds on the separate
USB tile correction and preserves delivered 0.1.51 Home/keyboard behavior.
This source unit does not itself compose or publish a product image.

## Contract and ownership

`--frontlight-tone --frontlight-tone-sdk <directory>` opts in the existing
resident host. The SDK directory supplies `RiscDisplayOutputFrontlightV1.h`.
It is a tagged optional suffix after the exact existing display snapshot
prefix, backed by the corresponding optional frontlight suffix. Both helpers
check the complete structure size before reading suffix fields, then validate
version, tags and required callbacks. Base ABIs and the host's 16 grants remain
unchanged. Legacy tables and an explicitly unavailable suffix omit the control.
A present provider's failure is distinct from absence and fences the operation.

Quick Controls remains one host-owned overlay above Home and resident clients.
The shared controller computes slider positions and tile geometry used by both
rendering and hit testing. The no-audio profile uses the original second slider
row for COOL / WARM; Volume and Silent remain absent. A sound-capable provider
uses three compact slider rows, retaining full-size two-column controls.
Reference fonts/icons are reused, with the label font subset explicitly extended
for the new text. Ordinary presentation remains low-latency; only explicit clean
refresh and established desk-clock paths select other intents.

## Level, tone and persistence

Tone is a dimensionless 0–100 cool-to-warm ratio. It is not Kelvin and does not
promise constant luminance. Neutral 50 preserves the shipped equal per-channel
output. Each endpoint reduces only the opposite channel. The provider never
raises either channel above its existing requested brightness duty.

Brightness and frontlight OFF remain separate. Changing tone while OFF does not
illuminate either channel. Subsequent brightness/on restores the selected tone.
Foreground initialization applies tone without changing the existing cold-boot
brightness policy; sparse minute wakes never enter that foreground hook.
Confirmed light-sleep resume rechecks tone before other foreground work.

`frontlight_tone` is a one-byte namespace-1 preference. Missing means the virtual
neutral default without an implicit write. Invalid or unreadable persistence is
displayed as unconfirmed and is not offered as an editable rollback target.
Dragging previews hardware; release confirms storage by readback. Cancellation
restores the pre-gesture value. A failed save rolls back to the previous confirmed
tone only after grant release succeeds. A failed provider call or release stops
the session without subsequent storage/provider activity or guessed rollback.

## Verification

- Production Runtime/Graph with actual host/client ELFs: 29 cases in each of normal and
  ASan/UBSan builds (58 total), including repeated tone/OFF/on/brightness changes,
  cancellation, audio/no-audio, legacy/unavailable suffixes and terminal faults.
- All 44 completed frames from the preceding USB-only regression remain
  byte-identical when tone is absent.
- Focused session tests cover exact-size legacy allocations, malformed suffixes,
  ratio bounds, 768 preference shapes/values, provider refusals, readback/release
  failures and no later I/O after retention.
- The controller tests repeat ratio/cancellation/capability cases 100 times in
  normal and sanitizer builds. Actual no-audio and audio captures are inspected.
- Home gestures (300), crown/drawer (8), idle (36), shared Quick Actions (108),
  existing backlight and shared text-host regressions pass independently.
- Selected Home compiles for Xtensa and passes structural/import/export checks.

Provider-level GPIO/CPU custody, PWM bounds, optional forwarding and target ELF
checks are recorded in the matching X4 provider source unit. Physical PWM,
brightness, color temperature, panel output, sleep current and USB operation
remain unmeasured here. No network exposure, device action or publication is
performed by these tests.

Additional present-provider call-site checks run the production sparse Home and
resident idle helper with saved tone 80 and brightness 0. Cold boot applies tone
without a brightness write; interactive GPIO wake restores zero after tone;
retained timer refresh does not access tone. A simulated lost tone across light
sleep is restored before foreground continues, including repeated sleep. Getter
and setter refusal at these hooks retain silently, with no timestamp, diagnostic,
cleanup or provider I/O after uncertain custody. The tone-retained Quick Controls
and low-battery paths use the same silent fence. Native providers are strict test
doubles here; the separate 58-case Runtime/ELF matrix verifies actual host routing.

A separate tone-only Runtime matrix enables stage logging, including getter and
setter failure after successful initialization at drawer-open synchronization.
Those failures must not issue a timestamp/diagnostic or any later provider call.
