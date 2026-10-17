# Paper Quick coordinate/raster baseline

Frozen source: System HEAD `118c7c27f3b143e34849f70c4a30f2fdbf886e86`, tree `c55e0dac6e2d5a64f42a92bff251f71ae4498d2b`, branch `paper-coordinate-reproduction-20261017`. Tracked diff was empty at baseline freeze, 2026-10-17 17:11 UTC. No production edits, commits, publication, hardware qualification, or external record writes were performed by this reproduction.

## Measured results

- **410×700 legacy sheet:** actual first-tile paint is `[27,201) × [264,344)`. Actual accepted tap rectangle is `[28,202) × [265,345)`. Horizontal tests used y=300; vertical tests used x=100. Press/release at x=27 or y=264 produced zero writes; x=28 or y=265 produced two SILENT preference writes. The unpainted right/bottom coordinates x=201 and y=344 also produced two writes; x=202 and y=345 produced zero. No writes occurred before release. Every run restored the foreground pixels exactly and released all frame/grant/subscription custody.
- **480×800 legacy control:** first-tile paint and hit rectangle both equal `[32,236) × [302,394)`. The generated provider fixture's full sheet is pixel-identical to the unchanged original production-app test fixture.
- **480×800 resident control:** the real resident host renders 57,912 black pixels without audio and 71,276 with audio. Both passes include title, icons, sliders, tiles, instructions, and battery content.
- **480×640 resident witness:** the real resident host renders only 2,080 black pixels in either audio profile. Without audio, the only painted black rows are 62–64 and 149–150, each 416 pixels wide. Title, slider, tile, and footer content disappear. With audio, the lower separator moves with the current reference layout; the total remains 2,080. An invisible USB tile still accepts the real action in the existing host/client lifecycle control.

The 410×700 image also shows clipped time and UNAVAILABLE text. This is a bounded visual observation only; text fitting/reflow is outside this coordinate correction.

## Qualified controls

175 successful final cases:

- 108 nonresident cases: edge press/release tests, Back cancellation, exact restore and clean lifecycle, 480×800 controls, 410×700 controls, display 90-degree orientation, reader UI flip, raw-touch 180-degree rotation, deferred raster snapshots, ASan+UBSan, and three delayed immutable-provider cases.
- 36 resident cases: six profiles × modes 0, 1, 3, 5, 7, 20. These cover repeated overlays and exact completed-image restore, USB action, terminal retention, focus-failure retention, audio content, and Home exit. Profiles are 480×800, 480×640, 480×640 raster snapshot, 480×800 display orientation, 480×800 reader flip, and 480×640 ASan+UBSan.
- 31 unchanged original test cases: every existing clock case in `scripts/test_paper_quick_actions.py` and all eight launcher cases, in the normal 480×800 build. Original multitouch/cancellation, service actions, alarm preemption, Home, and ownership assertions pass.

All 19 full-image comparisons pass, including normalized 410×700 orientation/flip/raw-touch controls, normalized resident 480×800 orientation/flip, resident 640 snapshot/sanitizer equivalence, and generated-versus-original 480×800 legacy equivalence. Display 90-degree orientation is separate from the existing native raw-touch 0/180-degree contract.

Delayed controls model a 75 ms queued presentation. They assert byte-for-byte immutability until completion, no new frame acquisition while pending, actual action commit only after release, exact final background restoration, and no outstanding ownership. Normal/snapshot/sanitized cases made 195/193/195 immutable status checks respectively. These are fixture timings, not hardware performance claims.

## Source-bound closure

1. `foreground_adapter_open.inc` uses `portable_display_dimensions` and `portable_display_geometry`; these admit the tested non-reference sizes. The provider surfaces use correct MONO1 ceil-byte strides.
2. `quick_paper.inc` currently converts rectangle origins and dimensions independently: x*W/480, y*H/800, w*W/480, h*H/800. This causes both the measured non-reference edge displacement and zero-height spans.
3. `adapter.c` sends paper input through floor(px*480/W), floor(py*800/H), after the current orientation path. `quick_actions.c::tile_at` uses half-open canonical rectangles. These facts give the measured ceil-projected physical hit endpoints.
4. `quick_reference.inc::qr_text`, `qr_round`, and `qr_icon` all emit source row spans through `qp_rect(...,h=1,...)`. At H=640, the actual current height calculation is 1*640/800=0 for every row. `adapter.c::fill` returns for non-positive height. The two thicker separator rectangles are the only surviving resident content.
5. The resident cache path in `quick_reference.inc` is restricted to exact 480×800; the 480×640 snapshot path reproduces the same defect and proves it is not an old cached reference image.
6. The resident source deliberately draws a rounded tile outer border at (x−2,y−2,208,96), while the hit rectangle is (x,y,204,92). That existing decorative extension is not a coordinate defect and must remain at the reference extent.

