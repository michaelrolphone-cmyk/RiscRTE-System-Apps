# Selected paper update catalogs

`build_portable_updates.py --touch-scrolling` selects OTA Update and App Store
1.2.4. It requires the explicit X4 native-time profile and paper transitions.
Omitting the option preserves the preceding app and provider ELF bytes and
manifests. No service ABI, product admission policy, grant, network destination,
or update feed changes.

The actual paper catalog had both a sixteen-row controller window and a
four-row visible page. The selected view uses one continuous 96-pixel row
coordinate space for Check plus the provider's existing maximum of 128 releases.
Only six visible release records are cached per submitted/completed image.
Rows clip to the existing 384-pixel viewport. A fitting catalog, progress,
confirmation, cleanup and Restart view retains its existing controls and layout.
Hardware Previous/Next reveals the selected row. Finger movement tracks the
shared bounded scroll controller, then decelerates. Ordinary motion uses damage
frames; initial and changed views retain clean presentation.

Input uses current logical geometry independently of submitted/completed pixels.
A touch pins the exact current release identity and offset at DOWN. A changed
catalog stops the gesture and requires a fresh choice. Navigation reveals the
current selection without waiting for a highlighted image. Identity, version,
installed version, availability, size and displayed reason are revalidated
before accepting Install and after reconnect immediately before `begin`.
Replacement, removal or a changed version cannot silently reuse a provider row
index. An explicit new confirmation remains required before installation.

Back and Cancel remain immediate while the display is BUSY. Quick Controls
owns its reserved gesture and suspends list momentum. Existing checked transport,
radio, bank and native-retention cleanup remains in the shared controller;
retained custody performs no subsequent catalog reads. Active operations and
confirmation still inhibit automatic Light sleep. Feed-unconfigured builds still
perform no network connection from Check.

## Reproduction

Use a staged native SDK containing the existing Runtime and tagged-alarm headers:

```
python3 scripts/test_touch_scroll_updates.py --logical-latency --sdk /path/to/native/include
python3 scripts/test_touch_scroll_updates.py --logical-latency --flip --sdk /path/to/native/include \
  --output-dir build/touch-scroll-updates-flip
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_x4_updates.py --touch-scrolling \
  --ble-broadcast --target-dir build/update-touch-scroll/selected
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_portable_update.py
```

The ordered-stream latency fixture executes OTA/App Store cases normally and
under ASan/UBSan at 0/17/2300 ms and never-completing display latency. It checks
immutable MONO1 submission custody, rapid navigation/touch, continuous scrolling,
exact logical selection, catalog mutations, explicit confirmation, cancellation,
and retained cleanup. Stable latest-screen raster hashes match across finite
latencies. The flipped run uses the production reader-orientation transform.
Historical snapshot-only scrolling fixtures are retained but do not qualify the
current ordered input reducer. See `UNIVERSAL_UI_PHASE2.md` and source-bound
receipts for the current passing matrix. LeakSanitizer is disabled because this
executor cannot support process tracing; AddressSanitizer and UndefinedBehaviorSanitizer
remain enabled.

`scripts/qualify_update_touch_scroll.py` rebuilds the immutable baseline and
selected targets using the same GCC 8.4.0 and verified idle/Runtime/alarm SDK
inputs. It checks manifests, imports/exports, structural ELF admission, source
hashes, all 14 existing grants, selected receipt agreement and unchanged provider
bytes. Its Watch and X4 flag-off comparisons are exact ELF/manifest comparisons.
All outputs remain development artifacts. Physical panel/radio/flash behavior
and a real published X4 release feed remain unqualified.
