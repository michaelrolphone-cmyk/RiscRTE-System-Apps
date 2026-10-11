# Shared text entry: product integration requirements

This is a source/host-test checkpoint, not a shipped product feature. X4 0.1.50
and Home 0.3.22 stay immutable. No provider has been inserted into that image.
The application versions in development build receipts are not new releases.

## Select and bind the provider packages

All three selected packages are ordinary, separately loaded Xtensa ESP32-S3
Driver ABI2 providers from this System source:

- `text-input-host` 0.1.0 provides `ui.text-input@1`. Its base manifest requires
  `ui.scene@1`. Select its alternative USB manifest only when the graph really
  supplies `usb.hid.keyboard@1`; the same ELF and ID serve both manifests.
- `scene-host` 0.1.1 provides `ui.scene@1` and requires `display.output@1`,
  `input.touch.raw@1`, `input.navigation@1`, `platform.clock@1`, and
  `ui.presentation-profile@1`. This replaces any older PR88 scene package with
  that ID; do not load duplicate scene hosts.
- Select exactly one `ui.presentation-profile@1` provider:
  `scene-profile-compact-color` 0.1.0 for 240x240 Watch, or
  `scene-profile-portrait-monochrome` 0.1.1 for X4's physical 800x480/logical
  480x800 presentation. Both have no provider dependencies. These are shared
  presentation policies, not app-local keyboard copies. The portrait profile uses
  scene display rotation 270 and touch rotation 0. Its logical(x,y) maps to physical
  (y,479-x), matching the installed X4 app adapter's rotation 90 convention. GT911
  touch is already portrait 480x800. The two components' rotation labels are not
  interchangeable; see `SHARED_TEXT_INPUT_ORIENTATION.md`.

Add the three manifests to the selected product's `drivers` list. Preserve
existing driver identities, instances, features, display policy and native
resources. Existing display/touch/navigation and platform clock satisfy the
scene edges only after exact product admission verifies their API contracts.

Each converted app's native manifest must require `ui.text-input@1` exactly
once. Its boot `app_capabilities` row must additionally grant
`{"capability":"ui.text-input","api":1,"instance_id":0}`. These clients acquire
instance0; select an unambiguous matching text provider. Preserve every existing
app policy grant. Apps do not need direct grants to `ui.scene`, the presentation
profile or raw HID merely to use text entry. Provider dependency binding owns
those edges. Private storage, alarms and other app grants remain app-owned.

The exact-pinned Points builds cover selected catalog Watch and X4 profiles.
The BLE development target is the Watch-style `alarm.service@1`/`rtc.clock@2`
profile, not the installed X4 resident `alarm.service@2`/native-time profile.
Before X4 replacement, rebuild BLE using the installed product's complete
resident/native-time flags and SDK/source pins, preserving its tagged alarm,
telemetry, custody and storage policies. Rebuild/rebind all changed clients;
do not transplant a development ELF over an incompatible profile.

## Hardware preference is conditional on a real input provider

A raw keyboard snapshot reports attachment; the mere existence of a provider
is not attachment. The base manifest supplies the same onscreen keyboard but
cannot discover hardware. The USB manifest enables tested attach/detach switching
only when the real `usb.hid.keyboard@1` input provider and its complete underlying
USB host/HID/controller/board-resource closure are installed and qualified.

The inspected .50 graph has no `usb.hid.keyboard` provider. Its `ble-hid` package
provides outbound `bluetooth.hid`, and `usb-device-msc-esp32s3` provides
`usb.device.msc`; neither is a keyboard-input source. Do not relabel either as
keyboard discovery. USB host/device PHY ownership and physical power/attachment
behavior need product-specific qualification. Hardware behavior here uses test
doubles; no physical keyboard or Watch/X4 test has been performed.

## Runtime and capacity prerequisites

