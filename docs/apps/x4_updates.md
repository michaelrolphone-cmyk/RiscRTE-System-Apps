# X4 OTA Update and App Store

The explicit `--product x4 --time-profile x4-native-time` build uses
`runtime.realtime@1` (read only), tagged `alarm.service@2`, portrait paper
presentation, checked radio cleanup, and the existing action-separated update
providers. HTTPS receives a closed native UTC sample, without timezone or
fixed-RTC-offset conversion. Toolbar time still uses the selected IANA zone.
Uncertain reader release retains the invocation before any further operation.

X4 policy pins `michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO`, product
`xteink-x4-pro`, and `xteink-x4-pro-{launcher,cohort}-VERSION.bin` assets.
Top-level source/product identity is mandatory. Firmware accepts complete paired
cohorts; raw runtime-only upgrades are disabled for X4. Full USB images are
informational USB-only rows and cannot become an OTA transaction.

No X4 feed is configured by default. Both apps show “No available update; X4
feed not configured.” Check completes with an empty list without opening Wi-Fi,
HTTPS, bank staging, or a UTC grant. An explicit owner-selected `--catalog-url`
may enable the exact product repository release-index URL in a later build;
this change does not create a release, tag, feed or upload.

## Reproduce

Use the existing pinned Xtensa ESP32-S3 compiler through `PLATFORMIO_CORE_DIR`,
with `PLATFORMIO_SETTING_ENABLE_TELEMETRY=No`:

```sh
python3 scripts/build_portable_updates.py --product x4 \
  --time-profile x4-native-time --native-time-runtime-repo /path/to/Runtime \
  --tagged-alarm-utilities /path/to/Utilities --alarm-client \
  --quick-actions --quick-radios --navigation --display-rotation 90 \
  --wifi-instance 15 --home-app default.elf --output-dir build/x4-updates
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_x4_updates.py
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_portable_update.py
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_update_paper.py
```

The native SDK is pinned by the shared native toolbar builder, and alarm headers
by the tagged-alarm builder. Every ELF records source hashes, explicit build
flags, imports/exports and target structural validation. Each provider reserves
bounded PSRAM BSS and retains the 2 KiB per-function stack gate. Leak detection
is disabled only because the executor uses ptrace; address and undefined
behavior checks remain enabled.

Product grants: display 3, touch 4, navigation 6, battery 7, Wi-Fi 15, HCI 16,
KV namespaces 6 and 1, read-only realtime 0, tagged alarm 0, plus exactly one
of `software.update.firmware@1` or `software.update.apps@1` at 0. The two providers
alone receive typed `platform.http-client@1`, `platform.bank-store@1`, and
`platform.clock@1`. Applications receive no raw bank/HTTP capability.

App Store updates installed authorized applications with unchanged authority.
Adding applications, providers or grants uses the fully admitted product cohort.
Native install/update hashing and ELF/graph admission remain mandatory; committed
boot and retained timer wake do not rehash firmware or the boot store.

## Software evidence and limits

X4 policy mutations and 46 cohort failure scenarios run normally and under
ASan/UBSan. Actual X4 paper controllers cover UTC/no-feed behavior, tagged alarms,
Home and retained cleanup. Shared Watch tests cover catalog/service/controller
and paper cancellation/Back/Home behavior. The default Watch app manifests and
ELFs compare byte-identically against the starting source.

Generic Runtime ABI2 tests cover full cohort image admission, wrong product,
staging interruption, rollback, cancellation, uncertain activation and retained
cleanup. No Runtime implementation change is required. No physical X4 network,
OTA flash/power-loss, radio or e-paper behavior is qualified by these tests.
