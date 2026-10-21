# T044: Watch low-battery reconciliation and local qualification

## Result

The requested production behavior already exists. System Apps PR #56 merged implementation `f146d82d4c2bc4d4b6ac97f7be83b04035ad7d60` on 2026-10-07. Published Watch 1.0.7 contains the low-battery feature in all 22 apps. No production change was made or justified.

A current regression fixture failed because its scripted timer increment was delivered during page-entry input gating. Its consecutive Save samples were a held contact, not two taps. A test-only repair allows neutral page-entry polls and separates later gestures, without changing any assertions, production input gates, policy, version, pin, manifest or release.

Local System Apps base: `7270a10ed9cefba6ec1368060196a13bb3370985`.
Local branch: `tasker/t044-low-battery-qualification-20261021`.
Local test-only commit: `df045bcdb30fa95650156ab9ad9f977f2ca8382a`.
Changed file: `test/native_apps/low_battery_adapter_test.c`.
Patch: `t044-low-battery-fixture.patch`.

All work was in isolated checkouts. Nothing was pushed, published, merged, tagged, released, cataloged, flashed or sent to hardware.

## Preserved policy

- A valid transition into below 10% consumes a persistent edge before changing existing namespace-1 preferences: idle sleep 20 seconds, brightness 15%, Hybrid deep-sleep delay one minute, Wi-Fi/Bluetooth off.
- Subsequent manual timer, brightness and radio overrides are preserved while low, including across app changes and reboot.
- A valid reading at or above 10% rearms without restoring previous preferences.
- Established first-start behavior: the first valid low reading with no persisted edge counts as entry. This predates this task and was not reinterpreted.
- Failed reads, percent 255, missing gauge profile and confirmed absent battery are ignored. Charging/external power alone neither suppresses nor rearms the edge.
- Existing Light/Deep/Hybrid selection is retained. The deep timer governs Hybrid progression, not a new sleep-mode selection.
- Partial writes are intentionally not replayed while low. Invalid/unavailable edge storage and uncertain capability release or radio teardown remain explicit failures.
- Shared capability APIs supply battery, key-value storage, display and radio control. No device-name branch or new hardware/Runtime ABI was added.

The historical behavior is documented in System Apps `f146d82d:docs/PORTABLE_LOW_BATTERY.md` and Watch `docs/LOW_BATTERY_PROFILE.md`. Current System Apps retains the policy; its additional paper/X4 frontlight handling was not modified.

## Exact source set

- Watch current main: `81dbde86969615f7378cc03c5800960a9092e076`, branch `tasker/t044-low-battery-edge-20261021`, no edits.
- System Apps current main: `7270a10ed9cefba6ec1368060196a13bb3370985`, plus the local test-only commit above.
- Accepted System Apps implementation: `f146d82d4c2bc4d4b6ac97f7be83b04035ad7d60`, clean detached checkout `System-Accepted`.
- Runtime current main: `13eca114cb58469cf6004fafc3da84fbd9d29beb`, clean detached checkout.
- Utilities current main: `6b2047500905f8f45087cb49989103a926fc5c63`, clean detached checkout.

`qualification.json` records all pins, commands and scopes. The canonical power-union receipt additionally hashes its full staged SDK and source inputs. No changes were made to T039/T040/T043 checkouts, Runtime, Utilities, accepted product bytes, source pins or publication gates.

## Baseline and repaired evidence

The unchanged current low-battery runner passed four core policy configurations and cases 0/1 before failing case 2 at its unchanged expectation of a 25-second manual idle timer. See `system-current-low-battery.log`. A separate instrumented fixture, not a production edit, reproduces the same failure in `trace-baseline.log`: the 3/4-poll increment did not leave the page-entry neutral gate, and 7/8-poll samples are one held Save contact. Trace code is retained as `trace-baseline.c`.

The repaired fixture uses increment at poll 10, Save/Cancel at 20 and exit/Cancel at 40. All existing exact timer, readback, persistence, Save/Cancel, unconfirmed-write and retry assertions remain unchanged.

Passed checks:

