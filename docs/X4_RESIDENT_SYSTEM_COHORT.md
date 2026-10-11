# Selected X4 System resident cohort

`examples/x4-resident-system-cohort.json` extends the frozen three-app development recipe to Files 1.5.15, Wi-Fi 1.1.18, OTA/App Store 1.2.8, and USB SD Transfer 0.1.3. Source 57464c1 preserves the delivered native time, navigation, scrolling, storage bindings, Wi-Fi permission classifier and disabled update catalog. Explicit resident clients use the host power-policy handshake, with Wi-Fi/update idle eligibility checked and USB held awake while it owns media. Ordinary and Watch build defaults retain their previous identities and selection.

The recipe requires the exact clean source/header pins in the specification and the existing Xtensa GCC 8.4.0 esp-2021r2-patch5 compiler. It produces development ELFs and an admission proposal, not an install image. Use a separate output directory:

```sh
python3 scripts/build_x4_resident_cohort.py \
  --spec examples/x4-resident-system-cohort.json \
  --system /workspace/shared/x4-resident-system-qualified-source \
  --runtime /workspace/shared/runtime-resident-policy-0189 \
  --utilities /workspace/shared/points-catalog-storage-custody \
  --productivity /workspace/shared/points-catalog-productivity \
  --product /workspace/shared/x4-resident-desk-product-040 \
  --sdk /workspace/shared/x4-provider-sdk-recovered \
  --baseline /workspace/shared/x4-043-image \
  --msc-sdk /workspace/shared/x4-037-image/replacement-inputs/usb_sd_transfer/include \
  --compiler /workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc \
  --output /workspace/shared/x4-resident-system-qualified-target
```

All eight targets pass actual ELF structure, loader and import/export checks. Exactly one `pqa_render` definition belongs to default; all seven foreground ELFs omit shared renderer/font/sheet symbols and the title. The three existing targets are byte-identical to the qualified policy cohort. The recipe keeps all 21 delivered application manifests and the 19-entry Springboard catalog. Its explicit role proposal selects default as host, seven foregrounds, and thirteen inventory-preserved legacy apps. GameBoy is recorded from custody metadata only; its accepted .43 binary is never read, built or tested and has no resident overlay while running.

USB adds one explicit alarm.service API2 instance-0 binding, already admitted to the delivered host. No provider or transport is rebuilt; updates use `--apps-only`. Product adoption still requires the qualified Runtime .89 native target, retained/policy/metadata capacities, complete store admission and final release identity reconciliation. These app checks do not qualify the stopped .87 native target, Windows USB hang, serial restoration or physical USB behavior.

## Reproducible qualification

The new test runner loads actual controllers, adapters and host through the production Runtime/Graph, with separate admitted synthetic peripheral ELFs. The controllers are seeded at settled screens, then execute explicit checkpoints and normal Home loops. It checks Files selection/editor preservation, copied file.open path and return cookie, Wi-Fi/update busy eligibility, USB stopped-owner gating, async settlement, ordinary refusal, terminal fencing, and cached API probes after retention. It runs 37 cases normally and the same 37 with ASan/UBSan:

```sh
python3 scripts/test_resident_system_clients.py \
  --runtime /workspace/shared/runtime-resident-policy-0189 \
  --display-sdk /workspace/shared/x4-provider-sdk-recovered \
  --alarm-sdk /workspace/shared/x4-resident-policy-qualified-target/default/idle-sdk/include \
  --msc-sdk /workspace/shared/x4-037-image/replacement-inputs/usb_sd_transfer/include
```

Existing scroll matrices passed 128 Files, 80 Wi-Fi and 120 update processes across normal and sanitizer variants. Their old cancelled/cleanup expectations failed identically on clean 5062560; the fixtures now distinguish recoverable queue GAPs (`next == -1`) from terminal touch errors (`next < -1`) and unconfirmed native cleanup. These test corrections assert pinned custody and no I/O, rendering or free after retention. The USB production app/adapter suite passed 24 normal plus 24 sanitizer cases using the exact delivered MSC SDK.

The source/input hash receipt is `/workspace/shared/x4-resident-system-proof/qualification.json`; target commands, exact pins, hashes and policy proposal are in the target output. Peripheral behavior is synthetic and hardware qualification remains open. The physical Home action while a child drawer is open was separately reproduced as closing the drawer and resuming the child; its correction and the requested reference rendering are the next isolated shell successor.
