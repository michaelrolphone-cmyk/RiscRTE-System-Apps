# Native Wi-Fi software qualification

Native Wi-Fi is an explicit standalone development profile, selected by
`PORTABLE_WIFI_NATIVE_UI`, `PORTABLE_NATIVE_CANVAS`, `PORTABLE_NATIVE_CUSTODY_FENCE`
and `PORTABLE_NOVA_UI`. The ordinary Wi-Fi builder defaults are unchanged.
Paper/X4 continues through the existing paper renderer and controller path.
No fixed 240-pixel image is scaled into the reported display.

## Geometry and interaction

The native renderer derives header, rows, footer and keyboard rectangles from
capability-reported dimensions. The hit tester consumes those same rectangles.
Selection pages by the actual visible row count. The 8-by-4 local keyboard keeps
its established character pages and page/delete/done actions. Glyph ink metrics
center compact labels inside their cells; the keyboard uses the available height.
The view preserves full SSID/password buffers and fits only the visible tail by
measured glyph advance. Passwords remain masked.

The full delivered feature recipe uses the existing copied shared text editor;
the local keyboard remains a separately selected legacy profile. Native layout
introduces no automatic fallback when a shared editor is unavailable.

The first frame or a changed control layout requires completed presentation
before a new gesture can begin. The app snapshots a raw sequence watermark at
its own successful display-token boundary, rejecting queued older beginnings.
An already established contact may finish across a status-only repaint when
its dimensions, page title, row labels, selection and keyboard page are unchanged.
Changing any of those control identities cancels it. The existing controller
view-generation boundary also cancels replaced scan results even when an SSID
label stays the same, such as an authentication-mode change. The controller rechecks the
current model before applying a tap, including a release delivered while a
compatible repaint is pending.

Ownership follows raster snapshots through submission, so an older or overlay
token cannot claim the successor frame's boundary. No timeout or guessed
timestamp epoch determines eligibility. Physical Back and runtime exit remain
responsive while painting is pending. Snapshot failure at either boundary path
invokes the existing retained-custody fence.

## Complete source-selected app

`build_native_wifi_qualification.py` locks the existing delivered Wi-Fi recipe,
all nine C inputs and all eighteen required capabilities. It retains scan and
connect ownership, disconnect/retry, atomic credential persistence and availability,
shared masked text entry, Contexts, telemetry, power/sleep/motion, Quick Controls,
app catalog and the existing return path. This is not the generic builder's
smaller feature subset. The source manifest is `Apps/native/wifi_settings.json`;
profile identity is `lib/PortableApps/profiles/native-wifi.json`.

The target compiler, dependency revisions, selected defines, source hashes,
license hashes, output bytes, import/export contract, ELF allocation layout and
production Runtime admission are recorded. Publication requires an exact clean
source commit plus normal and sanitizer admission receipts bound to that ELF
and manifest. Releases are draft-first and immutable after publication.

## Reproducible checks

- `scripts/test_native_wifi.py`: actual controller, adapter and native renderer,
  synthetic display/touch/radio/storage providers; eight sizes from 128x128 to
  410x502 and 502x240; normal, ASan/UBSan, snapshot and combined modes.
- `scripts/test_native_wifi_boundary.py`: idle-only negative control and corrected
  boundary using the same production code, plus a blanket-reset negative control
  that proves an established same-target tap must survive status-only repaint.
- `scripts/test_native_wifi_opt_out.py`: selected-compiler byte comparison against
  the immutable pre-native baseline with the native Wi-Fi selector absent.
- `tests/test_native_wifi_*.py`: source recipe, publication and admission contracts.
- `scripts/report_native_wifi_memory.py`: exact pinned production Runtime ELF
  admission, allocation placement and negative import/lifecycle/role controls.

Qualification captures and JSON/log receipts are attached to the focused CI run.
Synthetic providers prove software behavior, not physical panel timing or RAM fit.

## Compatibility limits

The development app requires the exact complete capability profile and a
capability-sized RGB565 display at least 128x128. Source-selected storage/radio
instances, return app and timezone policy remain those of the locked delivered
recipe. The standalone artifact does not establish a device installation path.
It does not replace, reserve a successor to, or mutate the immutable Wave .59
firmware/payload association. Hardware qualification remains outstanding.

## Ordered native idle qualification

The shared native correction keeps automatic idle behind ordered reset
cancellation, input resynchronization and held physical contacts. The actual
Wi-Fi atomic/availability controller fixture checks first-poll cancellation at
the idle deadline, contacts queued before presentation, contacts arriving during
the first poll and early contacts. Normal and ASan/UBSan controls require zero
sleep-helper entry while held, no unintended editor/network/storage action,
and eventual ordinary idle after release.

`test_native_wifi_idle_target.py` preprocesses the actual nine-input target
recipe and verifies that its automatic-idle guard and complete Wi-Fi readiness
body equal those exercised by the behavioral fixture. The other feature seams
remain explicitly bounded; the full target build and production-loader
admission qualify compilation and capability composition. No hardware result
or device memory-fit claim is inferred from these tests.
