# Watch presentation adapter reconciliation

Task2b8bd89c / execution f33e30ba, original8 points unchanged.

The selected published System source a24bf10 already supplies a common semantic ui.scene provider and separate presentation profiles. Its target builder selects watch-compact (240px optimized RGB565; telemetry absent) or color-dynamic (reported dimensions; telemetry absent). These are provider choices, not board-specific application binaries. The full provider remains available for richer capability needs; a narrow profile must not silently claim telemetry it omits.

Actual retained Watch and Waveshare package manifests require display.output, input.touch.raw, platform.clock and ui.presentation-profile, but do not require input.navigation. Existing host fault/lifecycle tests always supplied navigation. The new focused probe closes that specific coverage gap without rewriting the provider or replacing prior fixtures.

## New measured result

42 host executions passed:21 normal,21 ASan/UBSan. The unchanged real provider is compiled using target builder flags. Watch compact is tested at240x240 and explicitly rejects incompatible466x466 geometry without acquiring touch/display custody. Dynamic color is tested at240x240,466x466 and368x448. Each admits touch-only navigation, survives an input gap, and retains busy display custody until close can finish. Each required dependency omitted in turn causes startup refusal without hardware I/O. Existing fixture asserts guard bytes, serialized ownership, no I/O after retention and settled teardown.

The initial probe incorrectly assumed submit count equals explicit release count. The real display contract transfers custody at submit; release counts unsubmitted cancellations. That new-probe assertion was corrected, and its failed result is retained in initial-probe-failure.json. It was not a production defect or waived ownership failure.

## What this does not establish

This verifies selected published source and its declared builder profiles, not that every installed product's historical provider ELF is byte-identical to the rebuilt host source. No new target ELF, installation, hardware interaction or physical acceptance is claimed. Host shared-controls rendering still follows the separately tracked resident/default integration. The new custom canvas capability remains a separate pending contract/implementation dependency; none is pretended to be implemented by ui.scene tests. Optional telemetry absence is real and must remain visible to consumers.

## Next integration

Keep original scene host matrix and target profiles. Add this touch-only/required-absence matrix as an independent test runner with exact selected source provenance. Retain remaining custom-canvas/provider conformance work under the existing contract dependency rather than closing the broad adapter task from this subset.