1. Current System Apps: `python3 scripts/test_low_battery.py` — 44 executions, comprising four policy configurations and 40 real adapter/Settings runs, normal and ASan/UBSan, rotations 0/180. Log: `system-current-final-low-battery.log`.
2. Accepted System Apps pin: `python3 scripts/test_low_battery.py` — 38 executions. Log: `system-accepted-low-battery.log`.
3. Current recovered Watch source with accepted System pin: `python3 scripts/test_low_battery.py --system-apps ../System-Accepted` — four Clock/Hybrid programs, normal and ASan/UBSan. Log: `watch-accepted-low-battery.log`. This is not a complete historical Watch rebuild.
4. Native Settings: `python3 scripts/test_native_settings_timers.py --output ../evidence/native-timers` — 220 executions, five reported geometries, rotations 0/180, normal and ASan/UBSan. Includes bounds, save/cancel, failed save/retry, corrupt/unavailable storage and input gate coverage. Receipt: `native-timers/receipt.json`.
5. Canonical Watch power union: `python3 RiscRTE-T-Watch-S3/scripts/test_selected_power_union.py --system RiscRTE-System-Apps --runtime RiscRTE --alarm-source RiscRTE-Utilities --canonical-runtime-sdk --output evidence/current-watch-power-union` — 10 normal/sanitized programs. Receipt: `current-watch-power-union/qualification.json`. This explicitly stages current shared policy, canonical Runtime SDK and completed alarm suffix; it does not select a production profile.
6. Watch frozen-profile/version gates: `../test-venv/bin/python -m unittest discover -s tests -p test_low_battery_profile.py -v` — five tests. Log: `watch-low-battery-profile-final.log`.
7. Independent review and rerun at 2026-10-21 08:30:44 UTC: `python scripts/test_low_battery.py --case 2 --case 3` — four policy configurations plus eight adapter runs passed.
8. `git diff --check` passed. The only committed change is the test fixture.

ASan/UBSan were run; leak detection was disabled by the existing runners. Python profile checks initially lacked pyelftools/jsonschema. The isolated test virtual environment supplies pyelftools 0.32 and jsonschema 4.25.1; dependency setup and original failures are retained.

## Prior publication verified without changing it

- Existing implementation PR: https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/pull/56
- Existing implementation CI: https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/37528912019 — completed successfully at exact source `f146d82d4c2bc4d4b6ac97f7be83b04035ad7d60`.
- Existing release: https://github.com/michaelrolphone-cmyk/RiscRTE-T-Watch-S3/releases/tag/firmware-v1.0.7
- Release source: `cf30d732271db19a74492d4753736cafeafb92bb`.
- Accepted Watch source named in release: `5c317f80b471d754111dbbe5a14fe97fd0dc377c`.
- Downloaded accepted-app-evidence ZIP SHA-256: `419f66a79f6718742830adbb223614e266d0a6e25a30635cdafc18b72ac1a0f9`.
- Downloaded product-provenance JSON SHA-256: `87f1bf34683312fa2986133572febba340c2482cf1661a1a56b537a6b4290c5c`.
- Both match the current catalog metadata exactly. The build record declares `PORTABLE_LOW_BATTERY` in every one of its 22 apps and pins System Apps `f146d82d`.
- The declared accepted image SHA-256 remains `b039468858ab3cb5d1b53d843257e0e034d0a0b661ddaf3b74b59670519b0b2b`. The full BIN was not downloaded or rehashed in this task.

## Unqualified or intentionally unchanged

- No hardware measurement, actual installation, physical brightness/radio state, current draw or battery-crossing test was performed. Prior SDR acceptance is not low-battery acceptance.
- No new full target firmware/product cohort was compiled. Host proof does not qualify an arbitrary current installed product.
- Directly combining current Watch with current System in the legacy `test_low_battery.py` fails its frozen local/shared policy equality guard. This is preserved in `watch-current-low-battery.log`, not bypassed. Accepted-pin mode passes; the separate explicit canonical power-union runner passes without rewriting the frozen profile.
- Git could not retrieve historical Watch source `5c317f80...` by SHA in this environment. Exact historical full-source reproduction is therefore not claimed. Its published evidence and provenance were verified instead.
- Current Watch sleep/RTC, mounted-SD, installation and headless alarm ownership remain separate. T063 is a history record, native Alarms software delivery is completed, and the independent physical sleep/RTC outcomes are not closed by these host tests.
- The local test-only commit remains unpublished. Existing production publication is separate and already verified.

Recommended board state: Awaiting qualification, production source finished, scoped software tests passed, existing production publication published, hardware not tested. No fresh hardware request or publication approval was requested.
