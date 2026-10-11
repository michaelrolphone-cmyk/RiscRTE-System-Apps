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
- Pending Library input: eight-file selector correction 03a6af3. Runtime software services at instance zero must retain selector zero.

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
