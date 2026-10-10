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
- 170 presenter/profile regression cases pass normally and with ASan/UBSan.
- 100 actual Runtime/Graph/ELF text/scene cases pass normally and with ASan/UBSan against the exact installed native .100 source.
- 75 source-bound cadence/raster cases pass normally. These include 20 format/rotation and four NOVA-7 layer rasters identical to the sealed .53 baseline, overlap/layer/gap/Home cases, and 48 logical cadence cases with identical action/timestamp hashes under 0, 16, 120, and 2300 ms presentation.
- Cadence covers 600 inputs at 200 ms and faster 133/80/60 ms bursts with short 20 ms contacts. The fixture supplies physical edges; it does not replace the required GT911 register-level proof.
- A fresh Xtensa S3 target compiles with exactly the existing generic imports; it is intermediate, not image-ready.

Unfinished: sanitizer cadence rerun, exact installed GT911 .1.9 source recovery/reconstruction and physical cadence tests, integration with the universal adapter/native scheduling work, and clean product rebuild. No measured hardware latency or universal end-to-end claim is made. Existing docs/receipts from the restored baseline remain historical and do not qualify this new code.
