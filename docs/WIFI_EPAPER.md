# Capability-selected Nova7 paper Wi-Fi

Based on System Apps `2aa0cf63346e507525af884bbfbf6b69313442c4`.
The portable Wi-Fi controller now selects a static paper view using the existing
PaperPresentation geometry/retained-image/MONO1 capability gate. No board name,
radio backend, product grant, or runtime API is added. Legacy Reader and both
240×240 Watch views retain their existing behavior. The app version remains
1.1.3; these are development artifacts, not a release or product bundle.

The actual `docs/nova/screens/epaper-settings.png` pixels were inspected before
implementation. The Wi-Fi view reuses its shared Orbitron/Rajdhani raster fonts,
white one-bit surface, 3px rules, inverse selection and licensed FontAwesome
Wi-Fi/chevron masks. Rows page with selection; larger buttons and all 95 ASCII
characters use the existing three-page keyboard controller. SSID/key case is
preserved, passwords remain masked, and editor Done changes only a RAM draft.
Scan/connect/cancel/retry/disconnect, explicit Save/Forget, privacy information,
timeouts, and checked teardown retain their existing controller contracts.
No render, drag, page change or Home action saves credentials.

Physical Home closes the Wi-Fi session before requesting `default.elf`, including
inside a nested editor, scan or QuickActions. Cleanup refusal keeps the controller
available for a deliberate retry. A completed Home handoff cannot queue the local
Back target or redraw the old app. Local Back cancels the current editor/scan
before root Back closes the session and requests `springboard.elf`.

QuickActions uses the existing capability truth rules. Without `--quick-radios`,
the paper radio tiles are unavailable; the Wi-Fi app's own explicitly granted
scan/connect controls remain usable. The sheet does not infer radio settings
from a connection. Frontlight/Torch need advertised brightness support; sound and
Silent need sound output support. Missing/unreadable preferences remain
unavailable. No X4 sleep hook, background reconnect or product selection is added.

## Reproducible builds and owner-selected grants

```
python3 scripts/build_portable_wifi.py --nova-ui --wifi-instance 15 \
  --display-rotation 90 --navigation --alarm-client --quick-actions --wall-time \
  --home-app default.elf --return-app springboard.elf --output-dir dist/wifi-paper
python3 scripts/build_portable_wifi.py --nova-ui --wifi-instance 15 \
  --navigation --alarm-client --full-frames --return-app springboard.elf \
  --output-dir dist/wifi-watch
```

The paper build declares `display.output@1`, `input.touch.raw@1`, `net.wifi@1`,
`alarm.service@1`, `storage.key-value@1`, `input.navigation@1`, `rtc.clock@2`,
and `board.battery@1`. Wi-Fi explicitly requests instance **15**; credential
storage explicitly requests namespace **6**. QuickActions additionally requests
namespace **1** with read/write access. Display, touch, navigation, alarm, RTC
and battery must resolve uniquely within the product owner's authorized grants.
The Watch command omits QuickActions/RTC requirements and retains namespace 6.
The generic Wi-Fi provider already exists; this change does not select it in a
product. The integration owner supplies the actual manifests/grants and catalog.

`--quick-radios` remains a separate explicit opt-in and additionally needs the
existing Wi-Fi/Bluetooth radio-control bindings (15/16) and namespace 1. It is
not selected in the paper command above. Duplicate capability declarations are
collapsed before emitting a Runtime manifest.

Each output contains ELF, canonical Runtime manifest, license notices and
`wifi_settings-build-record.json`: source hashes, source commit/dirty status,
full compiler command, defines, requirements and explicit binding notes. Rich
profile/build information stays in the receipt, never in the Runtime manifest.
The target validator checks ELF layout and exact public import/export inventory.

## Verification

`python3 scripts/test_portable_wifi.py` executes 48 production controller/adapter
scenarios × two Watch presentations × two touch rotations × plain/sanitized C,
plus credential interruption and saved-profile fault suites.
`python3 scripts/test_wifi_paper.py` executes 354 paper cases across portrait,
native rotation and 400×600, plain and sanitized C. It checks the same controller
scenarios plus direct Home, nested Back, cleanup/launch refusal, keyboard cells
and gutters, list paging, drag cancellation, exact modal restoration and
truthful capability controls. Portrait/native rendered pixels must match.
All providers are fake; no host network, real credentials or device is used.
The default sanitizers are ASan+UBSan; `WIFI_SANITIZERS=undefined` permits an
explicit UBSan-only run on hosts unable to initialize ASan.

`python3 scripts/test_wifi_manifest.py --runtime /read-only/RiscRTE \
 dist/wifi-paper/wifi_settings.json dist/wifi-watch/wifi_settings.json`
links the unmodified production Runtime parser and calls metadata-only
`Runtime::prepare`. Synthetic provider/board metadata permits declaration/grant
validation without executing any module. Actual manifests must pass; injecting
unsupported `profile` must fail. This fixture is not product integration proof.
The Runtime commit and source hashes are recorded under `build/wifi-manifest`.

The branch's Wi-Fi workflow runs Linux ASan/UBSan and builds both target ELFs.
Local plain-C/UBSan and 52 repository unit tests pass. Local macOS ASan stalls
while reserving its shadow address range even in a trivial smoke program;
Linux CI supplies the ASan result separately. Hardware RF, panel latency and
physical button behavior remain unqualified.

Production fixture captures:
[Root](nova/screens/wifi-paper.png),
[scan results](nova/screens/wifi-paper-scan.png),
[keyboard](nova/screens/wifi-paper-keyboard.png),
[visual-only QuickActions](nova/screens/wifi-paper-quick.png).
