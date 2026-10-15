# Shared watch source union, 2026-10-15

This qualification combines the launch-brightness producer 24270c36 and native Quick b9feba2b with the existing main ancestry b3e11dae. The qualified code tree before adding this evidence is 3f85c6204d707b5971205aa26eebb564c192eb57. Product release pins remain unchanged: this merge does not silently rebuild or replace released firmware.

## Results

All required `build.yml` gates pass locally: 119 unit tests, every UI/provider fixture in that workflow, and all target commands producing 39 structurally validated ELFs. The broader review ran 38 suites: 36 pass; two unchanged optional Context assertions also fail on both original parents. `qualification.json` records final selections, commands, hashes and output ELFs; raw attempt receipts preserve setup failures instead of hiding them.

Passing coverage includes 126 initial-brightness cases, 192 custody cases, 632 RF native-frame comparisons, native Quick normal/sanitized geometry, 271 legacy Quick raster comparisons, text handoff continuity, Files policy/legacy normal and sanitized cases, and both Home automatic and Quick atomic normal/ASan/UBSan runs. LeakSanitizer alone is disabled because this executor's tracing is incompatible with it. ASan and UBSan remain enabled.

## Merge decisions

- Preserve the generic adapter's first successful COMPLETE brightness restoration and terminal custody fences. Native Quick's six layout/render files are byte-identical to b9feba2b, independently reviewed. The generic adapter continues using the legacy 240 canvas and capability-to-240 input mapping; native Home sets its own dimensions.
- Preserve automatic Context settings ownership and the legacy storage path, with the common callback-null guard after the custody checks.
- Preserve text-input exit-pending, file-browser ownership and handoff fences together; retain Home automatic inline handling and optional frontlight-tone guards.
- Retain both product-policy and native-package-feed builder options; retain Wi-Fi 1.1.30. Remove an unreachable older version return.
- Reconcile the broadcast profile unit test with its existing Wi-Fi 1.1.30 composition; explicitly reject the stale idle-only version. Add the pinned Runtime checkout and SDK environment required by the already-present native Springboard fixture. No test assertion was weakened to conceal a production regression.

## Known pre-existing failures

`scripts/test_context_client.py` fails at `context_client_test.c:93` on the one-second policy reload expectation. `scripts/test_context_x4_adapter.py` fails at `context_x4_adapter_test.c:108` on `capture_calls > before + 700`. The exact assertions reproduce independently on both 24270c36 and b9feba2b; parent source IDs and logs are included. These optional legacy fixtures need a separate expectation review. Neither assertion was removed or changed in this merge, and the merge is not claimed to pass every optional repository test.

## SDK setup and reproducibility

The normal launcher suite uses Runtime 4107eaee `sdk/app`; Files policy uses the complete Runtime 64c8e52e fixture and Utilities cc56de34. Quick atomic requires the complete generated X4 paper SDK, not only its optional-header directory. Its staged SDK combines that base with AlarmServiceV2 from Utilities and AlarmServiceV1 from this System tree; all header hashes are recorded. The frontlight header is byte-identical to canonical X4 commit 3ac013bd. Initial incomplete-SDK attempts are retained in raw receipts and superseded by the passing v7 runs.

The command scripts record the qualification workspace paths. Substitute local checkouts of the stated revisions and output paths when replaying. No hardware was exercised here. Intermittent device behavior still requires testing Wave 0.1.48 / T-Watch 1.1.39 and a fresh launch-to-return serial trace if a blackout remains.
