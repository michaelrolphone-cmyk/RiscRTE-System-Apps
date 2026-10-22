# Bounded native USB serial monitor qualification

This additive CI/documentation successor starts at the exact PR #150 head,
`bda5b6108243c21a12d682755a6a2da87402b15e`. It reruns the existing receive-only,
unmanifested monitor candidate. It does not transplant the monitor into current
main or change production source, SDK/ABI, product pins, accepted baselines,
admission, manifests, target policy, hardware, firmware or releases. PRs #150,
#155 and #156 remain unchanged. There is no merge authorization in this work.

The original monitor source is
`5843d1bf2079e66f837aec5eb03a1f15b9a95ae7`, captured on GitHub as
`3babc8fd5d683e537bf85290c74ce9d78234803a`, tree
`14c2b4e7a1566ea2910d7783864d969677280235`. PR #150 adds historical evidence
after that source. See [the existing behavior and boundaries](NATIVE_SERIAL_MONITOR_UI.md)
and [the client contract](PORTABLE_SERIAL_CLIENT.md).

## Exact-head workflow

`.github/workflows/usb-serial-monitor.yml` checks out the actual PR head, not the
synthetic merge commit. System and Runtime occupy separate, clean sibling trees;
all generated output stays outside them. Runtime is fixed at
`6a3b3d46f5b2857eacc6e9863799dec1f466b036`. Both runners verify that checkout and
stage its canonical headers without changing the bundled SDK or Runtime.

The workflow installs Pillow 11.3.0 and runs both existing scripts in normal and
ASan/UBSan lanes. `ASAN_OPTIONS=detect_leaks=0` is explicit in every lane;
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` prevents sanitizer recovery.
LeakSanitizer is not qualified. A failing lane does not prevent the other lanes
from being attempted, and failures remain failures. There is no automatic retry
or overwrite of old artifacts.

The final receipt gate checks actual commands and logs for 25 controller cases,
20 adapter invocations (10 cases at each of 240x240 and 480x800), nine expected
composition refusals, and 22 distinct production Runtime client cases per lane
(11 eager and 11 demand). These are expected gates, not a pre-run success claim.
The existing client unit, opt-out and C++ header checks also execute. Source,
receipt log and raster hashes are retained alongside each command transcript.

Every run uploads an exact-head/run/attempt-named artifact for 90 days, even on
failure. It contains logs, receipts, captures, contact sheets, source/tool
identity, a successful-lane summary when all gates pass, and SHA256SUMS. A
missing receipt or summary is not a passing result. Download the artifact from
the exact run before citing its bytes; preserve the downloaded ZIP hash and
verify the internal hashes. The archive extracts with an evidence-relative
layout; run its SHA256SUMS check from a directory containing it as `evidence/`.

## Local reproduction

Use clean sibling checkouts named `System` and `Runtime`. Check out this
qualification head in System and the exact Runtime commit above. From their
parent directory:

```sh
python3 -m pip install Pillow==11.3.0
export PYTHONDONTWRITEBYTECODE=1
export ASAN_OPTIONS=detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
python3 System/scripts/test_serial_monitor.py --runtime Runtime --output evidence/monitor-normal
python3 System/scripts/test_serial_monitor.py --runtime Runtime --output evidence/monitor-sanitized --sanitize
python3 System/scripts/test_portable_serial_client.py --runtime Runtime --output "$PWD/evidence/client-normal"
python3 System/scripts/test_portable_serial_client.py --runtime Runtime --output "$PWD/evidence/client-sanitized" --sanitize
```

The client output path is absolute because that runner invokes its compiled
programs from the System tree. Do not place Runtime or evidence inside System:
the clean-tree requirement includes untracked source-tree files.

## Evidence boundaries and still-open gates

The monitor adapter fixture uses fake Runtime/provider functions. It exercises
the actual native adapter, monitor and client, including retained custody,
immutable asynchronous display frames, input and cleanup. It does **not** load
the full monitor through production Runtime admission.

The separate client suite loads the production client through the pinned real
Runtime broker, provider graph, module loader and queues with a software serial
witness. Passing those 22 cases does **not** bridge the monitor's fake Runtime
fixture into full monitor-through-Runtime qualification or product permission.

Still open: target IDF/Xtensa/ELF build and callback timing; safely scheduled
polling/discovery and role integration; utility placement and product routing,
explicit instance grants, launch composition and admission; authoritative
electrical/VBUS/role-root evidence and physical USB/input acceptance. Host
rasters are review evidence, not device acceptance. No flashing, hardware
access, release publication or merge is performed by this workflow.
