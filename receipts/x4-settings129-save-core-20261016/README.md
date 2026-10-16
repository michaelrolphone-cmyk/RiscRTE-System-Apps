# Retained X4 Settings RTC/save-core qualification

The reconstructed Settings 1.3.29 editor and source-bound Save core pass 33 cases normally and 33 with ASan/UBSan. Four UI cases enter `settings_activate` once: local basis, UTC basis, missing basis, and an edited year. Each verifies exactly one RTC write/readback, native seed and atomic metadata submission against independent calendar, UTC epoch and complete metadata-byte expectations.

Fault coverage includes RTC acquisition/write/read/release/mismatch, native capability/seed/readback/release, stale revisions before and during clock updates, terminal CANCELLED/UNCERTAIN/IO/CAPACITY receipts, retained/error collection, dirty submit refusal, and unavailable/pending/conflicting storage. Terminal CANCELLED receipt coverage is distinct from user Back/cancel input.

## Scope

The delivered Settings ELF is `1c4914ed12749cb076019dd255c40939d73a6be10f1eda069c4e5d68b48148b7`. Its 3,928-byte native-time block and 11 relocation records match the reconstructed candidate. Three Save functions have matching reachable instruction/control-flow graphs; activation/touch differences resolve to independently matched shifted callees. Complete comparisons, literal checks and observed mismatches are included.

The reconstructed poll loop differs materially from the retained loop. Both the editor and atomic-save loop call poll, so these results qualify the source-bound core and reconstructed editor, while retained whole-ELF first-Save behavior remains unverified. Native providers are checked host doubles. Hardware, target execution, scheduling/timing and alarm firing/output interleavings remain unverified.

The alarm fixture follows the selected adapter's permitted settled/sealed/pending presentation states while forbidding a mutable/provider frame. This is specific to the selected adapter. The unrelated older `zone-touch-cancel` availability fixture has a page-count failure with this raster profile and is excluded from the 33 RTC cases.

## Reproduce

1. Check out System Apps `5b28172e979b5f008da19e9a00a3b98e35937974` and apply `reconstruction-and-tests.patch` with `git apply`.
2. Provide Runtime Git objects `274bc66f193cbe29018d2a85c9400cb0dce8aacc` for native-time SDK, Runtime `a91edf3fada6a559ec107186dd163d0c8da10d29` checked out for resident SDK, and Utilities `e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d` for tagged alarms. Set the variables required by `build.sh`; use Xtensa ESP32-S3 GCC 8.4.0 (2021r2-patch5).
3. Run `sh build.sh`. The symbolized candidate SHA256 is `a9fdc7b2c127c7a880dc44cde9dbb6b3e4af627e37436465959408d3c15b7c90`; the stripped candidate is `f48e2afc599620a9d019cee3dc42d81542654e6370782447b71f714ce66dee42`. Allocated sections and runtime relocations are identical between those two forms.
4. Run `python scripts/test_retained_rtc.py --target "$OUTPUT" --output /tmp/rtc-normal` from the reconstructed System tree; repeat with `--sanitize` and a separate output directory. The runner verifies all 258 production source hashes and 110 effective SDK/time hashes.
5. Run `compare_cfg.py --help` for entry-led instruction comparison. `summary.json` lists function entry pairs and all limitations. Poll mismatches are preserved explicitly.
