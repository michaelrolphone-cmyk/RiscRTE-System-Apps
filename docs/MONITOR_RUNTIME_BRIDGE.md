# Monitor through production Runtime: bounded host bridge

## Initial source delivery, not a qualification result

This isolated test-only suite is stacked from System PR157 exact head
`0dd03a6b65c4e643081d8a2d8e175f08b5c02c23`. It is captured before qualification;
expected assertions/counts below are gates, not a claim that any run passed.
A passing result requires the exact remote System head, clean source trees,
all 36 distinct cases in each normal and ASan/UBSan lane, and verified receipts.
Initial failures and later fixes remain separate commits/runs; no history is
replaced. PR157 source, tests, evidence and failed runs remain untouched.

## Actual composition and its boundary

The suite compiles unchanged `Apps/serial_monitor.c`, the native PortableApps
adapter and PortableSerialClient. Thin test-only lifecycle wrappers expose the
actual paired module init/fini and app_main implementations to the production
loader while recording entry/return/fini/unload. Compilation renames only those
implementation symbols; source bytes remain identical to PR157. No Runtime API,
capability acquire/release, or stream-client function is stubbed. A dynamic
symbol gate requires the app's unresolved `risc_runtime_get_api` to resolve
from the real Runtime host executable.

Runtime is pinned to `6a3b3d46f5b2857eacc6e9863799dec1f466b036`. Its loader,
provider graph, session broker, retention fence and provider queues are compiled
from source. Canonical app/driver/hardware headers overlay a temporary SDK copy;
all canonical header and production source hashes enter the receipt. No bundled
SDK or production source is modified.

Display, raw-touch, battery and two independent serial modules are explicit
software providers. Their test-only manifests/grants are generated into each
case directory outside both source trees. The required metadata architecture
string is not a target ELF/build claim: these are host shared objects. Serial
instances 41 and 42 require separate fixture gpio.bank hardware envelopes only
to exercise production exact-instance materialization. They never access those
pins or real hardware. The app catalog is also fixture-only and is not product
routing/admission.

The serial witness produces 31 unsolicited RX bytes, in chunks of at most 7,
through the real provider queue, without consuming/echoing TX. It checks the
selected device 17, generation 9 and framing, and logs any forbidden raw-prefix
fallback. RX text must appear as the exact `RX COMPLETE` glyph rectangle in the
actual monitor raster, not merely as a nonblank frame or logged receive count.

The display provider gives the real adapter an asynchronous RGB565 buffer. It
checks guards and the entire frozen buffer at pending/completed status calls;
the slow lane holds each submitted frame for 600 simulated milliseconds. RX
must continue while a frame is pending. Both 240x240 and 480x800 are covered.
These are simulated times and software buffers, not target timing or panel tests.

## Bounded matrix

Nine modes × eager/demand provider activation × two dimensions = 36 cases per
lane, 72 across normal and ASan/UBSan:

- normal: select explicit instance 41, receive/render, close, then exit;
- select-second: explicitly select/grant 42 rather than falling back to 41;
- slow-frame: immutable asynchronous frame with concurrent queue RX;
- reopen: close via Back and reopen a fresh session before final close/exit;
- missing-grant: serial declared optional but not granted, no session opens;
- wrong-grant: grant only 42, select 41, no automatic fallback/session open;
- close-retained: provider reports retained close;
- call-overrun: checked session callback exceeds its real wall-clock budget;
- close-overrun: provider returns successful close after its budget expires.

Overrun uses real host sleep because production broker timing uses steady_clock,
not the fixture's simulated UI clock. The existing broker detects elapsed time
after callback return and retains custody; it cannot preempt a blocking callback.
No new retention mechanism is introduced. At the provider's terminal observation
boundary no subsequent provider/UI/fini/unload callback is allowed. The retained
Runtime is not destroyed; process isolation ends the test without representing
that exit as safe recovery, physical quiescence or product teardown.

