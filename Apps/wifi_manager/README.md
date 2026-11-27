# Wi-Fi Manager 2.0.0 — scene migration

Implements the supplied **Wi-Fi Manager — NOVA-7 e-ink** design as an X4 native ELF.
The app supplies semantic documents, values and network intents. It contains no
rasterizer, font bitmap, screen coordinates, hit map, or keyboard implementation.
`ui.scene` 0.5.0 supplies network rows, signal bars, connection summary cards,
connection steps, the existing dialogs/buttons/scrolling, and two-choice bottom
navigation. Signal history uses the existing copied telemetry plot API.

Text entry is **the existing `ui.text-input` service**, not a Wi-Fi keyboard.
Its existing negotiated masked request is used for passwords. The shared service
now also exposes Show/Hide to masked callers; the ordinary text mode is unchanged.
Wi-Fi closes its scene before opening the keyboard and restores the scene only
after the text session closes. Pending passwords are neither logged nor rendered
by the app and transient copies are cleared after acceptance/cancellation/saving.
The shared keyboard keeps its established layout instead of copying the HTML keys.

## Working flows

* WI-FI / SAVED navigation, radio enable/disable using the existing shared radio
  policy, explicit scan with progress, signal/security rows and network details.
* Actual asynchronous join/cancel, existing password keyboard, open networks,
  hidden-SSID entry, saved-network lookup, reconnect and confirmed forget.
* Existing credentials/profile codec and namespace are reused. A network is
  saved only after a reported successful connection. Busy storage is deferred;
  failed writes and missing services do not report success.
* Connection summary with actual RSSI, address and connection duration; detail
  view includes channel/frequency, gateway, netmask and a 60-second RSSI graph.
* Back/Home, keyboard cancellation, delayed radio cleanup and shared-controls
  handoff use the native resident lifecycle. Returning uncertainty or losing the
  invocation does not release retained resources or issue more provider I/O.
* Static screens are not repeatedly submitted. Active status redraws are paced
  after the previous presentation settles, not from an unrelated timer.

## Backend limitations, not simulated features

This is a UI/capability migration over the deployed `net.wifi@1` asynchronous
provider. It does not add a different radio driver or replace the native firmware.
The HTML contains simulated capabilities the deployed API cannot supply:

- Static station addressing: DHCP remains the available mode. STATIC is disabled
  and the limitation is shown; no settings are stored pretending they were applied.
- Internet reachability/captive portal testing: explicitly NOT TESTED. A Wi-Fi
  link is not presented as verified internet access. No fake sign-in confirmation.
- BSSID, DNS and negotiated link speed: NOT REPORTED, never inferred from RSSI.
- Separate association, authentication and DHCP events: the existing provider
  exposes combined JOINING and CONNECTED states. Progress reflects those actual
  states; errors do not invent a more precise authentication failure reason.
- The radio provider serializes scan and join. A new scan stops the current
  app-owned connection first. There is no background roam/autojoin policy here;
  connecting is explicit and other apps continue using demand-loaded profiles.
- Unsupported enterprise/security modes show an explanation and cannot join.

These are not claims of complete functional parity with the simulator. Completing
static IP and internet/portal checks requires an extension to the shared network
provider/native network service, not app-private hardware access.

## Build and integration

Use Runtime `8d713b52edca004e1d53bf30a1c584859fe33d14` for the deployed native ABI.

```
python scripts/generate_telemetry_fonts.py
python scripts/test_wifi_manager.py --runtime ../Runtime --output build/wifi-tests
python scripts/test_wifi_manager.py --runtime ../Runtime --output build/wifi-asan --sanitize
python scripts/build_wifi_manager.py --runtime ../Runtime --output build/wifi-manager
python scripts/build_text_input_services.py --runtime ../Runtime --output build/wifi-providers
```

The last command builds both the shared text-input service 0.1.4 and scene 0.5.0.
The original portable/watch and T5 legacy entrypoints remain available; the X4
scene deployment selects `Apps/wifi_manager/app.c` through the new build script.
Native manifest: `Apps/native/wifi_settings.json`. Executable: `wifi_settings.elf`.
Requires ui.scene@1 instance0, ui.text-input@1 instance0, net.wifi@1 instance15,
storage.key-value@1 instances6 (existing profiles) and1 (shared radio policy).
New firmware registration/provider selection and matching app are needed; replacing
only an ELF on firmware with the old scene is not sufficient.

## Verification

The fixture executes the real C app, shared scene and shared text-input provider
with bounded simulated radio and storage. Fourteen cases cover actual DOWN/UP
input, masked/visible/masked rendered text, hidden SSID, successful save, failure,
forget, busy storage, missing keyboard, old scene, invalid radio snapshots,
asynchronous cancellation and invocation loss inside radio/storage callbacks.
Normal and ASan/UBSan executions are separate. Captures are full settled native
480x800 framebuffers with test networks, not HTML or physical radio observations.
Existing component/Reader, telemetry, lifecycle and shared-keyboard tests run too.
Compilation and host tests do not establish RF, credentials or hardware success.
