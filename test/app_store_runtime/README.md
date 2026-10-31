# App Store Runtime qualification

The production App Store controller, inventory facade, presenter, background adapter,
and published scene/profile execute under the real Runtime/ProviderGraph. A read-only
observer copies the presented document/navigation. Input is native navigation frames;
no semantic scene event or application-state mutation is injected.

Each geometry runs demand and demand-retained provider activation in three lanes:
* Select release, inspect full details, native Back; no connection or installation.
* Inspect details, Review, Cancel-default confirmation, deliberate Confirm, join saved
  WiFi, begin the release, then native Cancel before the next READY transition.
* No ui.scene grant: two clean invocations without hardware/update/storage I/O.

Each lane invokes and actually unloads the app twice, preserving exact namespace1
quick_radio and namespace6 credential bytes with zero writes. One compiled application
is reused across all geometries. Software-update, WiFi, display, touch, time and durable
storage endpoints are doubles. No package is installed, no real network is used, and
this is not target-device or product-native recipe evidence. Optional installed inventory
and BLE service are absent in this bounded matrix; their own domain suites qualify them.
This matrix exercises navigation Cancel, not a held physical touchscreen contact.

Run scripts/test_app_store_runtime.py with --runtime, --utilities (verified public bf111
Battery helper inputs), --output, and optionally --sanitize. Raw per-command logs,
commands, source hashes and results are retained. Endpoint update_step intentionally
advances to READY after delivery of the physical Cancel frame if cancellation was not
processed first; activate is forbidden in this lane.
