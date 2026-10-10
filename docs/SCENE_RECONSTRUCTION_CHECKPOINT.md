# Scene 0.1.4 working checkpoint

This is an unfinished reconstruction after the 2026-10-10 execution workspace was replaced. It is not byte-identical recovery of the previously local e282a497 implementation. No new hardware image, release, tag, or production-readiness claim is made.

Baseline bytes and modes come from the X4 .53 Library source archive `libfile_54bf28aad1848191b2de20baf9287582`, keyboard snapshot SHA-256 `3f7db7a3e7a2d1c0288c5f936f563aaed2eeffefb2c461e24ce7526aba555515`. Original System source identity was `8df929100d32440990e1fd7b62be7a2a2a8c9a0b`, tree `7a6cb6d48a7bb2684dc89af9ff4f6ab9e402adba`. That archive did not contain Git objects; this checkpoint preserves recovered source under its normal paths with explicit new Git ancestry.

Implemented so far:
- logical hit maps and application actions do not wait for display completion;
- immutable visual snapshot and eight-logical-row bounded raster slices;
- physical edge capture before and after raster work;
- queued edges processed in order across logical layer acknowledgements;
- overlapping keyboard contacts commit in DOWN order;
- hardware-keyboard attachment and session boundaries retain real input fences;
- display custody still settles before provider unload.

Fresh post-reset checks on this reconstruction:
- 173 presenter/profile regression cases pass normally and with ASan/UBSan.
- 100 actual Runtime/Graph/ELF text/scene cases pass normally and with ASan/UBSan against the exact installed native .100 source.
- 75 source-bound cadence/raster cases pass normally and with ASan/UBSan. These include 20 format/rotation and four NOVA-7 layer rasters identical to the sealed .53 baseline, overlap/layer/gap/Home cases, and 48 logical cadence cases with identical action/timestamp hashes under 0, 16, 120, and 2300 ms presentation.
- Cadence covers 600 inputs at 200 ms and faster 133/80/60 ms bursts with short 20 ms contacts. The fixture supplies physical edges; it does not replace the required GT911 register-level proof.
- A fresh Xtensa S3 target compiles with exactly the existing generic imports; it is intermediate, not image-ready.

Exact installed GT911 .1.9 source is recovered at b87bcf9cc4c35f273e67eb10aca588ab821da1d1 (driver SHA-256 da287cbd675b6a59131b97278a48c6cf1734b90e30a9e2674e72b0063c9a86d1). Final actual Runtime/Graph plus unchanged GT911 register-model qualification passes 72 cases for each of MONO1 and RGB565, both normally and with ASan/UBSan. This covers 600 sustained nominal 60 wpm inputs, 180-input 133/80/60 ms bursts, varied 10–25 ms contacts, same-key/alternating-key and overlapping tracked fingers. Independently scanned 5 ms reports latch until STATUS ACK; display completion is 1/17/2300 ms. Modeled per-pixel costs are 0/500 ns. All 82,080 contacts are delivered in order; largest modeled raw polling gap is 5,632 µs.

The unchanged .53 negative control reproduces loss under the same explicit 500 ns/pixel load: 274/600 alternating and 312/600 same-key nominal 60 wpm contacts, with a 350,992 µs maximum poll gap. These values are deterministic test loads, not hardware measurements.

Frozen production source: a0dddaeaefd10fdddfadd386da9cd7416d592779. All 173 presenter/profile, 100 actual native .100 Runtime text/scene, 75 logical cadence/latest-raster cases pass normally and with ASan/UBSan. A fresh clean Xtensa target and both existing profiles pass ELF/import validation. Full structured evidence is in receipts/scene-014-reconstruction-qualification.json; runners preserve exact commands, input hashes and failures. Product assembly and hardware validation remain separate.

Recovery scope: no component-guide styling or animation policy changes were applied. The existing NOVA-7 styling and smooth UI policy are preserved. One additional qualification-driven refinement was made after recovery: nonkeyboard multi-contact ambiguity cancels its own timestamped report, rather than erasing a completed earlier report. This is covered by separate regression cases. Existing docs/receipts from the restored baseline remain historical and do not qualify new code.
