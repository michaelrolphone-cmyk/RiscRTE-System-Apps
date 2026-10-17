# Admitted paper alarm coordinate composition

## Result

The existing Settings build flags admit a retained 480×800 MONO1 display without `PORTABLE_WATCH_COLOR_ONLY`. Its alarm is painted and hit-tested in native paper coordinates. The native-height input branch incorrectly projected native samples into the 240-unit legacy RGB space. The narrow correction applies that projection only to legacy RGB; drawing, native RGB, physical buttons, token handling and input ownership are unchanged.

Reproduction source: `9d2c55b32e88c0b12b1fa16200f399164767f5d1`.
Publication parent: `c07f81dab34318659bf27f139e9668354d681ba3`, tree `6656c464c5c7842c928986a93b5701fd0a6e56a4`, preserving the qualified Battery merge.

## Unmodified controls

Four unchanged Clock scenario 13 runs toggle native-height and alarm-native. Both height-off controls pass; both height-on controls fail ACK/restoration. An actual Settings application with provider-only doubles reproduces the same four outcomes. Admission succeeds in every row; the painted button is `(32,612,416,88)` and its top edge has 416 black pixels. The same raw `(240,650)` release gives three frames, one exact-token ACK and exact restoration only when height is off. Height-on gives two frames and no ACK. All lifetimes end with zero retained frames, grants, subscriptions and launches.

The original failures remain in `baseline-clock.json` and `baseline-settings.json`. Initial incomplete RTC-provider and reversed-bit pixel-oracle observations remain separately in `fixture-corrections.json`; neither was treated as production evidence.

## Corrected controls

- 16 Clock/Settings × height × alarm-native × normal/ASan+UBSan configurations pass.
- All 24 Settings before/alarm/restored frames exactly equal the known-passing legacy baseline bytes. The maintained runner pins those SHA-256 values independently of the candidate.
- 16 paired baseline/candidate RGB configurations pass, totaling 1972 existing modal cases. Legacy 240×240 and native-height 410×502 controls retain projection. Native RGB retains all seven existing sizes. Input boundaries, stale/exact tokens, retry/back, uncertain output, delayed restoration, snapshot allocation failure and custody are checked. Corresponding render hashes agree exactly.
- All 20 selector-off preprocessed-token comparisons pass. The frozen pre-alarm baseline is verified by exact Git blob hashes. Only the fixed literal `baseline-guard.patch` is normalized; its own file hash and the exact old/new guard hash are retained. This does not compare the candidate against a copied current guard.
- Compiler dependency hashes and binary hashes are retained; disposable executables were removed one at a time after hashing. `source-closure.json` verifies every unchanged paper dependency against the current union tree.

## Repeat

Run `python scripts/test_paper_alarm_composition.py --output build/paper-alarm-composition`, `python scripts/test_paper_alarm_rgb_controls.py --output build/paper-alarm-rgb-controls`, and `python scripts/test_alarm_native_optout.py --output build/alarm-native-optout`. The existing alarm workflow runs all three plus its complete existing regression matrix. Optional `--baseline-root` on the RGB runner pairs an independent frozen source checkout; provider-only generated fixture substitutions are explicit and source-matched.

This is host qualification. No physical fixture, device installation, or hardware success is claimed.
