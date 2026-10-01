# External migration readiness — 2026-10-01

Reader remains authoritative; no deletion/cutover is approved. Source baseline
for this audit: Reader `be82695ea0ecb14525c0de1ddc78cd0c77e4614b` (master).
Release-version comparison: Reader release-index
`7f07d124dc20435618ae32bb2f89e0f9595d6d9a`. U1 `impl/u1-riscrte` is prospective
compatibility only and is actively changing. No U1 source was imported here.

| Owner repository / baseline main | Scope | Source divergence | Build/test/artifacts | Independent release/index |
|---|---|---|---|---|
| System-Apps / `ed9c065004d0fdca88729452c1f1a02966135f4e` | 18 foundational apps | All 38 source/manifest/helper files match audited Reader; base-aware sync preserved external changes | Independent pinned SDK; 18 host fixtures; 18 validated ELFs; five exact published-byte matches; CI development artifacts | Not ready; zero apps claimed parity-ready |
| MCU-Dev-Tools / `d58158666d59b13474df66ae039950b0e1d685c6` | Serial Monitor, USB Debug, Firmware Flasher | Eight source/helper/manifest files match Reader; versions match released index | No standalone SDK/build/tests/CI yet | No releases/index |
| Utilities / `c9e992a0be59d658d4cb044308c17d2d0bfd7ec2` | Battery, GPS, LoRa | Six source/manifest files match; versions match | Source/docs only; Battery metadata overstates completion | No releases/index |
| Productivity / `d06ff80fa8d862cb139d45d0e70b861181f518da` | Text Editor, Timecard | Missing upstream Text Editor 0.2.1 discard fix and Timecard 1.0.1 lunch-overlap fix; helper unchanged | No independent SDK/build/tests/CI | No releases/index |
| Drivers / `9039be6c9abb30742b7a77a8ef39d507aaf01cea` | 23 installable capability packages | All driver trees and 23 headers match Reader; 21 released versions match | All 23 build artifacts and parity job passed; some canonical replay still fetches historical Reader context | No destination releases/index |

System-Apps source parity is recorded in [source-drift.json](source-drift.json):
all 38 source, manifest and helper entries are unchanged against the new audited
baseline. Synchronized App Store 1.0.6→1.0.7, Springboard 1.2.2→1.3.0,
File Browser 1.3.0→1.3.1, Wi-Fi Networks 1.0.1→1.0.2 and Font Manager
1.0.0→1.0.1. These reuse the identical already-versioned upstream bytes, not new
payloads under existing published identities. [Release comparison](release-parity.json)
proves exact published-byte parity for those five; 13 older artifacts still
differ, which is a real outstanding qualification item. All 18 app-specific
interface/capability docs remain indexed in README.

Drivers evidence: [run 36788950538](https://github.com/michaelrolphone-cmyk/RiscRTE-Drivers/actions/runs/36788950538)
passed at exact main above with 23 unexpired artifacts. Its older “CI pending”
metadata for board-power 0.1.6, power-profile 0.1.1 and controller 0.1.19 is stale:
canonical byte replay passed for all three, including 763 controller loader-map
sites. However `controller_host_startup_test.cpp` and
`controller_role_switch_test.cpp` are not invoked by destination CI; this is a
real remaining coverage gap. I2C, GT911 and controller replay are not yet fully
decoupled from Reader build context. No changes were made to Drivers here.

## Maintenance claim / next increment

System-Apps independent build PR #3 is merged; its main-branch
[CI and artifact](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/36797871187)
passed at `ed9c065004d0fdca88729452c1f1a02966135f4e`. This next increment
claims the five System-App updates, matching helpers/fixtures/docs, and pinned
release-byte checks. No external-only changes were overwritten.

Next prioritize the two published Productivity fixes and its independent build
pipeline. A [historical no-strip probe](historical-build-probe.json) reproduces 11 of the
13 older artifacts exactly; integrate explicit version-pinned historical build
profiles and investigate the remaining Button Remap/Driver Manager differences
without reissuing changed bytes under old identities. Reuse the verified build approach
for MCU/Utilities/Productivity, and wire Drivers' controller fixtures before
correcting its completion metadata. Recheck remote heads before each increment.
Validated external parity PRs may now be merged under owner authorization; no
release, catalog cutover or Reader deletion is authorized by that permission.

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
   redirected. No release, hardware qualification or cutover implied here
