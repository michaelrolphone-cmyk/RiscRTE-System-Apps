# X4 radio startup and policy qualification

The accepted product 0.1.14 uses Wi-Fi provider source
`cf30d732271db19a74492d4753736cafeafb92bb` and Bluetooth provider source
`4966acee548e6cc3997289db206e996ed4c15cda`. These exact sources were read from
the product's `minimal/apps/sources.json`; provider-ready is not RF-ready.

The following behavior is established by the source and host tests:

| Entry/state | Wi-Fi | Bluetooth |
| --- | --- | --- |
| Normal Clock boot, missing `quick_radio` | Allowed; no scan/join requested | Off |
| Normal Clock boot, valid saved preferences | Restores allowed/off policy | Explicitly restores saved on/off |
| Corrupt or unreadable preferences | Checked shutdown; controls show error | Checked shutdown; controls show error |
| Airplane mode | Off; prior preferences retained for restoration | Off; prior preferences retained for restoration |
| Timer-only sparse Clock wake | No foreground radio acquisitions | No foreground radio acquisitions |
| Wi-Fi Settings open, saved profile present | Loads a draft; Connect remains explicit | No controller request |
| Wi-Fi Settings close or suspend | Checked disconnect and grant release | No controller request |

Wi-Fi namespace 6 credentials are independent of namespace 1 `quick_radio`.
Reset preserves valid stored policy when NVS is preserved. An empty/erased
preference store follows the missing-record row above. No new boot reconnect,
discovery, advertising, pairing or retry policy has been added.

## Reproduced defect and fix

The deployed native Wi-Fi Settings 1.1.9 build has Quick Actions but omits
`PORTABLE_QUICK_RADIOS`. Its Scan/Connect policy checks were conditioned only on
that define. Consequently, the native app could start RF after Clock selected
Wi-Fi Off or airplane mode.

The native-time Wi-Fi profile now checks the existing namespace 1 policy even
when that app does not expose radio toggles. The existing explicit quick-radio
opt-in also retains this check. Missing preferences keep the existing Wi-Fi
allowed default; corrupt/unreadable state remains blocked. Default/Watch builds
retain their existing compile-time opt-out.

`test_radio_startup.py` drives the actual native Wi-Fi controller and adapter
without `PORTABLE_QUICK_RADIOS`. Its before-fix `radio-off` scenario fails because
no policy reads occur. After the fix, Off, airplane, corrupt and I/O-failed
records prevent both Scan and Connect; missing records allow them. Each case
closes the real app grants. Existing Home, refused Home, finalization and failed
cleanup paths also pass under normal and ASan/UBSan builds, with stage logs both
enabled and disabled.

## Automatic diagnostics

The existing product stage build now distinguishes missing, persisted, corrupt
and unavailable radio preferences. It records Bluetooth requested/confirmed
state, Wi-Fi shutdown results, confirmed preference saves, restoration failures,
Wi-Fi provider admission, saved-profile presence, Scan/Connect acceptance,
link transitions, operation timeouts, cancellation and cleanup outcomes.
Lines carry the existing automatic monotonic `APP t_ms=` timestamp. No diagnostic
command or performance recorder is required. No SSID, password, scan entry,
Bluetooth packet, MAC address or IP address is added to logs.

Native SDK start/cleanup failures and controller state are diagnosed by the
companion Runtime change. The existing SDK Wi-Fi log suppression remains in
place to avoid exposing network names or credentials.

## Validation and limits

- `python scripts/test_radio_startup.py --runtime ../x4-runtime-radio-diagnostics --utilities ../watch-power-utilities`
- `ASAN_OPTIONS=detect_leaks=0 python scripts/test_portable_wifi.py`
- `python scripts/test_quick_radios.py`
- Native X4 Wi-Fi target: Xtensa ELF validation and import/export checks passed.
- Watch Wi-Fi Quick Actions opt-out ELF is byte-identical to base `1ef795c`:
  `37b8e52468cb692bcb524dfc6fb7a6dbfc8920c57a642fb5ac6d76637a21eb26`.

System version allocation belongs to the combined motion/radio integration;
the selected native Wi-Fi candidate is reserved as 1.1.10 there. This source
slice does not alter shared profile versions independently.

The intermittent physical startup report is **not reproduced or explained** by
these host tests. RF coexistence, actual controller initialization, cold flash,
reset and wake on the X4 still need device evidence. The policy bypass is a
separate verified defect; the added logs make the next physical failure
distinguishable from intentional defaults or unrequested connections.
