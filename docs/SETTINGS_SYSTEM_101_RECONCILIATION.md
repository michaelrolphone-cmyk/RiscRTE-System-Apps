# Experimental settings reconciliation with X4 .101

The disabled SD-first Settings, Wi-Fi, Quick, Home and utility-preference source
is reconciled with the delivered .101 System source. The current shared keyboard
capture/privacy, Files scrolling, completed local-file handoff and existing
OBD/Home changes are preserved. No firmware, package, version or feature selector
is changed by this checkpoint.

Local source `d21aade84b6bc34ba4dddde90872bde011b6c099` combines the prior
experimental checkpoint `472036f0c52e8fc88f0ccefb262e25734e6eda39` with final
public System `57e549ef5a1c727e8a24658e572f402e006fe82d`. The exact .101 baseline
tree was independently reconstructed and matched before integration. The only
overlapping source file, `resident_adapter.inc`, keeps both the Files scrolling
policy inhibit and the Settings atomic-custody checkpoint fence.

## Executed checks

All 18 selected suite invocations pass on the combined source: normal and
sanitized Wi-Fi, Settings, Quick, automatic Home, text input, scene, Files peer,
mounted Files and Files setup. The Python suites use ASan/UBSan; the existing
Files shell runners use UBSan. LeakSanitizer is disabled. Their actual commands,
outcomes, logs and unchanged source hashes are in `receipts/settings-system-101`.

The separate resident-system suite executes the actual .28 Runtime/Graph plus
separate System host/client/provider ELFs, covering Files, Wi-Fi, updates and
USB in normal and ASan/UBSan modes. Providers and physical transports remain
fixtures. These checks preserve scrolling, handoff, cancellation and retained
custody; they do not qualify a physical USB connection or the new native
persistence backend.

Two initial test-setup failures are retained: the text-client invocation needed
the already-required `--decoupled-attention` option, and scene compilation needed
the existing generated telemetry font. The corrected runs use baseline CI's
options and Pillow 12.3.0; generated font SHA-256 matches the pinned value.
No product source was changed to make either test pass.

## Remaining work

The selected controller tests still inject scheduled atomic endpoints. Actual
native source/store/SD qualification is recorded in Runtime's separate evidence.
The qualified production private filesystem, complete native selector/cohort,
target resource/latency measurements and physical power-loss/card-swap checks
remain open. This checkpoint is source/test integration, not product enablement
or a claim that the everyday SD-first path is complete. Watch receives no SD
policy. Independently developed RTC and Nearby Files changes require subsequent
reconciliation before any combined product build.
