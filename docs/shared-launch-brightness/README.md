# Shared app-launch brightness repair

The generic RGB adapter now restores the current brightness after its first
successfully completed display presentation, once per invocation. This covers
an early unpromoted handoff that reopens the display dark and a later retained
provider. Synchronous and asynchronous paths share the same policy. Accepted
brightness previews and torch remain authoritative. The change does not write
preferences, change display drivers or require provider retention. Paper and
non-Quick builds retain their existing behavior.

The cumulative source also preserves native SDR rendering and its unobscured
status strip from System e49bded and the exact replay metadata from 2b954689.
All 36 target recipes bind that cumulative source and preserve native SDR.
The Waterfall recipes use the measured [lower-memory linker option](linker/README.md).
Each final ELF retains its pre-existing import/export interface.

## Source qualification

The complete cumulative-source tests passed:

- 126 first-frame cases: 240x240 and 410x502, sync/async, normal/ASan/UBSan,
  saved level, previews, torch, failures, retained custody and paper/no-Quick
  controls.
- 192 custody cases, including callback retention, on both RGB sizes and both
  paper presentation modes, normal and ASan/UBSan.
- Eight dark-launch reproductions on the unchanged deployed adapter fe238022.
- Eight brightness and eight custody baseline cases at the CI baseline
  ffe0295, exercising the actual workflow commands.
- Native RF raster regression, 632 immediate/replayed comparisons.

`qualification.json` records the case outcomes and source hashes. Address and
undefined-behavior checks are enabled; leak detection is disabled for the host
execution environment. Host tests do not prove physical display behavior.

## Target qualification

Fresh strict replays passed all 17 Waveshare, 18 T-Watch and one external-media
LoRa recipes against source 96742a20. Every transitive input, final ELF hash,
size and symbol inventory is checked. `strict-replay.json` records the results.
This qualifies target bytes; final product composition/admission, catalog
publication and physical acceptance remain separate.

## Preserved Springboard investigation

The separate Springboard source f31f896 already restores brightness on COMPLETE.
No additional Springboard defect was demonstrated by the launch, sleep,
provider retention, panel reopen or presentation audit. Its panel reopen is
intentionally dark; its first completed frame restores the selected level.
The existing 32 first-frame sanitizer cases pass.

`test_springboard_launch_brightness.py` additionally executes the real controller
rather than only a painted fixture image. It checks dark and retained providers,
both sizes and both presentation paths, three complete invocations per case,
visible framebuffer pixels, exactly one brightness restoration per invocation,
and clean teardown. It does not rewrite production source.

## Reproduction

Run the shared test scripts with `--sdk-include`, `--app-data-sdk`,
`--utilities-root` and `--output` as in the shared-data-custody workflow.
Add `--callback-audit` to the custody runner. For preserved Springboard:

```
python3 scripts/test_springboard_launch_brightness.py \
  --springboard-root /path/to/System-f31f896 \
  --bridges /path/to/Watch-6e82fdca \
  --output build/springboard-brightness --sanitize
```

## Hardware acceptance and targeted diagnostic

The eventual cumulative test builds must retain the native SDR and watch-face
predecessors. Launch Springboard, SDR and an ordinary app both soon after boot
and after services are ready, returning by crown between launches. Each first
completed frame should appear at the selected brightness. A physical pass has
not been recorded for this repair.

If Springboard still opens black, retain the full serial segment from launch
request through the first attempted return, including RTE_SOURCE, app load/init,
provider starts and PANEL_BRIGHTNESS. On Waveshare a default 40-percent restore
prints `PANEL_BRIGHTNESS value=102`; a different saved level scales to 0..255.
The preserved stage-profile trace identifies first draw, submit and COMPLETE
(phases 4, 5 and 6). Missing COMPLETE points before visibility restoration. A
successful nonzero brightness command requires checking actual output/content
rather than assuming that adding another brightness write fixes it. Do not
reclassify the separate retained-invocation crash or motion-stutter tasks.
