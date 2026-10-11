# Shared Files / WebDAV / BLE integration plan

Source checkpoint plan, 2026-10-11. No firmware assembly until inputs qualify.

## Exact dependencies
- Current System base: 81c877ddc5667b702350570f5c680ea99a16a7c4 (dynamic installed packages).
- Current Runtime base: 73f07a6bf333fad881b3b15ef0f282b244e11fa4 (dynamic installed packages; preserve current capacity).
- Historical System frontend: b9527121a1d8fe18b1e3e664702f5e854caa5ece, including sharing commits be5f94912e97598829d3206040f13a4dbb643af8 and 3e00721612ee148b29bc58fb432bbe77847922d6.
- Historical Runtime reference: 39d2718d731c501dd8cd8cb7fbaecf1ab56c140b. Reconcile entropy 67e0dbe1a5f519fd96d8f88797ff91b9e1a3096c, HCI coexistence 72836606aadfc0485b0728d1f3108bb60a3c5b6a, scheduler-only wait 39d2718 individually; exclude historical capacity/version changes.
- BLE setup provider: Drivers 72ab7038732c62ece220d48fd33da873575c122f.
- X4 complete baseline receipt: 8d1680b34888593325b38e6c16c36d788d282d81 (.80; Reader/DailyPaper, Wi-Fi 1.1.26, MSC 0.1.7, SD 0.2.16, panel .12).
- Watch full 25-app canvas baseline: 089868395268b696c05651c33816345bfe0425df (draft PR64, touch 2.2 / Buttons 1.22).
- Concurrent discovery composition reference: X4 4200423; never write its feat/dynamic-installed-packages branch. Rebase/final composition belongs to the single integrator.
- Received Library correction 03a6af3: archive SHA256 c3d505ce2fb416f82eb67c3d140209e8ababf28959027e83466d2af413efb346; all eight source hashes verified before applying. Runtime software service selectors remain zero. See selector-source-mapping.json.

## Sequence and boundaries
1. Publish this source plan, then port the three focused System sharing commits onto current source, resolving common adapter changes without losing installed discovery, file management, handler dispatch, canvas/raster or scrolling.
2. Qualify canonical BLE provider independently; reconcile Runtime as a focused patch series on the exact current base, without old capacity overrides or product policy in Runtime.
3. Apply supplied selector correction, verify capability authority against actual provider instances, and build selected RGB Watch and paper X4 Files plus TCP/entropy/BLE provider ELFs.
4. Exercise authentication/replay/session deadlines and secrets, stop/expiry/failed cleanup, radio ownership/coexistence, SD unavailable/exported state, file operations and type-to-app launch. Distinguish host evidence, target component compilation and hardware evidence.
5. Supply exact component receipts, export map/grant requirements, and composition procedure against both complete baselines. Assemble no full firmware until all integration inputs qualify; no credentials, hardware access, flashing, master merge or release.

## Transport and browser contract
WebDAV over explicitly started Wi-Fi carries files; BLE supplies a finite setup descriptor with explicit numeric comparison. OTS remains backlog. Preserve mature Reader browser behavior through shared Watch/Nova capabilities and UI, never firmware coupling. Existing restricted AppData export semantics must not be represented as unrestricted SD WebDAV mutation support; qualify and report the actual exported surface.

## Publication
Use isolated feat/shared-files-webdav-broker, feat/shared-files-runtime-reconcile and feat/shared-files-ble-qualified branches. Checkpoint tested source increments on GitHub. Initial sandbox fetch failed with DNS resolution; approved network escalation successfully fetched the exact Drivers source. Report actual later publication failures verbatim.


## Latest reconciliation and qualification
The focused series was rebased without textual conflicts onto Runtime
91324f41b5a79df21f6bc39a04899eba4d0be351 and System
782fcbb69678a6d722f4a0d0aed127c3fdfe24f9. Runtime result is
287e1650567007ee24482e375cd14d172e709b47 on
`feat/shared-files-runtime-91324f4`; System uses
`feat/shared-files-system-782fcbb`. Earlier published branches are retained.
Latest X4 discovery composition is user-supplied d38300b, which still needs
complete-store assembly by its integrator; this is not the .80 baseline used
for the preserved-store comparison below. No concurrent branch was modified.

Async storage v3 copied/idempotent removal and v2 discovery compatibility
remain unchanged by the Files series. Current async adapter, transactional
installed service and App Store tests pass. Target Files builds were rerun
against the rebased source. Source hashes and commands, not version labels,
identify the components; the successor Files label is 1.5.23.

### Evidence and actual scope
See qualification-2026-10-11 for target ELF receipts, composition preservation
hashes and host logs. Watch builds against the exact full canvas app contract,
including Contexts, low battery, local sleep/navigation, telemetry and Quick
Actions. Checked storage is an explicit shared adapter mode; it does not select
an X4 toolbar. The Watch Alarm header is retained intact, avoiding interpretation
of its sleep callback as X4 output-mode metadata.

Host tests cover real frontend/adapter flows, authentication and replay,
secret expiry, user stop and refused cleanup, radio lease coexistence,
rename/move/copy/delete and recursive operations, absent/remounted/exported SD,
and Runtime immutable file-type handler dispatch. BLE provider protocol and
numeric comparison tests pass. ESP32-S3 Files, TCP listener, entropy and BLE
provider objects/ELFs compile; this is not hardware interoperability evidence.
Darwin wrappers are included: sanitizer runs use UBSan only, not ASan; linker
and Bash compatibility adaptations are explicit. ASan did not complete on this
host. Historical evidence copied by earlier commits is not new verification.

### Complete composition path and admission blockers
1. Integrator uses the latest discovery source lineage and the focused Runtime
   patch series; retain async storage v3/v2 interfaces and current capacities.
2. Build both Files components with the commands in their receipts. Build the
   three providers using the source builders and their receipts; BLE uses exact
   Drivers 72ab7038732c62ece220d48fd33da873575c122f.
3. Run scripts/prepare_shared_files_composition.py with a complete baseline,
   matching Files directory and TCP/entropy/BLE build directories, Runtime,
   explicit export map and a new output path. It verifies ELF hashes and export
   namespace ownership, preserves every other app/provider file, and emits a
   NOT_ADMITTED_NO_FIRMWARE store plus evidence. The example map is a test input,
   not a user-approved deployment export policy.
4. Qualify provider/live-grant/RAM capacity and full native graph/import admission.
   Complete .80 X4 needs 31 providers; Watch needs 30. Current discovery limits
   provide 29. No historical capacity override was copied. Watch requires 22
   policy and 18 requirement rows (select 24/24); X4 requires 16/15 (16/16).
5. Reconcile the X4 d38300b complete composition and rerun admission with all
   existing Reader/DailyPaper, Wi-Fi, MSC, SD, panel and Watch touch/button
   components. Physical client interoperability remains untested by instruction.
   Only then consider a full firmware build in a separately authorized task.

The network export surface is explicitly mapped AppData files: GET/HEAD,
OPTIONS, PROPFIND and conditional PUT. General remote SD rename/move/delete,
collection creation and unrestricted SD WebDAV mutation are not implemented.
Local browser operations remain available. The overall shared-browser product
is therefore unfinished, despite qualified component source and host coverage.
No credentials, flashing, master merge or full firmware were used.

Publication succeeded with approved network escalation. Initial sandbox DNS
failure was bypassed; there is no outstanding GitHub publication barrier.
The Library transfer initially failed, then succeeded with the system CA file;
archive hash verification passed. Signed transfer metadata is not published.
