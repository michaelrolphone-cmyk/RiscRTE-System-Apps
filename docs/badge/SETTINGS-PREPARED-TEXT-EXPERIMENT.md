# Settings prepared-text experiment — unselected

Base System `88b12c0605b9e5c5b450075539adf64e7c7480cd`.
`PORTABLE_SETTINGS_PREPARED_TEXT` is opt-in and is not added to any builder or
published profile. **No measured host speed gain; no device cycles or FPS claim.**

## Source finding and bounded candidate

Snapshot replay runs Settings primitives one physical row at a time. Every text
row previously repeated alignment measurement, the complete string's glyph
bounds/advances, round chord fitting and font-bank descent. Conservative command
bounds separately scan font banks once per command; this experiment leaves those
bounds and replay cadence unchanged.

The candidate retains only pen, top and resolved font side for the current
immutable text command or button's single nested text call. It checks command
owner, width, height, format and round mode. Command span, materialization and
free reset validity. Arbitrary custom commands cannot borrow this setup.
Rendering/glyph lookup loops, pixel checks, clipping/rotation/flip transforms,
callbacks, frame receipts, displayed action maps and allocation fallbacks remain
unchanged. There are no new allocations. This is not the earlier Nova
icon/caption metadata experiment (`fcc31e6`), whose mixed host results already
prevented selection.

## Measured workload and limits

The existing actual Settings renderer/controller fixture completes 25 root
scroll redraws at offsets 0,5,...,120 on 466x466 with 69px safe inset, native and
round geometry, snapshot/sliced replay and 8ms work budget. It uses
`Apps/settings.c`, **not the exact selected native-time entry or a physical touch
acquisition sequence**. The exact selected native-time target is linked separately.

| Count, 25 completed frames | Baseline | Candidate |
| --- | ---: | ---: |
| Recorded text commands, including invisible commands | 650 | 650 |
| Text setup evaluations, including nested button text | 9,983 | 422 |
| Alignment/button measure calls | 2,238 | 1,308 |
| Full-string round fitting passes | 15,541 | 710 |
| Glyph lookups inside text fitting/drawing only | 238,820 | 91,951 |

These glyph counts exclude command-span and measurement lookups. Rectangle and
button-outline setup remain unchanged. Counts are not cycle estimates.

Uninstrumented host `-Os`, 16 alternating-order pairs, identical completed frame
hashes and progress-step counts: baseline median **0.2824915s** (range
0.257787–0.348804); candidate median **0.285360s** (0.249971–0.396547), about **1.0%
slower** at the medians. The earlier single instrumented 0.366181→0.297140s result
is preserved but does not establish a gain and must not be substituted for this
matched result. No further optimization or selection follows this experiment.

## Exact selected target and assembly evidence

The current selected Settings compile recipe from
`badge-picker-cohort-build/target/settings/settings-build-record.json` was reused
with only source paths/output paths and the candidate define changed. Disabled
candidate exactly reproduces selected ELF SHA-256
`7bbbb0ecf7e4b2afc3ed7cb3071e555cd0223f3fbc078171f8b60ec2126d77d0`.
Xtensa ESP32-S3 GCC 15.2.0 `-Os`: text 255,906→256,314 (**+408 bytes**), data
51,776 unchanged, BSS 2,060→2,092 (**+32 bytes**); imports unchanged.

Baseline `sv_text_aligned` contains an actual `call8 sv_glyph_at` at `0x15bed`
inside its string-fit loop and `call8 portable_round_band` at `0x15c89`.
`portable_round_band` calls `portable_round_contains` in a binary-search loop;
the latter uses `mull`/`muluh` for 64-bit ellipse arithmetic. These are real target
instructions, not instrumentation. Candidate setup-hit path jumps at
`0x15c71` to `0x15e95`, past fitting, but adds owner/geometry guard loads and
width/height/round calls. This establishes avoided target work, **not a net target
speed gain**; pixel work and glyph drawing lookup remain. The host null result
cannot be overruled with static instruction inspection.

## Qualification and preserved failures

* `scripts/test_settings_prepared_text.py --runtime-sdk SDK --output DIR` compares
  actual baseline/candidate replay: 144 cases each normal and ASan/UBSan, varying
  text/button, all six faces, all three alignments, round/non-round and rotation.
  Same-command dimensions/round mode rebind mid-replay; command storage is reused
  after explicit free. A terminal failed-state repeat cannot write pixels.
* Existing nine-page synchronous/sliced hashes, four completed-map cases, two
  allocation fallbacks and 15 retained/save cases pass normal and ASan/UBSan in
  the owning fixture's original rectangular profile.
* Full timing-suite pass requires the already-established test-only timestamp
  correction from `badge-settings-timestamp-tests/scripts/settings_sliced_timing.py`:
  observed touch events retain acquisition ticks rather than backdating to the
  scheduled position phase. The runner substitutes that module without changing
  product source or weakening assertions. All eight sync and eight sliced model
  cases pass in both normal/sanitized lanes. Original failing logs are preserved.
* Adding round geometry to the old retained fixture makes its fixed touch
  coordinates miss the adjusted button: baseline **and** candidate fail the same
  first retained assertion. Before that failure, all round page/map/OOM comparisons
  pass. The dedicated dynamic parity test covers round geometry independently;
  this does not claim round retained touch coverage from the incompatible fixture.
* No exhaustive new flip/clip/callback-rebind suite or physical execution was
  performed; those paths' pixel implementation is unchanged. This experiment is
  not a production-admission recommendation.

Evidence scripts, raw logs, original failures, corrected-module mapping, target
ELFs/disassembly and hash manifest are frozen in
`badge-settings-prepared-evidence` beside this isolated worktree.
