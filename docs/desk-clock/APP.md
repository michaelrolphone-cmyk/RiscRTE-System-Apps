# Opt-in retained X4 desk Clock

This is the development-only `paper_clock` **0.3.0** build selected with
`--desk-clock`. It does not change the normal Clock, Watch UI, shipped X4 0.1.6,
or a firmware/source lock. Product integration, a complete image and physical
qualification remain separate. No BIN or device-flash claim is made here.

## Build and authority

Use `scripts/build_paper_clock.py` with all of:

```
--navigation --alarm-client --desk-clock
--local-sleep-source /path/to/X4/minimal/apps/portable_sleep.c
--sleep-capability x4.power
--sleep-sdk /path/to/Reader/sdk/driver
--retained-wake-sdk /path/to/Runtime/sdk/app
--display-rotation 90
```

`--quick-actions --quick-radios` is optional. The build stages only the selected
canonical prefixes/extensions together; it does not mix stale copied prefixes
with new suffix declarations. Other existing deployment ABI headers stay intact.
The default build remains byte-identical; the checked controls Clock ELF SHA-256
is `608fa1fa5b7437b3e6b1d87ad0705e3ae49c5d38ff756a89988ffd31c8a62466`.

The manifest has exactly twelve distinct requirements:

- display.output@1, input.touch.raw@1, rtc.clock@2, board.battery@1
- storage.key-value@1 namespace 1, input.navigation@1, alarm.service@1
- x4.power@1 instance 17, runtime.retained-wake@1 instance 0
- storage.volume@1, net.wifi@1 instance 15, bluetooth.hci@1 instance 16

The deployment must grant the matching instances. Provider graph admission and
typed suffix availability remain required; old interfaces refuse safely. The
X4 power provider must include the separately integrated board-keepalive hold
and rollback. The Clock does not acquire raw GPIO or an extra board-power grant.
Private desk/adapter functions are hidden local links, never native exports.
The ELF exports remain only app_main, app_module_init and app_module_fini.

Pinned composition inputs used by CI are Reader SDK
`aac8c06d3221139084acd0cfc64f7b0ba194a97a`, Runtime
`3c39aa7ac50ecd7f6da0296a62a2d7af066f82d1`, and X4 app client
`c8a4f951abccd2e88660468958621612c82083c3`. This Clock is based on the
unchanged Settings helper source `386bab981a7a6fee9f5638046509a978d9603f8e`.
The output includes both Noto licenses and `licenses/desk_clock/SOURCES.json`.

## Scene and boot behavior

Manual crown entry uses the saved Light/Deep preference (mask 3, fallback Light).
The existing Light route is retained. Stale Hybrid never becomes deep entry and
no preference read repairs storage. Face selection uses the finalized Settings
helper, with Segments fallback and all six stable face IDs.

`app_module_init` may open touch but does not draw or read retained state.
`app_main` reads the Runtime-owned classified record before the normal Clock.
Only a valid timer record can enter the resume path. GPIO, deep-other, reset,
cold, malformed and foreign records start the normal Clock; held input stays
neutral-gated. An early timer-resume refusal discards reuse authority before
drawing normal UI, so a later manual attempt cannot seed a stale desk image.

The retained scene is deterministic: face, displayed minute, date, 12/24-hour
format, AM/PM and power-wake text. Battery readings, transient notices and current
preferences never contaminate old-image reconstruction. This profile uses the
explicit, unconverted RTC wall-time policy and English text. RTC UTC mode,
legacy chip-layout variant, UTC-versus-local interpretation reference epoch,
and UI flip are zero/unknown or inactive. The typed clock does not expose the
Reader's legacy RTC interpretation/timezone metadata. No
Denver conversion or process-global timezone is adopted. The English date
format here differs from Reader's ISO YYYY-MM-DD date; localized wake/unset
captions remain a later parity slice. Changing any stored
rendering configuration forces a full refresh.

For differential timer updates the app rebuilds every old pixel, seeds both the
panel and adapter MONO1 cache, then repaints the **same frame lease**. Releasing
that lease would invalidate the panel's seed. A missing history/cache takes a
full refresh; an unclassified failed seed retains custody without cleanup.
Only checked present completion advances the visible-minute checkpoint. Submit
acceptance, failed/unknown status and incomplete waits cannot advance it. Any
failed or unconfirmed desk present conservatively preserves the invocation;
there is no normal frame/grant cleanup after an ambiguous display failure.
First entry and every thirtieth committed cycle are full refreshes.

## Checked transaction

1. The shared adapter finishes the frame, closes touch, clears input state and
   invokes the deployment hook. Power neutrality is checked before work.
2. Paint and confirm the current scene. Observe a bounded RTC second edge while
   providers are awake. If the minute changed, repaint within the same bound.
