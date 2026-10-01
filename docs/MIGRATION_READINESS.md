# External migration readiness — 2026-10-01

Reader remains authoritative; no deletion/cutover is approved. Source baseline
for this audit: Reader `524e2cb3ea27311b062f3d7f182668c6912829fc` (master).
Release-version comparison: Reader release-index
`964866f65cc2ebb07b820b42c06d0827c03d197d`. U1 `impl/u1-riscrte` is prospective
compatibility only and is actively changing. No U1 source was imported here.

| Owner repository / baseline main | Scope | Source divergence | Build/test/artifacts | Independent release/index |
|---|---|---|---|---|
| System-Apps / `353dcfcb4018a0414270fa80fa79e314d4abb143` | 18 foundational apps | Baseline `525e3268`; five apps have upstream-only source+manifest updates; no external-only/conflicts | This increment: independent pinned SDK, 17 host fixtures, 18 validated ELFs, CI development artifacts | Not ready; zero apps claimed parity-ready |
| MCU-Dev-Tools / `d58158666d59b13474df66ae039950b0e1d685c6` | Serial Monitor, USB Debug, Firmware Flasher | Eight source/helper/manifest files match Reader; versions match released index | No standalone SDK/build/tests/CI yet | No releases/index |
| Utilities / `c9e992a0be59d658d4cb044308c17d2d0bfd7ec2` | Battery, GPS, LoRa | Six source/manifest files match; versions match | Source/docs only; Battery metadata overstates completion | No releases/index |
| Productivity / `d06ff80fa8d862cb139d45d0e70b861181f518da` | Text Editor, Timecard | Missing upstream Text Editor 0.2.1 discard fix and Timecard 1.0.1 lunch-overlap fix; helper unchanged | No independent SDK/build/tests/CI | No releases/index |
| Drivers / `9039be6c9abb30742b7a77a8ef39d507aaf01cea` | 23 installable capability packages | All driver trees and 23 headers match Reader; 21 released versions match | All 23 build artifacts and parity job passed; some canonical replay still fetches historical Reader context | No destination releases/index |

System-Apps drift is explicit in [source-drift.json](source-drift.json): App
Store, Springboard, File Browser, Wi-Fi Networks and Font Manager. No payloads
were copied in this increment, so their existing versions remain unchanged.
All 18 app-specific interface/capability documents remain indexed in README.

Drivers evidence: [run 36788950538](https://github.com/michaelrolphone-cmyk/RiscRTE-Drivers/actions/runs/36788950538)
passed at exact main above with 23 unexpired artifacts. Its older “CI pending”
metadata for board-power 0.1.6, power-profile 0.1.1 and controller 0.1.19 is stale:
canonical byte replay passed for all three, including 763 controller loader-map
sites. However `controller_host_startup_test.cpp` and
`controller_role_switch_test.cpp` are not invoked by destination CI; this is a
real remaining coverage gap. I2C, GT911 and controller replay are not yet fully
decoupled from Reader build context. No changes were made to Drivers here.

## Maintenance claim / next increment

This branch claims only System-Apps independent build/test/evidence plumbing.
Next prioritize base-aware synchronization of the five drifted system apps and
the two published Productivity fixes, keeping existing independently developed
files intact and bringing matching host fixtures and code-grounded docs forward.
Then reuse the verified build approach for MCU/Utilities/Productivity, and wire
Drivers' existing controller fixtures before correcting its completion metadata.
Recheck current remote heads before every increment; the hashes above are an
audit snapshot, not permanent authority. Keep one coherent draft PR per changed
repo and no direct writes to default branches.

## Removal criteria / real blockers

1. Source changes reconciled with immutable baselines, per-app versions and
   interface/capability docs maintained; no external-only improvement lost
2. Independent pinned builds plus meaningful tests and exact-commit CI artifacts
   for each owned package; eliminate remaining Reader build-context fetches
3. Per-package `.rte.zip` and generic release-index behavior verified against the
   accepted runtime format, including identity, hashes, resource layout and
   update/rollback behavior. Current U1 release-record/index work is unfinished;
   do not invent a competing external format or publish loose pairs as complete
4. Independent publication/version parity and actual runtime integration proven;
   development artifact retention is not release availability
5. Explicit owner cutover approval before Reader copies/catalogs are removed or
   redirected. No merge, release, hardware qualification or cutover implied here
