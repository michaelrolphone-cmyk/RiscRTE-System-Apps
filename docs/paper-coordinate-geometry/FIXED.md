# Paper coordinate correction verification

175 fixed cases pass: 108 nonresident, 36 resident, 31 unchanged original clock/launcher. All 30 pixel/reference comparisons pass. The frozen baseline receipt remains SHA256 `0f16dc7b6b947a58e075f813ad491138b822b443e44e2ba99a1a66686045cd3a`.

Measured actual provider outcomes:

- Legacy 410×700 first tile now paints `[28,202) × [265,345)`, exactly matching unchanged accepted press/release bounds. No premature writes; all outside-edge assertions, actions, cancellation, and exact background restoration pass.
- Resident 480×640 content recovers to 45,863 black pixels without audio and 56,601 with audio, from 2,080 in either baseline profile. Every single output pixel equals inverse-floor row sampling of the corresponding frozen 480×800 resident reference. Individual collapsed source rows are accepted by that explicit sampling contract; no minimum-one expansion is used.
- All 480×800 legacy and resident full-frame comparisons are exact, including the resident's intentional two-pixel decorative border extension.
- Display 90-degree orientation, reader flip, native raw-touch 180-degree rotation, raster snapshots, ASan+UBSan, delayed 75 ms immutable presentations, terminal/focus-failure custody, Home, USB action, and original modal controls remain green.

The first resident build exposed an unused `qp_x` static function after rectangle projection stopped using it. The parent's nonresident definition guard resolved that compile error without warning suppression. The identical resident matrix then passed. The nonresident matrix was repeated after the guard so every final successful compiler closure uses the same production source. The pre-guard run and failed build remain in the logs; they are not mixed into the final 175-case total.

## Durable evidence

- `fixed-receipt.json`: final maintained-run 175 cases, 30 comparisons, all 29 compiler dependency closures, staged SDK hashes, runner/workflow/fixture hashes and removed executable hashes.
- `fixed-test.log`: final maintained build and test output.
- `pure-results.json`, `pure-test.log`: signed, clipped and extreme-coordinate properties under normal and ASan/UBSan builds.
- `original-results.json`, `original-test.log`: 124 unchanged original paper cases and two Home ordered-input runs; optional captures were omitted in that separate historical run.
- `fixed-410x700.png`, `fixed-resident-480x640-no-audio.png`, `fixed-resident-480x640-audio.png`: final actual provider captures.
- `sdk-source.json`: exact external repositories, commits, header paths and SHA256s.
- `target-probe-results.json`: no-import freestanding Xtensa probe, unchanged ELF admission, and both compiler dependency closures.
- `target-results/`: the four actual target builders, logs and executable hashes after the wide-division correction.
- `target-admission-failure/`: preserved original-head first-rejection witness and artifact identities.
- `summary.json`: source/artifact hashes and qualified component limits.

The maintained runner is `scripts/test_paper_coordinate_adapter.py`, with four separate `test/native_apps/paper_coordinate_*` fixtures. Existing fixture files remain unchanged. It accepts explicit Runtime/display SDK paths and records compiler non-system dependencies. Frozen reference assets are checked against `baseline.json` before compilation and pixel comparisons. Builds run sequentially; only generated executables are hashed and removed.

The earlier scratch matrix and its pre-guard compiler failure remain historical local receipts. The final durable receipt is generated afresh with the current source and default committed baseline assets. There is no hardware or whole-app acceptance claim. The same Quick task retains small paper-label visual acceptance and the separate paper alarm horizontal input-boundary decision.