No new Runtime UI logic, text semantics or per-client lifecycle extension is
needed for the normal converted close-before-release flows. Existing generic
Driver ABI2 dependency/lease/quiescence semantics, native capability acquisition,
invocation retention, and asynchronous display ownership are required. The
qualified production Runtime source is
`e66c5f1056a90f80b17cbe3f42db0242cbe2ced4` (recovered 0.1.99 SDK source). Client
receipts separately record their exact SDK source; a compile pin is not proof of
binding to the selected firmware. Require the retention API tail and its
terminal liveness behavior, not only a nominal API1 prefix. Earlier firmware is
not established by these tests.

The current X4 .50 graph already selects23 ordinary providers. Adding the three
above without removing features requires26, exceeding the tested paired/PSRAM
Runtime's24-provider bound. Demand loading does not reduce the admitted graph
count. A coherent capacity/metadata build change is required before full .50
composition can pass; see `SHARED_TEXT_INPUT_CAPACITY_DIRECTION.md`. Additional
USB input packages need further slots unless already present. This checkpoint
does not change the native binary or silently trim existing features.

All newly bound artifacts need fresh product/version reservations, a complete
boot/cohort manifest, exact firmware/import evidence, strict ELF validation,
production Runtime admission including boundary tests, resident/child lifecycle
replays and store round trips. Preserve .50's native SDMMC/provider behavior and
fast-interactive display flags. Physical e-ink timing, USB attachment, cancellation,
Back/Home, repeated invocation and sleep/wake remain product qualification work.

## Migration scope

Converted: selected Points catalog custom-type names, and BLE per-address sensor
aliases. Points allows31 printable ASCII characters and BLE24; empty accepted
BLE text clears an alias. Both use the shared capability and close before release on accepted,
cancelled, error, Back, alarm handoff and ordinary app-exit paths. Pending cleanup
retains the exact session/grant until completion; uncertain custody is terminal.
They have no local keyboard fallback. Missing hosts surface an unavailable state.

Other source paths remain unmigrated, including the legacy Points app, Timecard
text entry, Text Editor/filename USB handling, Wi-Fi credentials, File Browser
filters/rename, LoRa and audio/RF label/message editors. Some require different
limits or password treatment; this checkpoint provides only printable ASCII
plain text, at most71 bytes. Existing staging copies of legacy headers alone do
not prove an active renderer is linked into a converted client. Alarms' PR88
semantic form presenter is reused; time/integer forms are not new text formats.

## Abandoned sessions and separate lifecycle direction

Converted clients explicitly close. A caller that returns without close under
eager boot pins may unload while its pointer-free host session remains active;
the host then refuses graph shutdown and Runtime requires restart. This is a
regression-tested limitation, not automatic app-exit cleanup. Explicitly armed
demand-retained mode has the same pointer-free orphan outcome; unpromoted
demand modes retain the app at failed grant teardown. The separate
`SHARED_SERVICE_LIFECYCLE_DIRECTION.md` proposes optional generic per-consumer
leases so a future Runtime can request provider-owned cancellation even while
boot pins keep a provider loaded. That extension is not implemented or required
for the normal converted paths.

### Terminal retention with the installed diagnostic profile

Text handoff uses `portable_adapter_retain_silent()` when the existing
`PORTABLE_ALARM_TERMINAL_RETENTION` selection is enabled. A lost Runtime API,
terminal host result, or abandoned editor cannot safely emit stage diagnostics:
the normal stage logger calls Runtime health and diagnostic callbacks after
custody may already be terminal. The flag-off legacy path is unchanged.

The X4 Points exact-profile regression includes `PORTABLE_STAGE_LOGS` and checks
all five `text-runtime-*` boundaries plus `text-abandoned`; the test rejects any
provider, allocator, health or diagnostic call after Runtime liveness is lost.
The separate entrypoint scenarios repeat acceptance, cancellation, pending close,
unavailable host, Home and alarm interruption through actual `app_main`.
