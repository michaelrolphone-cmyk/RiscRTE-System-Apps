# Native app-owned resource custody

The X4 native Wi-Fi Settings, Files, App Store and Firmware Update controllers
use an app-local Runtime facade. It preserves exact requested capability/API/
instance bindings and records the returned token until checked release clears
it. No Runtime interface, manifest grant, provider contract, version allocation,
or Watch/default runtime selection changes.

An external acquisition failure, even with an empty output, malformed token or
required interface, failed release, uncleared release output, or changed release
size retains the invocation. Key-value reads/writes share the adapter's existing
checked result handling: CONTEXT and unknown results retain; documented ordinary
errors remain checked UI failures. The telemetry storage barrier is shared and
its own policy transaction bypasses recursive pause/acquire calls.

App Wi-Fi uses its ordinary provider table, so JOINING and UP remain valid.
Quick Controls retains its separate shutdown-only proxy. Failed checked Wi-Fi,
scan, update cancellation, directory or file cleanup stops the native controller
immediately. Reentry, further provider calls, queued handoff, rendering and
finalization are suppressed after the Runtime retention barrier. Default/Watch
retry behavior is unchanged. Healthy repeated opens do not overwrite live native
grants, and successful close permits reopening.

## Verification

The initial production-adapter reproducers at base b0d33b5 demonstrated the same
bug in all three controllers: open returned true and made one further acquisition
after an injected empty acquisition failure. With the facade, each returns false,
retains exactly once and makes no later acquisition.

Run the production-controller fault matrix with a generated native SDK:

```
python scripts/test_native_app_custody.py \
  --sdk-include build/x4-updates/ota_update/native-time-sdk/include
```

It tests both update kinds, Wi-Fi and Files with Quick radios enabled, telemetry
on/off, normal compilation and ASan/UBSan. Cases cover empty/dirty acquisition,
malformed tokens, release failure/uncleared output/size damage, first and later
credential reads, credential writes including selector commit, healthy
JOINING/UP, cancellation, repeated open/close, failed scan/file/directory cleanup,
ordinary read/write/connect errors, queued handler handoff cancellation, telemetry
policy recursion and repeated finalization after retention. Provider I/O and
allocator frees after retention abort the fixtures. The queued-handoff check
models the established Runtime retention contract; it is not a Runtime or
physical-device test.

Compatibility/target checks:

```
python scripts/test_native_system_apps.py --runtime RUNTIME --utilities UTILITIES --skip-legacy --ble-broadcast
python scripts/test_x4_updates.py --ble-broadcast --target-dir build/x4-updates
python scripts/test_portable_wifi.py
python scripts/test_portable_update.py
python scripts/test_file_browser_operations.py
python -m unittest discover -s tests -v
```

The two legacy Wi-Fi/update test runners explicitly enable TEST_IDLE_ELIGIBILITY
for their fixture-only idle hook assertions. This does not enable a product idle
policy.

## Integration

Cherry-pick the source commit into the in-progress System cohort based on
b0d33b5. Rebuild the selected native Wi-Fi, Files, Firmware Update and App Store
ELFs from that source. Include PortableAppCustody.h and the updated
PortableNativeCustody.h when staging portable headers; existing builders copy
these automatically. Preserve the allocated cohort versions and pinned Runtime
and alarm SDK prefixes. Refresh downstream artifact identities/admission receipts
through the normal composition build. Do not overwrite the frozen 0.1.16 image.
No publication or physical hardware operation is part of this change.
