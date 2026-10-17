# Exact retained X4 Settings 1.3.29 reconstruction

The rebuilt stripped Settings ELF is byte-for-byte identical to the retained deployment artifact: 153,132 bytes, SHA-256 `1c4914ed12749cb076019dd255c40939d73a6be10f1eda069c4e5d68b48148b7`. `cmp` succeeds and every file-backed ELF section is identical. The symbolized companion has SHA-256 `950c0b9554d571e7d7e89ca25aa5e8dc2e137f3712691e7f91874224d0c2a5fa`.

## Historical poll source recovered

The earlier reconstruction combined the retained adapter with three later poll behaviors. Restoring the historical behavior resolves the complete binary, without patching ELF bytes or adjusting a hash:

- No `cooperated` tracking or its additional final yield condition.
- No special one-millisecond slice for a sealed raster awaiting submission.
- Clear `resident_controls_requested` unconditionally after the controls checkpoint.

The restored `poll_input` is identical to System Apps `5b28172e979b5f008da19e9a00a3b98e35937974` after whitespace normalization. The first two differences explain the polling loop mismatch; the third explains the extra resident BUSY branch. Once all three are restored, the incidental reordered gesture predicate and shifted branches also disappear. The complete entry-led poll comparison visits 1,254 instruction nodes with zero mismatches. Whole-file equality is the stronger binding.

The local immutable production restoration checkpoint is `abb460e1aa1cc1339b3ba871960fd9472b7da6d4`; the source plus corrected host fixture is `ef6294bdfb40a20ba6c4a6daa55c07ad129097fc`. These identify reconstruction checkpoints, not claims about an original historical build commit.

## Host test fixture correction and results

All 33 RTC/save cases pass normally and all 33 pass with ASan/UBSan against the exact reconstructed production source. An independent rerun reproduced both 33-case passes and complete retained ELF equality; its review accepts the software criterion within the existing hardware waiver. Source and effective SDK hashes are checked by the runner. Four UI cases enter `settings_activate` once (local basis, UTC basis, missing basis, and edited year), and verify one RTC write/readback, one native seed, and one atomic metadata submission against independent calendar, epoch, and metadata-byte expectations.

The original fixture failed its poll guard with this exact source. Its button script used the raw touch sample count as navigation time; raster capture-only work sampled touch without consuming a navigation frame, silently skipping scheduled UP presses at samples 5 and 9. Only CONFIRM at sample 13 reached navigation, opening the year editor rather than selecting Save. The corrected fixture gives the navigation provider its own ordered frame stream. The change is isolated to `retained_rtc_save_test.c`; no production behavior or success assertion is relaxed. The original failure logs are preserved alongside the passing results.

This qualifies exact-source reconstruction and host software behavior. Providers are checked doubles. It does not establish execution on the target, physical RTC/device behavior, hardware scheduling/timing, or alarm firing/output interleavings. The previously excluded unrelated `zone-touch-cancel` fixture is not added to this suite. The suite has four first-Save UI cases and 29 direct Save-boundary cases; terminal CANCELLED receipt injection is distinct from user-input cancellation. ASan leak detection is disabled, so no leak-freedom claim is made.

## Reproduce

1. Check out System Apps `5b28172e979b5f008da19e9a00a3b98e35937974` and apply `reconstruction-and-tests.patch` with `git apply`. This is a complete patch from the base, not an incremental patch requiring the earlier receipt.
2. Provide Runtime Git object `274bc66f193cbe29018d2a85c9400cb0dce8aacc` for the native-time SDK, Runtime `a91edf3fada6a559ec107186dd163d0c8da10d29` checked out for the resident SDK, and Utilities `e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d` for tagged alarms. Use Xtensa ESP32-S3 GCC 8.4.0 (esp-2021r2-patch5).
3. Set the environment variables required by `build.sh`, optionally set `RETAINED_ELF` to the retained file, and run `sh build.sh`. It asserts the stripped ELF hash; with `RETAINED_ELF`, it also compares the complete files. `CATALOG_LAST` is empty; the catalog retains its original source order.
4. From that System source tree run `python scripts/test_retained_rtc.py --target "$OUTPUT" --output /tmp/rtc-exact-normal`, then repeat with `--sanitize` and a separate output directory.
5. Optional CFG audit: `python compare_cfg.py --objdump "${NATIVE_APP_CC%-gcc}-objdump" --target "$RETAINED_ELF" --candidate "$OUTPUT/settings.elf" --target-entry 0x1176c --candidate-entry 0x1176c --output /tmp/poll-exact.json`.

`historical-poll.patch` and `ordered-navigation-fixture.patch` separately document the changes from the prior qualified source. Earlier scoped source/core evidence remains valid as historical evidence; its explicit poll mismatch is resolved by this new receipt.
