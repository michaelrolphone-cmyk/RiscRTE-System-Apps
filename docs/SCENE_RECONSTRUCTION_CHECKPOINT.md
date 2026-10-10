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

Observed checks on this reconstruction: warning-clean host compilation against exact native .100 SDK; 20 format/rotation and all four NOVA-7 layer rasters match the sealed .53 bytes; 48 logical cadence cases have identical action/timestamp hashes under 0, 16, 120, and 2300 ms presentation, including 600 inputs at 200 ms and faster 133/80/60 ms bursts. This cadence fixture supplies physical edges; it does not replace the required GT911 register-level proof.

Unfinished: regression fixture updates for intentionally changed visual-gating semantics; sanitizer reruns; actual Runtime integration; exact installed GT911 .1.9 source recovery and physical cadence tests; target ELF build and exact imports. Existing docs/receipts from the restored baseline remain historical and do not qualify this new code.

Remote checkpoint status: this first commit contains changed source/test paths only, atop the older public declarative branch. The exact recovered .53 baseline is being published separately. This partial commit is not a standalone build/qualification pin.
