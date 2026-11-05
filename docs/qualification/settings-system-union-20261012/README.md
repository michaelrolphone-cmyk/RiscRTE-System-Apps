# Settings / Files / clock / Watch profile source union

This is a working source checkpoint, not a firmware build or persistence
activation. X4 persistence remains default-off. The non-SD Watch never selects
SD persistence. New Files shared-text work and unavailable-settings UI continue
independently and are not claimed complete here.

## Exact component mapping

- Previous persistence union public `0a9c02f1b9f42577b8a1809fa60f792b1f40c4c3`
  equals local `61d666697b6cd006405d99364a300fa916dc6e0c`.
- X4 alarm Busy adapter public `0cae9c90b74982e784f5e59c655f246dfbc69178`
  from local `5ca128b9102b3a11d8b45994fe6d4fbd8bc87355`.
- Watch verified clock-change boundary public
  `3508092f135d428b5bbccee0546616d046938527`, from local
  `8f831566c51f724955cdc475b4fd3088a913bb1b`. Only the opt-in raw Settings
  editor/contract is carried. Native-time Settings remains independently gated.
- Nearby Files source public `59878efb3aadd22ef3138085568fb3555ea7c553`
  plus `54d25603c3e9610f5e82dab5472e3ece76ab242e`, exact local
  `102f3c64b283a7900eccd78882c4ecc68a9b9f06` plus
  `0e2e4d3dadb06773548a8300a64e2c9e5ec4a936`.
- Opt-in Watch color-only / frozen delivered icon profile public
  `9550949bab67e820aa19984ef7bc20322756feb3`, tree
  `d0dde11e2c8ca168e0e428d346f5fb9ab59242b2`. Both selectors remain off in
  generic/X4 builds. The Scanner and Points app migrations are separate
  Utilities/Productivity components; this checkpoint does not invent new pins.

The tested final source is local `ef974726` (full commit/tree and all tracked
file hashes in `source-start.json.gz`). Settings conflict resolution preserves
both PORTABLE_SETTINGS_NATIVE_TIME exclusion and opt-in clock-change control.
No Files implementation was edited during this composition.

## Checks and limits

`regressions-final.json` records 23 completed normal/sanitizer invocations:
Wi-Fi, Settings, Quick, Home, shared text/scene, alarm adapter and Files
peer/mount/setup/source/local scopes. The opt-in Watch profile contract also
passes all four cases. The initial Files ASan run hit filesystem ENOSPC after
15 passing cases; its log/receipt are retained. After byte-preserving sparse
allocation of completed evidence, the full 45-case source suite passed. No
sanitizer defect was suppressed or relabeled.

The earlier pre-Watch-profile union `0805f765` also passed 80 real Runtime/Graph
resident cases and the actual Files → Runtime 12 MiB+123 source handoff,
receiver invocation and fresh Files return in normal and ASan/UBSan modes,
using Runtime core `98b9d2b08cd6a05f75ec0ca0cd755a63484a2246` for the latter.
These earlier exact-source receipts are included with the `nearby-` prefix;
they are not represented as reruns after the Watch-only profile addition.

Large logs/manifests use deterministic lossless gzip. Decompress to inspect
original bytes and their recorded source/command hashes. Host providers and
SDK/UI/device boundaries remain explicit; there is no physical device claim.