## Smallest coordinate contract recommendation

Keep the existing inverse-floor input mapping. Project canonical half-open rectangle endpoints, rather than separately scaling an origin and size:

- X0 = ceil(x * W / 480)
- X1 = ceil((x + w) * W / 480)
- Y0 = ceil((y + transition_offset) * H / 800)
- Y1 = ceil((y + transition_offset + h) * H / 800)
- The resulting rectangle is [X0,X1) × [Y0,Y1), with dimensions X1−X0 and Y1−Y0.

For canonical hit [a,b), integer physical px satisfies a ≤ floor(px*480/W) < b exactly when ceil(aW/480) ≤ px < ceil(bW/480). Thus the same endpoint projection preserves current accepted taps and aligns the legacy tile paint to them. At 410×700 the proposed legacy tile paint becomes `[28,202) × [265,345)`, matching the measured existing taps. At 480×800, every endpoint and span is unchanged, including transition offsets.

**Collapsed rows:** do not blindly force every source span to at least one pixel. At H=640, consecutive canonical one-row spans projected by endpoints partition the screen into 640 one-pixel spans and 160 empty spans. This is consistent with the retained inverse-floor sampling. Empty spans must be harmless no-ops; forcing them to one pixel can paint over adjacent rows or outside a control. The necessary visibility requirement is complete rendered glyph/control content at the admitted profile, verified on the actual renderer and provider pixels. This report does not claim that every individual source row survives, nor that an unexecuted fix has met that requirement. If complete content still fails, a separate, explicitly reviewed bitmap-resampling/coverage policy would be needed; it should not be hidden inside a global minimum-one rectangle rule.

**Signed/offscreen handling:** implement mathematical ceil for positive denominators and signed numerators, including negative transition offsets. In C, truncating division followed by `+(remainder > 0)` gives signed ceil; naive `(n+d−1)/d` is incorrect for sufficiently negative n. Promote before adding x+w or y+offset+h, and before multiplying. Use int64 intermediates, clip logical endpoints to the canvas before narrowing, and reject non-positive source extents/empty projected spans. The current admitted dimensions are uint16-bounded; these promoted products fit comfortably in int64. Retain the existing native rotation/reader-flip transformation in fill, applying the new projection once in logical paper coordinates.

**Post-fix acceptance:** rerun a separate, clearly labeled post-fix output directory; preserve this baseline. Require exact 480×800 full-image equality; the legacy 410×700 paint/tap bounds above; full title/icon/slider/tile/battery content at 480×640; no unintended paint outside transformed source bounds; negative/offscreen transition controls; orientation/flip equivalence; and all existing action, cancellation, delayed/snapshot, and custody controls. Preserve the resident decorative border extension.

## Artifacts and limits

- `baseline-receipt.json`: frozen source/SDK hashes, HEAD/tree, no tracked diff, measured rectangles, 19 image comparisons, and final case counts.
- `nonresident-results.json`, `resident-results.json`, `original-control-results.json`: per-case results.
- `nonresident.log`: initial 66-case matrix; `nonresident-extended.log`: final 108-case matrix; `resident.log`: resident build/run attempts and final matrix; `original-controls.log`: unchanged original controls.
- `legacy410-capture/frame-2.png`, `legacy480-capture/frame-2.png`: actual normalized sheet captures.
- `resident480/quick-0.png`, `resident480/quick-7.png`, `resident640/quick-0.png`, `resident640/quick-7.png`: real resident provider captures.
- `generate_fixture.py`, `coordinate_probe.c`, `run_nonresident.py`, `run_resident.py`, `run_original_controls.py`, `summarize.py`: reproducible build-local harness and analysis.
- `harness-attempts.md`: excluded fixture compatibility failures and the honest limit on overwritten early logs. In particular the legacy snapshot-refusal mode 2 did not terminate under the input adaptation and is not claimed as a qualified production result.
- `executable-sha256.txt`: retained executable hashes where collected. Some early binaries were already removed before hash-retention was requested. All reproduction executables were removed; source, scripts, logs, SDK copies, results, and captures remain.

The native workspace itself is the only execution environment used. No current production correction is included here. Baseline must not be rerun against edited production source under the same labels.

## Published evidence subset

The historical artifact paths above identify the frozen local reproduction bundle. This directory publishes its baseline receipt, per-case nonresident/resident results under `baseline-*-results.json`, selected original reference captures, failure captures, and `HARNESS-ATTEMPTS.md`. The later maintained runner and fixed receipt provide repeatable current-source qualification; historical reports are not relabeled as current runs.
