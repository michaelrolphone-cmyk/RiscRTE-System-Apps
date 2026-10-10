# Next integration unit: coherent provider capacity

Design/test direction only. Runtime and the immutable X4 0.1.50 image were not
changed. References below are to recovered Runtime commit
`e66c5f1056a90f80b17cbe3f42db0242cbe2ced4`.

The inspected .50 boot has23 ordinary providers and21 app policies. Three shared
UI packages require26 providers. Its demand-retained activation does not bypass
admission limits. Do not remove existing features to fit.

## Smallest coherent change

For the PSRAM/cohort branch only, change `RiscLimits` from Apps24 / Providers24 /
Grants40 to Apps24 / Providers26 / Grants42. This preserves the existing16 graph
grant slots beyond eager/promoted boot pins. Keep legacy static19 /17 /32.
Strengthen the cohort reserve assertion to preserve that16-slot policy rather
than relying only on the current Providers+12 check. Rebuild, version and qualify
a new native firmware/cohort; this is not a metadata-only edit to .50.

Grants42 is the minimum capacity-policy-preserving direction, not proof that an
arbitrary resident host+foreground combination fits. Each invocation has16 live
app-grant slots, while the graph pool is shared. Instrument actual Home→Points
and Home→BLE naming flows with26 promoted providers. Existing declared provider
rows are Home12, Points6 and BLE8, before adding the text grant to either client;
policy declaration counts do not establish simultaneous live use. If the actual
peak exceeds16 non-boot graph grants, use the measured bound plus justified
headroom. Do not suppress grants, revoke live leases or change retention semantics
to hide exhaustion.

## Exact limit and storage locations

- `src/runtime/RuntimeLimits.h:7–12`: current legacy17/32 and cohort24/40.
- `src/runtime/drivers/ProviderGraphV2.h:50–54`: inherited module/grant capacity.
- `src/bootstrap/Runtime.h:137–142`: inherited MaxDrivers; int8 policy-index
  assertion covers26.
- `src/bootstrap/Runtime.cpp:687`: rejects boot driver lists above MaxDrivers.
- `src/runtime/drivers/ProviderGraphV2.cpp:119`: independent graph admission cap.
- `Runtime.h:288,339–340`: provider storage, boot grants and driver records.
- `Runtime.cpp:193`: dependency matrix `[MaxDrivers][MaxDrivers]`; stack storage
  grows576→676 bytes.
- `ProviderGraphV2.h:145–149,165–166`: node, bound dependency and grant arrays;
  uint8 node indices remain covered by the assertion at line52.
- `src/runtime/streams/ProviderQueueHost.cpp:43`: provider stream-context array.
- `src/ports/esp32s3/CpuPort.h:111–115` and `CpuPort.cpp:185–189`: provider
  synchronization storage and admission.
- `src/bootstrap/CohortRuntime.inc:167–172`: filename bound derives from
  `2*Apps+2*Providers+3`, automatically99→103.

## Other bounds to verify, not enlarge blindly

- Provider requirements16 (`ProviderGraphV2.h:51`): current .50 maximum7;
  scene5, text1 or2, profile0.
- App policies24 (`Runtime.h:69`): .50 has21; no extra app identity is needed.
- App manifest requirements16 and policy rows16/17 (`Runtime.h:70–71`,
  `AppPolicyLimits.h:5–10`): installed .50 Home already uses16/17. Adding text
  to selected .50 Points gives10 requirements/11 rows, BLE11/11. Preserve all
  existing profile-specific grants, including Home's17-row native build option.
- Live grants16 per invocation (`Runtime.h:295`, acquisition at
  `Runtime.cpp:412–413`); distinct main/foreground invocation storage is at
  `Runtime.h:325–327`, resident transitions at `ResidentShellRuntime.inc:166–191`.
- Native platforms32 (`Runtime.h:137`) and privileged policy entries16
  (`NativeProviderPolicyV1.h:82–83`): the three ordinary services add neither.
- Provider stream endpoints32, queues/context4 and bytes32KiB
  (`ProviderQueueHost.h:10–12`): these services use no streams.
- JSON file limit65,536 bytes (`Json.cpp:18`): current boot.json is27,859 bytes.
- Hardware-device/bus/GPIO limits do not grow for these software singletons.

## Required acceptance and boundary tests

1. Update `test/run_runtime_limits_test.sh:13–23` for26/42 on host, paired and
   metadata-PSRAM builds; retain the exact legacy19/17/32 checks.
2. Rerun full/overflow/duplicate-last Runtime admission
   (`test/runtime_test.cpp:29–45`), graph last-provider/dependency16–17/grant
   saturation (`test/drivers/provider_graph_v2_test.cpp:46–78`), and derived
   provider stream-context saturation (`test/provider_queue_host_test.cpp:276–285`).
3. Extend hard-coded p00..p23 fixture generation at
   `test/run_demand_retention_test.sh:13`; its C++ limit assertions already derive
   the capacity (`test/demand_retention_test.cpp:85–111`).
4. Add exact26-provider product admission, reject27, and execute converted
   Points/BLE accepted/cancelled/Back/handoff/pending/retained paths with maximum
   promoted occupancy and recorded peak live graph grants.
5. Rerun demand activation/retention; resident shell/loading/legacy/policy/native
   context; cohort/update/admission; stream retention; ASan/UBSan; target builds
   and strict ELF/import checks. Check final target DRAM/PSRAM and stack deltas,
   including stream registry and CPU synchronization storage.
6. Produce fresh exact-native/cohort/store receipts and round-trip checks before
   any separately authorized product release or physical qualification.

This26-provider unit covers the base onscreen text manifest. .50 has no
`usb.hid.keyboard` input provider or its complete host stack. Selecting the USB
manifest needs additional packages and a new capacity/live-grant audit; outbound
BLE HID and USB mass storage do not meet that dependency. No new per-client
Runtime lifecycle semantics are required for normal converted text flows.
