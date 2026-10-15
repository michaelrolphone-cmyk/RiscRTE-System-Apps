# Shared first-poll retention investigation

## Observed hardware failures

The Wave 0.1.43 Workboard logs show successful load and initialization, followed
by `app invocation retained; restart required` shortly after entry:

| App | Entry duration | Evidence |
| --- | ---: | --- |
| Wi-Fi | 327230 us | T051 comment cedd83b9-f32b-49ee-be02-e4396c5cec6e |
| BLE Buttons | 327755 us | T039 comment e9c8bc55-2b59-4cb9-a8ee-10ec42ffab6e |
| Points in Time | 332441 us | task 3d492001-c7f4-4ec2-9b8e-f24393fffe3b serial evidence |

The T037 Touchpad log ends during launch and does not establish its return
reason. T069 SDR has a separate CPU0 IQ-worker idle-starvation defect repaired
in Runtime 0.3.5, commit 4107eaeedd8c801eeaf7deb1c7bc4a943cd27342.

## Shared source defect

The checked foreground adapter lazily opens the Contexts service during its
first poll. The fingerprint extension then attempts optional app-side shared
storage: Audio bank 2, RF bank 3 and Contexts rules 4. The reader already has an
unavailable-storage fallback. However, `custody_acquire_for` converted even an
unmodified, empty denial for those requests into terminal invocation retention.

All three affected 0.1.43 app manifests and boot-policy rows omit these shared
storage grants. Expanding their authority is unnecessary: missing persistence
must leave the ordinary foreground app usable.

The repair admits only that existing fallback, for API 1 and instances 2–4,
with the original empty grant and a still-live, identical native Runtime. A
dirty record, wrong request, revoked Runtime or failed cleanup remains terminal.
The Contexts client checks Runtime liveness before further callbacks. Fault
injection also exposed two retained-success callback edges: a capture callback
could clear the borrowed raster and return true, and a fingerprint CONFIG
callback could retain and still allow step. Both now stop at the custody fence.

## Exact deployed source

The official Wave 0.1.36 Evidence.zip (SHA-256
`5bfdbf1c606c3c9802c7836e6aa40fb9dfd040c97eff4c88f7fa5f6c9dfb87e1`)
contains `final-source-pins.json` and the original T054 source recipes. The
BLE recipe and the Wi-Fi/Points recipe all select public System
`ffe0295eff869f0d33d56f98548f9b435975d584` (`System-product-t054`). The archive's
assembly-report app hashes exactly match those inherited by 0.1.43.

This repair branch starts at that exact source pin. Its Contexts adapter is
byte-identical to the earlier diagnostic reproduction; the failing acquisition
wrapper is unchanged. The exact source already enables the Contexts live
callback guard. The repair changes qualified missing-storage handling and the
two callback boundary checks above. No new grant or authority is added.

The frozen 0.1.44 native-UI release remains separate and does not include rebuilt
affected app ELFs. A successor cumulative image must replace every affected
inherited app binary, in addition to the independent SDR native-runtime repair.

Host fixtures and target links do not constitute hardware qualification. Both
watch products require first-frame, repeated entry/exit, input, background
service, low-battery and sleep/wake testing after cumulative image integration.
Relevant e-ink profiles require the same optional-storage and retention matrix.

## Regression evidence

The production-source runner `scripts/test_shared_data_custody.py` passes
192 checks: 24 cases, asynchronous/synchronous MONO1 paper and native-size
240x240/410x502 RGB565 profiles, normal and ASan/UBSan. Its unchanged deployed
source counterpart reproduces first-poll terminal retention in all eight
profile/mode combinations. Tests cover clean denials 2/3/4; five dirty-grant
variants; wrong API or namespace; revoked identity/health; missing health; failed
release; retained-success capture at raster/input/presentation boundaries; step;
and four fingerprint CONFIG entry points. The ordinary first-poll case goes on
to draw/present, change DND, launch, and clean up.

`regression-summary.json` records hashes and results; complete per-case build
commands and logs are retained with the release evidence. The SDK and Utilities
roots are explicit inputs. No physical watch, e-ink panel or radio was exercised.

Eight basic target ELF baselines (Battery, Calculator, Stopwatch, Frequency
Generator, Alarms, Countdown, BLE Touchpad, BLE Buttons) and Points in Time have
already reproduced their deployed .43 binaries byte-for-byte. Final repaired
links and the remaining inherited cohort are being assembled separately.
