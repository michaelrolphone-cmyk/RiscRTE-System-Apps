# Nova7 main clock and launcher gesture

This app is the minimal retaining-MONO1 main clock, installed as `default.elf`.
The separately installed launcher is `springboard.elf`; ordinary apps return to
that launcher, whose explicit Back destination is `default.elf`.

    python scripts/build_paper_clock.py --navigation --alarm-client
    python scripts/build_portable_springboard.py --display-rotation 90 \
      --navigation --alarm-client --wall-time --return-app default.elf

Only a retaining monochrome portrait capability enables this UI. The shared
adapter maps native 800×480 MONO1 into logical 480×800 and consumes portrait
GT911 input. Existing Watch and Reader apps remain on their original paths.

## Clock and input

The mockup's high-contrast tick ring, large time/date, battery percentage and
Nova7 typography use real RTC/battery reads and the existing licensed font
subset. Font Awesome supplies the apps-grid glyph. Missing RTC/battery values
are shown as unavailable, never fabricated. Time format reads namespace1's
`time_format` key without writing defaults. The face does not synthesize a
Points schedule; Points remains available from the installed launcher.

Clock refresh is once on entry, then only when the RTC minute/date or validity
changes. No seconds animation or e-paper transition. The explicit `--wall-time`
launcher option enables unconverted RTC wall time; it is mutually exclusive
with `--denver`. Existing builds without a policy still show an unavailable
clock, and Watch's explicit Denver policy is preserved.

Watch main at `844dfabb8260f91a8fca3f5d7f34c23e7b7c085d` uses
`apps/clock/faces/picker.h`: after a neutral contact, movement of at least 20/240
on either axis opens the launcher in any direction. This client applies the
same per-axis threshold proportionally (40 horizontal / 67 vertical on X4).
Tap, subthreshold jitter, entry-held contact, cancelled/multitouch input do not
launch. A qualifying swipe requests `springboard.elf` while the sharp completed
clock image remains on the provider. There is no synthetic tap on entry.
Failed launch shows an error and accepts a fresh gesture after neutral input.
Confirm navigation also opens Apps. No picker, motion or Watch UI is altered.

## Ownership and tests

The clock uses generic display.output@1, input.touch.raw@1, rtc.clock@2,
board.battery@1, namespace1 read-only time_format, optional input.navigation@1
and alarm.service@1. It releases its own grants before launch and retains on
unconfirmed release. Common alarm overlay/sleep ownership remains in the
shared adapter. No provider callback or app pointer survives handoff.

`python scripts/test_paper_clock.py` runs 98 normal/sanitized cases over the real
app and adapter with native 800×480/100-byte MONO1 and portrait snapshot touch:
four directions, tap, jitter, startup contact, cancelled contact, failed-launch
retry, minute-only update, missing RTC/battery and alarm dismiss/byte-exact
restoration. Existing paper launcher, paper Settings and Watch Nova suites run
separately. Both clock/launcher target ELFs pass GCC8.4 structural and symbol
validation. This is development evidence, not hardware qualification.

Actual native-buffer capture: `docs/nova/screens/paper-clock.png`.

## Synchronous retaining-panel completion

Clock 0.1.1 and Springboard 1.6.1 use the shared adapter's bounded
`wait_present` callback for retaining MONO1 providers that do not advertise
`ASYNC_PRESENT`. Such a provider may leave a submitted frame QUEUED until a
positive wait budget initiates its physical transfer. Repeated status reads
alone cannot advance that implementation. The existing 10-second total frame
budget is preserved; a short per-poll timeout is not substituted for it.
Asynchronous MONO1 and RGB565 still use the input-serving status loop, and a
missing optional wait callback retains the existing status fallback.

The clock fixture covers immediate, wait-driven and async completion with all
14 interaction scenarios in normal and sanitized builds (84 executions), plus
seven failure modes in both builds: acquire, surface, submit, wait callback,
FAILED, SUPERSEDED and pending timeout (14 executions). Each failure emits one
bounded diagnostic and does not retry or launch a replacement application.
A never-completed provider remains responsible for refusing unsafe quiescence.

All applications linking `lib/PortableApps/src/adapter.c` must be rebuilt to
receive this correction. Source updates alone cannot fix an installed ELF.
This is software regression evidence; physical panel verification is separate.

## Controls-first 0.2.0

The clock now includes a battery status icon and charging readout. Optional
`--quick-actions` selects the static Nova7 pull-down sheet while preserving
body/rejected-top-edge swipes to Apps. Physical center Home is independent from
the product's crown-style navigation key. See [PAPER_CONTROLS.md](PAPER_CONTROLS.md)
for the explicit root target, build flags, and current X4 sleep capability gap.
