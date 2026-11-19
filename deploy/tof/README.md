# Portable ToF coordinator candidate

This preserves the complete installed module closure captured in
installed-source-provenance.json. The installed release is the baseline.
Geometry, surveyed world state, front/rear ranging, IMU history, LiDAR floor
correction, magnetic reference persistence and survey-zero remain real.

## Current qualification boundary

The source and image recipe support isolated build/replay qualification.
The default container is inert preparation. The existing
biomerte-service-adapter/1 lifecycle still requires a ToF-specific consistent
checkpoint, pending-command custody, native-reader handoff and output fence.
No automatic failover or active replacement is claimed before those proofs.

Keep the native coordinator as production owner. Do not mark authoritative
JSON as cache/stateless, share native SQLite across the VM, or start a second
active coordinator. A registry is not needed for local build/export.

## Transport boundaries

GatewayRoutes delegates current registered routes, identity-preserving
observations and native ARP candidates to the shared authenticated host
adapter. Front/rear FTM and magnetic calibration resolve device references;
camera credentials remain on the native side.

The native UDP relay retains original peer IP/port, receive-clock bounds and
collector reply source. Sequence gaps clear legacy partial packet assembly.
Docker UDP NAT is not assumed to preserve that identity.

VisionFeed retains the complete metadata/JPEG input and all sensor semantics.
Known host monotonic fields are converted using bounded exchanges; original
values remain available. Excess uncertainty, stale clock samples or changed
host boots fail closed. Firmware counters and UTC times are not restamped.

Native tof_registration reads rover_ranges.observed in the Mac epoch.
The output conversion therefore changes only raw monotonic range records
back to that host epoch and retains local values plus host intervals.
UTC baseline/refinement/world state remains UTC. Persisted motion events
now explicitly convert the original observation age to UTC; the previous
coordinator passed a monotonic midpoint into this UTC state model.

Optional authenticated HTTP survey control preserves world_id/request_id.
The native adapter can forward its existing survey operation to the one
qualified portable authority. Existing native execution still uses Unix
survey control by default.

## State inputs and ownership

| Input relative to mounted state | Role |
| --- | --- |
| tof/baseline.json | Coordinator baseline history/calibration; recovery state |
| tof/geometry-config.json + lock | Measured geometry, explicit CAS configuration writer |
| tof/display-floor-priors.json | Read-only display priors |
| tof/anchor-registry.json | Validated anchor discovery/checkpoint state |
| tof/rover-refinement.json | Coordinator motion/ranging refinement state |
| tof/latest.json | Atomic solution, consumed by native vision/provider |
| vision/config.json | Front identity; container needs identity, native adapter retains credentials |
| vision/camera-rig.json | Front/rear geometry and registered rear identity |
| vision/survey-world.json | Survey reference and measured features |
| vision/attitude-world-reference*.json | Existing persistent AHRS world references |
| vision/rover-motion.json | Native motion input |
| vision/survey-pose-zero.json | Survey-zero authority shared with operator/native consumers |
| vision/survey-heading-state.json | Coordinator heading continuity writer |
| vision/survey-magnetic-north.json | Confirmed device calibration receipt |
| vision/dual-radio-survey-calibration.json | Front/rear radio calibration writer |
| vision/rover-world-body-pose.json | World pose writer, consumed by native map/vision |

The service-specific checkpoint must cover these revisions consistently.
Native readers must follow the qualified authority or receive atomic
publications. A one-time directory copy does not provide failover. Native
DeviceRoutes SQLite and vision.sock are never image/mount inputs.

Preserve pending FTM request custody and uncertain POST effects during
handoff. A timeout does not establish that a device command did not apply.
Fence the previous coordinator/device/UDP writer before activating another.

## Configuration and health

New optional native arguments:

    --vision-state /state/vision
    --host-io-config /config/host-io.json
    --health-port 8771 --health-listen 0.0.0.0
    --survey-port 8772 --survey-listen 0.0.0.0
    --survey-token-file /private/survey-control.token

Container listeners must use private Docker/loopback publication. Survey
control requires an owner-only credential and a fixed operation path.

--check reads input/dependency/route/clock evidence and exits without effects.
--prepare-only stays alive without owning UDP, survey control or authoritative
writers. BIOME_SERVICE_PHASE=preparing from the existing runner forces this
mode too. No pretend activation endpoint is supplied.

GET /healthz is liveness. GET /readyz stays503 while preparing; active readiness
requires a current loop, fresh vision and anchor data, a qualified clock and
no camera error. A running inert child does not imply application readiness.

## Local build and registry handoff

Use the qualification harness from a committed checkout. It creates bounded
Git archives for this package and the pinned reviewed core client, leaving
credentials and native state outside the build contexts:

    python deploy/tof/ci.py \
      --core-repo /path/to/BiomeRTE \
      --docker /usr/local/bin/docker \
      --output /path/to/new/tof-release

The output contains image.tar, images.lock.json, image-receipt.json and
source-proof.json. The common lock records the immutable image ID, source
commit, platform, base digest and archive checksum. Image labels include
io.biomerte.image=tof-coordinator and io.biomerte.platform, plus both OCI
source/revision and the reviewed secondary core source. Use the shared
BiomeRTE release utility to verify and render the candidate:

    python /path/to/BiomeRTE/deploy/docker-services/images.py verify \
      --lock /path/to/new/tof-release/images.lock.json
    python /path/to/BiomeRTE/deploy/docker-services/images.py render \
      --lock /path/to/new/tof-release/images.lock.json \
      --image tof-coordinator --local \
      --template deploy/tof/service.template.json \
      --output /path/to/new/tof-release/service.local.json

Rendering records the local immutable image reference; it does not declare
or activate a service. Keep the result outside the live catalog pending the
state and fence qualification below. Once a registry is available, the same
BiomeRTE images.py publish command accepts this lock and records the resulting
immutable registry references. There is no separate ToF publication path.

NumPy1.26.4 and SciPy1.11.4 match the audited native versions. Binary wheels
are required; do not silently compile numerical libraries on a small host.
Linux arm64 is the first candidate. No arm/v7 readiness is claimed: SciPy
wheel availability and numerical qualification on the Pi remain a gate.

service.template.json uses the existing generic runner's named bindings,
externally_managed state, externally_fenced effects and staged activation.
Its image/authority placeholders are deliberately not deployable. Keep it
outside the live catalog until the checkpoint receiver, exact grant proof,
physical fence and native-reader handoff have been qualified.
