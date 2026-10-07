# Automatic low-battery Watch policy

The future Watch profile defines `PORTABLE_LOW_BATTERY` for Clock and every
portable foreground application. It automatically applies these existing
namespace-1 preferences when a valid battery sample enters the range below 10%:

- Sleep Timer: 20 seconds of inactivity
- Brightness: 15%
- Deep Sleep Timer: one minute asleep before the Hybrid transition
- Wi-Fi and Bluetooth: off through the existing radio controls

There is no user enable switch. The compile-time profile boundary preserves
historical/frozen deliveries. Runtime remains a headless mechanism and stores
opaque records; neither its ABI nor the radio ABI changes.

## One edge, then manual control

`battery_edge` is a checked, versioned four-byte record. A first valid low sample
with no record counts as entering low battery. The entry is persisted **before**
any setting is written, preventing a reboot, app switch, or partial failure from
replaying the entry. While the battery remains below 10%, settings are not
rewritten and hardware changes are not continually reapplied. Manual controls,
including edits after a partial application, remain authoritative.

A valid sample at 10% or higher rearms the next entry without restoring prior
settings. Charging and external power alone do not rearm, undo, or suppress an
entry. Unknown percentage, failed reads, missing gauge profiles, and a confirmed
absent battery are ignored. Valid readings from older providers without the
optional power-status suffix remain usable.

The foreground samples at most once per five seconds, at a settled display and
service boundary. It cancels any pre-entry pending slider/torch action and drains
app-owned radio sessions before refreshing physical controls. Subsequent new
user gestures remain valid overrides. This is foreground observation, not a new
background service or a promised battery interrupt.

## Settings and failures

Settings 1.3.1 appends Sleep Timer and Deep Sleep Timer without changing existing
row identities. Save/Cancel and unconfirmed-write handling use the same records
as the automatic policy. Clock and the shared sleep adapter consume the same
confirmed timer values. Existing Clock Light/Deep/Hybrid selection is not
changed; Deep Sleep Timer governs its Hybrid progression and shared app Hybrid
sleep. Timer defaults remain 60 seconds idle and five minutes asleep.

`brightness` and `quick_radio` retain their original encodings. The two new
five-byte timer records store bounded seconds: idle 5–3600, deep 60–3600. The
brightness control displays 15% exactly; the existing manual slider's ten-point
steps continue to choose subsequent overrides.

Readback confirms every write, including ambiguous I/O returns. An unconfirmed
edge write prevents all preference changes; other write failures are reported as
partial application and are not retried while still low. An inaccessible or
malformed edge record is not overwritten. There is no erase/format/recovery
write. A failed capability release or uncertain radio drain stops normal
progress rather than claiming hardware is off.

The multi-key update is intentionally not a transaction. If power is lost after
the edge was committed, some preferences may remain at their prior values. The
next boot respects the consumed edge. This favors preserving manual overrides
over silently replaying a partially applied policy.

## Verification

`python scripts/test_low_battery.py` executes production policy, Quick Controls,
shared adapter and Settings code in normal and ASan/UBSan builds, including both
touch rotations. It covers repeated samples, exact 10% boundary, first low boot,
reboot/app-switch persistence, charging, unknown/absent battery, corrupt records,
write/readback/acquire/release faults, partial updates, radio/brightness faults,
actual 20-second idle entry, modal cancellation, owned-radio drain failure, and
manual timer Save/Cancel/retry. The target Settings profile is built with:

```
python scripts/build_portable_settings.py --low-battery --nova-ui --full-frames
```

Watch adds production Clock and Light-to-Deep timer regressions. These checks do
not establish physical current consumption or hardware qualification.