Normal cleanup must close the session, run paired fini, unload the app and leave
zero queue allocation. Retained queue allocation must remain exactly 256 bytes (two 128-byte queues),
including a late successful provider close whose broker pins remain retained. LeakSanitizer
is explicitly excluded (`ASAN_OPTIONS=detect_leaks=0`), consistent with the existing
ptrace-restricted host qualification. ASan/UBSan remain enabled in the sanitized
lane with no recovery; no leak-qualification claim is made.

## Reproduce and interpret evidence

Use clean sibling System and Runtime checkouts at the exact published head and
pinned Runtime commit, then install Pillow 11.3.0. Output must be a new directory
outside both trees:

```sh
PYTHONDONTWRITEBYTECODE=1 python System/scripts/test_monitor_runtime_bridge.py --runtime Runtime --output evidence-normal
PYTHONDONTWRITEBYTECODE=1 python System/scripts/test_monitor_runtime_bridge.py --runtime Runtime --output evidence-sanitized --sanitize
```

`--development` permits work-in-progress smoke testing but can only emit
`DEVELOPMENT_PASS`, never qualification PASS. `--case` is development-only.
After a valid external output directory is reserved, preflight, staging,
compilation and test failures preserve an UNQUALIFIED receipt with failure type
and message. Build/test failures additionally preserve their command log. Invalid
CLI arguments, unsafe output paths and inability to create the output directory
cannot guarantee a receipt; their console error and missing receipt are never
success. Each CI lane also runs two isolated negative controls: a wrong Runtime
checkout and an injected staging-copy failure must leave zero-case UNQUALIFIED
receipts. These are runner controls, not additional monitor cases or production
mutations. CI checks out the exact PR head, never the synthetic merge. Both lanes
run independently.
Artifacts preserve logs, generated manifests, binaries, symbols, rasters, source
hashes and receipts even on failure, with an exact head/run/attempt/lane name.
They expire after 90 days. Download and verify SHA256SUMS before citing artifact
bytes. Source in GitHub is durable delivery; artifact retention is not permanent
archiving, and a passing host suite is not product acceptance.

## Unchanged open product gates and cohort

This suite supplies neither native USB discovery/role scheduling nor real USB
providers. Drivers PR40 usb_host_v2 has no poll suffix; CDC consumes an already
published inventory. No test-only discovery helper is inserted into the monitor.
Runtime's trusted controller 0.1.25 is not widened to the separately qualified
0.1.27 candidate. Existing Home/return/resident/sleep/Quick composition guards
remain intact. No accepted bytes, product pins, routing, release, firmware,
merge, hardware or device actions change.

Relevant unchanged references:
- Drivers PR39 `05abdee8311c5846087580fdf9a6d2e500a85061`;
- Drivers PR40 `90e7a8faae1a6678a24c92c32f95032b2fbe1e24`;
- Reader declaration `c98e4222048244b1e729869ed47aa99130025fd3`.

Native provider composition, product placement/routing/grants, target ELF/timing,
exact trusted controller admission, source parity and electrical/physical USB
acceptance remain separate gates. This suite closes only the host evidence gap
between the unchanged monitor and the real Runtime machinery when its exact-head
qualification succeeds.

## Qualification correction after initial capture

The initial source capture compiled the Runtime host at O1. Local GCC14.2 and
CI GCC13.3 sanitizer compilation stopped before cases on ArduinoJson
maybe-uninitialized warnings.
The follow-up runner explicitly builds only the Runtime host at O0, matching
the pinned Runtime runner; app/provider modules remain O1. Strict warnings and
ASan/UBSan stay enabled. No diagnostic is suppressed and no dependency source
is edited. An O0 host pass is not equivalence to an optimized O1 host pass.
The fixture also asserts exact 115200/8N1/no-flow-control framing instead of
merely accepting any valid serial configuration. Initial source and failed
compilation evidence are not erased by these corrections.
