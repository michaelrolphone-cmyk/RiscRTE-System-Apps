# Universal UI source recovery working checkpoint

Status: incomplete reconstruction; no target/hardware qualification or release claim.

Base: archived X4 .53 System source, originally 24b07f92eb8bab1943c1e7ef146960e5d896b37b. The original working tree and Git objects were lost when the execution filesystem was replaced on 2026-10-10. Current source is reconstructed from preserved source bytes, exact recovered PortableTouch and retained edit recipes. The GitHub parent is only a transport anchor; this is not a merge/integration claim against main.

Implemented: optional app-local nonblocking frame readiness/drain suffix, portable input collection separated from ordered one-edge logical delivery, profile-independent presentation progress, bounded raster input sampling, Home launch-state gates removed, Watch Nova timeline/current geometry independent of present completion, shared text attention ungated, and Quick Actions/alarm logical loops separated from pending presentation. Physical Home alone can use the additive Points Home guard. Provider/public Runtime ABI is unchanged.

PortableTouch consumes the existing ordered provider queue without a second queue. Same-owner geometry changes cancel only the active gesture; actual ownership handoffs use the snapshot watermark. Terminal provider failures freeze I/O. Valid ordered UP(old)/DOWN(new) contact replacement is two gestures, unlike the previous lossy snapshot-only fixture; Quick Actions case12 now explicitly verifies both completions. Failure cancellation remains covered by case8.

Fresh qualification after recovery (host doubles unless explicitly stated):
- Actual adapter RGB565/MONO1 with 0/17/120/2300 ms presentation: eight profiles each normal and ASan/UBSan, 60 touch cycles plus 60 navigation press/release cycles per profile. All logical actions finish before the first 2300 ms frame; only initial/latest images are submitted in that case. Modeled input lag is 1–4 ms. GRAY4 belongs to the separate scene provider, not this adapter's supported format set.
- Ordered touch declared-interface tests: nine normal/ASan/UBSan runs pass. A recovered reducer received one new focused guard: same-report MOVE before a second DOWN cannot expose ambiguous motion as a swipe; later independent reports do not cancel earlier movement. No secondary event queue or watermark advance is added.
- Generic app, Settings, local navigation, idle sleep/wake, retained sleep, time-format, and catalog capacity suite passes. Fixtures now emit raw edges and navigation release frames. The old 18-rejection assertion was already stale against the delivered 18-entry bound; actual 17/18 acceptance and 19 rejection are tested.
- Quick Actions: 104 actual-adapter rotation/radio/sanitizer cases plus four core/session runs pass. Foreground alarms: 24 normal/sanitized runs pass.
- Actual sparse Home/controller/adapter: 300 normal/sanitized gesture cases pass. A fresh reducer-proven DOWN can be the first logical pass; held entry remains blocked. A launch no longer requires a painted pressed frame.
- Actual Home, adapter and alarm service: 20 normal/sanitized readiness/failure cases pass. Copied Points projection updates at 264 ms while an immutable 900 ms image remains pending, with zero provider I/O for the copy.
- Actual restored UC8279 .1.12 provider set_brightness during ACTIVE presentation passes normally and sanitized; display/custody state is unchanged. This is a host resource-safety proof, not hardware testing.

Logical scroll controller rollout also passes 162 production cases including 81 sanitized, across 480x800/400x600 and 0/17/2300 ms display latency.

Unfinished: final shared controller integration, full selected Home target and boot/wake graphs, Utilities/GameBoy integration, crash-spool successor merge and clean target builds. Exact installed GT911 .1.9 source was recovered subsequently, SHA256 da287cbd675b6a59131b97278a48c6cf1734b90e30a9e2674e72b0063c9a86d1. The actual driver plus adapter reducer now pass 57 normal/ASan/UBSan runs (48 actual-driver and nine declared-interface runs), including same-report MOVE before second DOWN cancellation. Host I2C/GPIO/time are modeled; physical hardware remains untested. Scene/text raster and GameBoy owners are separate checkpoints and not silently included here. Manifest versions have not been changed for this unfinished working copy; reserve successors before target integration.

All target builds must explicitly set PLATFORMIO_SETTING_ENABLE_TELEMETRY=No. No hardware, flashing or updater transaction has been performed. This is a source working checkpoint, not a release.
