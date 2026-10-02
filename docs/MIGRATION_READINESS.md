# External migration readiness — 2026-10-02

Reader remains the read-only source of truth. This audit refreshed current Reader master after U1 #96 and CAM #344 merged.

## System-Apps source and release parity

- Reader master: `82caa0997e913f01c1f5f9ab942d056bc9f04a82` (CAM #344 merge; first parent includes merged U1 #96).
- System-Apps starting main: `28d02fb01c9006b025c0f83e829c1fe117283115`.
- Durable work claim: [System-Apps issue #13](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/issues/13).
- The current base-aware audit covers all 38 approved source, manifest, and helper inputs: 17 unchanged, six converged, 15 upstream-only manifest updates, with no external-only edits or conflicts. All 18 approved app source files now match current Reader; the 12 remaining differences are manifest-only changes outside this bounded package-workflow batch. See [source drift](source-drift.json).
- The three synchronized package workflow apps use current published Reader identities: App Store **1.0.8**, Driver Manager **1.0.8**, and Package Manager **1.1.1**. Each independent Xtensa build exactly reproduces the corresponding released ELF. Full Reader `.rte.zip` asset size and SHA-256 also match the exact release workflow artifact and GitHub release metadata.
- Exact release byte parity is now **11/18**, and all 11 required identities match. The other seven rows retain their existing historical classifications; no parity gates were weakened. [Release parity](release-parity.json) records the updated artifacts.

CAM added the optional Camera Utility and camera-driver packages. Camera Utility is outside this repository's foundational System-Apps set. Driver work remains with its owners; the open X4 Drivers PR #12 was inspected and left untouched.

## Other authorized repositories

Fresh main/open-work checks found: Drivers `cb0bd49d1fdd002c639c81437dce9fb0e3055d5b` with owner-owned X4 PR #12; MCU-Dev-Tools `70fd3ad39eaacdb736a16de5f351c128b208f255`; Utilities `4343b808f762b67bb14c39fb50f28d32819af07e`; Productivity `4c9ae58cf172babd4405a0d6b095d1517eb1fa5a`. No open work was found in the last three. Their already completed parity was not repeated.

## Readiness limits

`parity_ready_count` remains **0**. These source, test, build, and published-byte results do not prove U1 ZIP/runtime behavior, separate publication, device installation, physical readiness, or provider cutover. No package-format migration, release automation change, live catalog change, Reader source removal, hardware qualification, or runtime ownership switch is included.

Historical checkpoints: [U1 package workflow refresh provenance](PARITY_REFRESH_U1_PACKAGE_APPS_2026-10-02.md), [Status Bar Settings refresh](PARITY_REFRESH_2026-10-02.md), [2026-10-01 refresh](PARITY_REFRESH_2026-10-01.md), and [historical ELF lineage](HISTORICAL_ELF_LINEAGE.md).


## 2026-10-02 published manifest reconciliation

Current Reader master declares Button Remap, Clear Reading Cache, and Firmware Update at 1.0.2. System-Apps now tracks those released manifests. Their 1.0.2 ZIPs were extracted from Reader release workflow artifact 11209466823 (run 36965130240); SHA-256 and sizes match GitHub release metadata, and the embedded ELFs match the prior 1.0.1 ELF identities exactly. This refresh updates metadata and per-app provenance without changing binaries or fabricating a version. See [manifest reconciliation provenance](PARITY_REFRESH_MANIFESTS_2026-10-02.md).