3. Stage the confirmed proposed record in Runtime RAM **before** peripheral
   holds. Native terminal entry is the only operation that commits it to RTC.
4. The X4 client proves Wi-Fi and Bluetooth off without rewriting preferences or
   requesting credential access; it reconciles alarms, darkens the frontlight,
   and prepares touch, panel and the separate zero-handle storage sleep API.
5. Recompute the deadline after preparation from the existing edge anchor and
   monotonic elapsed time, with one immediate RTC consistency read. There is no
   app yield/provider polling, KV access, Runtime stage/clear or grant release
   while the prepared GPIO holds are live.
6. If preparation crossed a minute, fully restore, clear the staged proposal,
   repaint, stage a replacement, and prepare again. At most **three confirmed
   frame attempts** occur in one cycle; exhaustion returns to normal UI.
7. Enter using the shorter clock/alarm bound, keeping the retained-wake grant
   alive. Successful entry never returns. Ordinary refusal restores storage,
   panel, touch and brightness before clearing the pending record or releasing
   grants. An unconfirmed rollback or retained result stops all ordinary I/O and
   preserves resources. Missing SD media is reported as unavailable, never as a
   successful remount.

The normal Clock shows a one-shot refusal notice. When radio shutdown occurred,
it explicitly says `DESK CLOCK STOPPED; RADIOS OFF`. Input queues, saved contact
snapshots, navigation and app gesture state are reset; simultaneous pre-sleep
finger movement cannot become a launcher swipe on return. Quick radio policy
application is delayed until normal foreground is needed, so timer boots do not
enable saved Bluetooth only to disable it again. Light/default paths retain
their existing behavior.

## Time resolution and remaining limits

rtc.clock@2 provides whole seconds. The app polls an adjacent second transition
every 10 ms, accepts only a bracket of at most 100 ms, and stops within 1,250 ms
if the clock stalls, jumps or reads too slowly. The upper endpoint is a
conservatively late epoch anchor, not measured subsecond RTC accuracy. After
preparation it advances that anchor with millisecond uptime; one immediate RTC
read must agree within the adjacent-second uncertainty and finish within 100 ms.
A re-anchored adjacent second can widen the conservative phase uncertainty to
200 ms. Preparation older than 65 seconds is rejected. Staging/entry-call elapsed
time is subtracted from the proposed duration.

The final relative `deep_sleep_for` API does not report timer-arm/entry latency
or accept an absolute deadline. Thus these are bounded sampling/deadline rules,
not a claim of subsecond physical wake accuracy. No gettimeofday, native SDK time
symbol, new Runtime export or fabricated microsecond read is imported.

Runtime currently starts the full admitted provider graph on every boot.
This implementation does not claim the Reader's minimal timer-boot efficiency.
Electrical GPIO/rail retention, touch wake behavior, radio current, actual RTC
phase/entry latency, display waveforms, and repeated on-device minute/overnight
operation remain hardware qualification work. A tap wholly inside an
indivisible synchronous panel/provider call cannot be latched by the present
level-only power API; held/newly observed power presses abort before entry.

## Verification

- `scripts/test_paper_desk_clock.py --sdk /path/to/Reader/sdk/driver` runs the
  real Clock and adapter, normal and ASan/UBSan: initial/manual/timer/GPIO paths,
  exact complete-scene reconstruction, all six faces across 62 fresh-process
  cycles each, two full-refresh periods, date rollover, 12/24-hour scenes,
  retained cancellation, failed seed/presentation, clock failure/jump/slow-read,
  preparation rollover, catch-up exhaustion, stale timer authority after early
  refusal, and simultaneous power/touch refusal without replay.
- X4's `run_desk_clock_sleep_test.sh` exercises the actual typed client failure
  matrix. `run_desk_clock_typed_app_test.sh` links it with the real Clock/adapter
  in both QuickActions/radio configurations and enforces the prepared-state
  gates and same seeded frame lease.
- `scripts/test_desk_clock_runtime_gate.py --runtime /path/to/Runtime` uses
  real Runtime/CpuPort/dlopen fixtures to prove the storage gate
  against actual GPIO holds and provider locks, rather than a permissive API
  stub. Runtime's existing retained-wake suite independently checks ownership,
  envelope integrity, fresh boot causes, one-shot reads and terminal commit.
- Existing Clock: 98 normal/sanitized cases. Paper controls: 124 cases. Existing
  unit suite: 52 tests. Quick-radio policy regressions also pass; the default
  radio-enabled Clock ELF remains byte-identical at SHA-256
  `e75843904da745dc410b99a3fc5326b89efdb9f53ead2c8cc5c485052f6a7f4b`. Xtensa GCC8.4 full build, bounded public imports/exports,
  and the actual loader's structural validation pass.
- `app-evidence/` contains lossless pixels, source/build hashes and compressed
  logs. Host composition and structural validation are not hardware execution.
