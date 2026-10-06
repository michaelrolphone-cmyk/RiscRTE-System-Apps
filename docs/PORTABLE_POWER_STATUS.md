# Read-only power status

The opt-in `PORTABLE_POWER_STATUS` adapter exposes an app-local
`portable_power_read()` helper to the independently built Battery app. It copies
one existing `board.battery@1` sample. No Runtime mechanism, import, table size,
capability grant, storage authority or canonical SDK header changes.

`PortablePowerStatus.h` specifies optional bits in the existing four-byte sample.
Bit2 is an explicit extended-status-valid marker. Bits3..7 mean usable external
input, charger enabled, charger complete, battery present and PMIC die thermal
regulation. Interpret those bits only with bit2 set. Old providers with just
charging and unavailable-SOC flags remain supported. No flag conveys battery
current, cell temperature, charging speed, health or time to full.

The helper resets output to unknown on missing provider, read failure, inactive
grant or failed invocation. A provider that partially fills its buffer and then
returns failure cannot leak a stale success. Successful reads preserve every
byte, including a genuine 0% and unknown/out-of-range estimates; presentation
makes the validity decision. Sampling has no register writes or IRQ ACKs.

The definition is shared byte-for-byte with the Watch driver companion header;
it does not modify the pinned `RiscBatteryGaugeV1.h`. Compile Battery with
`PORTABLE_NOVA_UI`, `PORTABLE_POWER_STATUS` and
`BATTERY_RETURN_APP="springboard.elf"`. Do not also select `PORTABLE_RETURN_APP`:
the app owns Back from its nested Details pages. Existing caller builds are
unchanged when the feature macro is absent.

Verification:

- `python scripts/test_power_status.py`: normal and ASan/UBSan; 256 flag/percent
  combinations, partial-provider failure, null and inactive/failed guards
- `python scripts/test_portable_apps.py --utilities <utilities>`: existing
  battery, Settings, time, sleep, retained-error and data-preservation fixtures
- `python scripts/test_nova_ui.py`: existing shared renderer and retained modal
  regressions
- Utilities' `scripts/test_battery_power.py --system-apps <this checkout>`:
  real production app and adapter, framebuffer stride guards, status transitions,
  failed-read recovery, nested navigation, launch refusal and grant cleanup

Physical charge current, thermal behavior, sleep current and display appearance
on a watch remain unqualified.
