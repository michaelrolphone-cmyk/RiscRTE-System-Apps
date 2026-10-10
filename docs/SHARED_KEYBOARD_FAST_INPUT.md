# Shared keyboard: fast input and NOVA-7 portrait presentation

## Integration unit

This is a component-only successor to installed System `a8459b94597baba3021d0c8283836008c846ded2` (.52), via the separately committed additive scene lifecycle checkpoint `4c06bbd5c84c4da12432cb26a9af85f6d217aa52`.

Select **scene-host 0.1.3 and text-input-host 0.1.1 together**, with the unchanged portrait-monochrome 0.1.1 profile (display rotation 270, touch rotation 0). The new text owner carries the copied character limit in the private keyboard-node extension; the new presenter supplies CLEAR. The public text-entry ABI and the frozen base scene/checkpoint headers are unchanged. Compact presentation still uses its existing geometry. Both profiles use the same scene-host ELF.

The prerequisite lifecycle suffix is size/version-safe and optional; existing text clients never configure it. It does not install or select Alarms, alarm-control, a resident policy, or storage migration. Do not pull the Alarms app or domain providers into a keyboard-only product update. The separate Alarms source/archive is untouched.

## Input repair

The prior shared presenter disabled keys whenever a text revision was dirty or its frame was BUSY. It also resampled touch on every completed text frame. Rapid contacts were discarded between letters, and a contact crossing completion could disappear.

The repaired presenter retains the already displayed hit topology across otherwise byte-identical keyboard documents that change only text/revision. A fixed 32-entry semantic FIFO decouples validated release gestures from frame completion and owner acknowledgement. Each delivered key is rebased only to the latest text revision of that same topology. The owner still updates after every key, including an ignored capacity-limited edit. No app pointers, callbacks, pixel coordinates or frame handles enter the queue.

Layer, hardware attachment, route, node/action/flags, and other document changes still advance the input epoch, clear old queued keys, and require a completed new image plus neutral input. Layer/Done/Cancel block subsequent old-layout contacts, including while an earlier queued letter is acknowledged. Home discards pending keyboard keys. Queue overflow explicitly retains the session rather than silently pretending success; retained/closed sessions perform no further provider I/O. Touch gaps, multiple contacts, truncated provider drains, stale frames and supersession retain their fail-closed fences.

## Presentation

The shared portrait presenter uses the supplied NOVA-7 Orbitron 700/900 and Rajdhani 700 font assets, exact baseline sizes, 4-pixel rounded key outlines, backspace icon, inverted DONE and pressed-key feedback, back/Home controls, text field, insertion cursor, character count and CLEAR. Typed case is preserved. All 95 printable ASCII characters remain reachable through four layers. Orbitron lacks the caret glyph; that single character uses the licensed Rajdhani fallback at the same physical size instead of rendering a missing-glyph box.

The uppercase key-grid crop differs from the previously qualified Points renderer by **2 of 153,488 pixels**. Four real presenter raster previews and the comparison receipt are included. The generic editor intentionally does not invent Points-specific symbol/type controls. The long draft displays its insertion end instead of scaling the entire text to illegibility.

Font inputs, a deterministic generator, glyph metrics/kerning/bitmaps, provenance hashes and SIL OFL licenses are included. The builder now fingerprints nested glyph includes and carries font licenses in the scene package. The genuine fonts increase the target scene ELF and loaded sections; see the qualification receipt. This host proof does not establish device heap/latency limits.

## Qualification

- 170 actual scene/profile cases, normal and ASan/UBSan: long BUSY same/different-key bursts, 100 further taps, held contact across text/frame completion, queued layer barrier, all four layers/all ASCII, no stale layer activation, bounded FIFO overflow retention, HID attach/detach, gaps, multi-touch, orientation, cancellation, cleanup and unchanged lifecycle discovery/controls.
- 100 production Runtime/Graph/Module executions on native .101 with compact presentation, normal; 100 on native .102 with portrait270 presentation, ASan/UBSan. These load the real text and scene provider ELFs, exercise eager/demand/demand-retained/armed policies, and include rapid input, long BUSY layer fencing, accept/cancel, close/pending/retention, hardware input, callback revocation and actual application unloads.
- Additional fast-input cross-checks: portrait normal and compact sanitized, 8 executions each.
- 28 text-owner/ABI tests, including shared CLEAR, revision exhaustion, copied request lifetimes and legacy text-transport coexistence.
- 24 existing Home/Points/BLE resident-controller cases (normal and ASan/UBSan), using the exact .52 Home build receipt/product metadata and preserved .51 Points/BLE sources. Accepted/cancelled/Back, pending/retained close and Home handoff pass. The external Runtime fixture's obsolete profile90 test flag is explicitly replaced with profile270 and fingerprinted by the wrapper; production sources are unchanged.
- 18 existing production Runtime Alarms executions pass against the successor, including actual UI unload/reload, nonresident and headless scheduler paths. These are regression tests, not product integration.
- The old shared scene/text services fail the new Runtime rapid-burst ordered-text assertion. The negative receipt identifies that expected failure.
- Both target packages structurally validate. Exact target imports/exports and source/SDK hashes are recorded. Rebuilding verifies byte-identical output. PlatformIO was not invoked; its telemetry-disabling environment was supplied to target build commands.

Physical devices are doubled. No product composition, publication, hardware access or flash occurred. ASan/UBSan use `detect_leaks=0`; LeakSanitizer and hardware typing latency are not claimed. The resident client fixture uses test entrypoints around real production controller/lifecycle functions, not full continuous app loops or RF scans. Detailed inherited fixture limitations remain in its evidence receipt.

## Reproduction

Use `scripts/test_scene_host.py`, `scripts/test_text_input_host.py`, and `scripts/test_text_input_runtime.py` with an explicit Runtime root; the latter accepts `--paper`, `--fast-only` and `--sanitize`. `scripts/test_shared_keyboard_clients.py` wraps the selected Runtime's existing client fixture, with every external source path passed through explicitly. `scripts/build_text_input_services.py` builds scene/text packages with `NATIVE_APP_CC` pointing to the existing Xtensa GCC 8.4 toolchain and `PLATFORMIO_SETTING_ENABLE_TELEMETRY=No`.

`python scripts/generate_scene_keyboard_fonts.py` regenerates the checked-in glyphs from the bundled licensed font inputs. Exact final hashes, source revisions, commands and output paths are in the adjacent qualification receipt.
