# Shared Files source-build readiness — 2026-10-11

**Superseding result:** [FULL_LINK_TEST_BUNDLES.md](FULL_LINK_TEST_BUNDLES.md) records completed full native links, memory bounds and independently verified complete test bundles. The source-only limits below describe the earlier checkpoint.

The complete current X4 and Watch compositions now qualify as inputs to native firmware assembly. This supersedes the unfinished/capacity/SD blockers in the initial integration plan. It does not assert a linked firmware image, on-device heap margin, or physical WebDAV/BLE interoperability.

## Exact source inputs

| Component | Commit |
|---|---|
| Runtime focused reconciliation and explicit sharing build profile | `81845e8027b94abc5c7a36a45e0004884ada73b6` |
| Runtime dynamic-installed base | `91324f41b5a79df21f6bc39a04899eba4d0be351` |
| System Files SD/WebDAV implementation | `28f8cfa85211b1be422fe44705a2f48950e4207b` |
| System dynamic-installed base | `782fcbb69678a6d722f4a0d0aed127c3fdfe24f9` |
| X4 composer, admission and real SD integration tests | `b455a100a31a60171bd439105a506a9d9e0ddcd6` |
| X4 complete current baseline | `d38300bccdbf75d2d786bd3cbc87101fef1dabb7` |
| Watch full canvas baseline | `089868395268b696c05651c33816345bfe0425df` |
| BLE setup provider | `72ab7038732c62ece220d48fd33da873575c122f` |
| Loader compaction and boot-store tooling | `db5f6c5ba1a71ed3fcc22ff29847397a050b7ee4` |

System's accompanying publication adds packing tooling and this evidence; its parent is the implementation commit above. Exact component source hashes and commands are recorded in `qualification-2026-10-11/source-build-ready/`. The original eight-file selector correction remains applied; Runtime software service selectors stay zero. Isolated branches are `feat/shared-files-runtime-91324f4`, `feat/shared-files-system-782fcbb`, and X4 `feat/shared-files-composition`.

## Complete composition and capacity

| Target | Apps | Providers | Target ELFs admitted | Store payload | Free 4 KiB blocks |
|---|---:|---:|---:|---:|---:|
| Current X4 | 26 | 32 | 42 | 2,740,980 bytes | 552 |
| Watch full canvas | 25 | 30 | 54 | 4,520,430 bytes | 76 |

Both boot-store images are 5,308,416 bytes. Their packing receipts include every file hash, loader-preservation proof, independent extraction/round-trip proof and image SHA-256. Original debug ELFs remain in the local packing output. Watch required lossless ELF metadata compaction to fit; no app, provider or functionality was removed. Every non-ELF input was preserved byte-for-byte. Admission was rerun on the final compacted stores, not only their larger inputs.

Select 32 providers / 48 grants / 64 installed apps / 24 policy rows / 24 requirement rows. Default and legacy capacities are unchanged. Target compiler layout evidence gives +12,624 bytes per Runtime object, +25,248 bytes for the two PSRAM objects, +240 bytes CpuPort and +60 bytes TCP registry. These are structural deltas, not measured hardware heap headroom. A 33-provider cohort is rejected. Namespace ownership negative cases remain rejected. Native role/import and full Runtime cohort admission succeed with zero hardware/storage calls.

## Behavior and tests

Shared Files keeps canvas/raster browser behavior, installed handler launch, local file management and shared Watch/Nova capability boundaries. X4 selects its real SD volume; the Watch baseline has no SD provider and selects the explicitly mapped AppData surface. It does not invent an SD capability.

The generic SD backend supplies OPTIONS, GET, HEAD, PROPFIND, PUT, MKCOL, DELETE, MOVE and COPY through the existing storage interfaces. Streaming uses 4 KiB chunks. Digest auth/auth-int, replay, session expiry, user stop, cleanup refusal and radio lease ownership are covered. See [SD_WEBDAV.md](SD_WEBDAV.md) for overwrite recovery, bounded traversal, partial recursive-operation behavior and the deliberate lack of strong SD revision ETags. AppData revision/CAS behavior is retained.

The actual async-v3 X4 SD driver and FatFs integration passes MKCOL, 128 KiB PUT/GET, overwrite, recursive copy/delete, MOVE, PROPFIND, USB-export unavailability and recovery, and async-install-worker exclusion/recovery. Sector failure after removal correctly enters retained failure rather than pretending normal cleanup succeeded. Separate volume tests cover 1 MiB PUT, 2 MiB GET/COPY, 513-entry PROPFIND, UTF-8, named properties and cancellation at 19 phases. Existing ten regression suites pass; actual Files frontend HTTP/BLE flows and immutable file-type launch evidence are retained in the qualification directory. Sanitized Darwin runs use UBSan; no ASan success is claimed.

## Reproducible assembly path

1. Use the exact complete baseline stores and component receipts. Build both Files profiles plus TCP listener, entropy and the pinned BLE provider. Files receipts preserve the full target-specific UI contracts. The target components compile and pass ELF/import validation.
2. Run `scripts/prepare_shared_files_composition.py` against each complete baseline with the matching Files/provider directories, Runtime, `--provider-capacity 32`, and an explicit AppData export map. The supplied Timecard-only map is a test policy, not authorization to export other private namespaces. Preserve every unrelated source component. The preparer's raw `NOT_ADMITTED_NO_FIRMWARE` status is intentionally preliminary; final admission receipts supersede it.
3. Run `scripts/pack_shared_files_store.py --store <composed-store> --tools <pinned-tools> --output <new-output> --compiler <xtensa-gcc>`. It emits a boot-store image only and verifies loader equivalence plus independent filesystem round-trip. Run X4 `minimal/scripts/qualify_shared_files.py` on the packed store using the recorded Runtime and native import witness. The witness is only a resolver reference, not a newly built firmware deliverable.
4. For X4 run `minimal/scripts/prepare_native_runtime.py prepare` against the exact Runtime with `--environment esp32s3-16mb-appdata-iq-stage --app-policy-rows 24 --app-requirement-rows 24 --installed-app-capacity 64 --provider-capacity 32 --shared-files-transport --app-image-cache --usb-phy --retained-wake-bytes 512 --failure-evidence`, preserving the baseline's native options. For Watch use an actual Git checkout of Runtime and environment `esp32s3-16mb-appdata-iq-sharing`. A Git archive alone cannot satisfy the existing reproducible-build Git identity checks.
5. `pio run -e <environment> -t compiledb -j 1` succeeded for both prepared sources; the actual Runtime.cpp object compiled from each generated command database. Receipts record the selected defines and object hashes. These are source configuration checks, not a native link. The single integrator can now assemble against the matching packed store and qualify full-link memory/layout before any hardware release.

## Coordination and remaining limits

The concurrently developing tagged async storage suffix reserves ops 100–103 separately and preserves v2/v3 requests and ops 1–7. This WebDAV work needs no new storage ABI or reservations and does not alter those interfaces. The separate task's unpublished installer journal/admission changes were inspected for overlap, not copied or overwritten. Reconcile any subsequent published Runtime changes through the single integrator and rerun admission when source changes.

Publication succeeded through approved network access; no outstanding GitHub publication barrier remains. A direct coordination message to the supplied parent thread returned `thread not found`; this report carries the reservation conclusion for the parent. No credentials, hardware operations, flashing, master merge, full firmware link or fabricated on-device evidence were used. Physical client interoperability, live heap/stack margin, full-link memory layout and deployment export policy remain integration/release checks.
