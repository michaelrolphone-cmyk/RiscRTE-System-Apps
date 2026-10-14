# Deployed RGB Quick and automatic preference checkpoint

Ports explicit Quick USER saves and low-battery/claimed Contexts AUTOMATIC writes into the deployed RGB adapter while preserving its optional-service retention, native-height layout and tap calibration lifecycle. Hardware selectors remain WiFi 15 and Bluetooth 16. No generic legacy write is promoted to USER intent.

Copied saves suppress new edits until collected. An observed raw neutral boundary during the wait allows the next distinct contact; a still-held contact remains gated. A battery crossing discards all queued stale previews before reloading confirmed settings.

Validation: 68 ASan/UBSan executions across 240x240 and 410x502, rotations 0 and 180, using the actual Settings/Quick adapters. Includes timer Save/cancel/reentry, same-value USER intent, failed save/reopen, five-write automatic low-battery policy, manual override, ordered gestures and held-contact suppression. Existing default-profile Quick and low-battery suites pass. Native-height suite passes 190 cases including byte-identical original 240-pixel Settings frames. Full deployed Settings target recipe compiles with both atomic selectors.

Not yet a firmware release. Contexts automatic boundaries compile in the full target profile; their existing policy tests remain separate from this new real-controller matrix. Candidate-specific product composition, exact writer registration, native builds and preserving-update verification are still required. No hardware testing occurred.
