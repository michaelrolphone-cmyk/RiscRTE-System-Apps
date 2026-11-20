# ToF image qualification CI

This change is based directly on main and includes only the tested standalone
ToF runtime/tests and its container preparation. It does not import the
unmerged ESP32 firmware, OTA provisioning or earlier firmware workflow edits
from the original deployment worktree.

source-lock.json records exact hashes for 31 runtime, test and package files.
The original qualified source is
72a725bf2f7fce388d1a6cf761fe7b70b0965ad8, retained on its public source
branch. The locked Dockerfile and README now add common release metadata and
handoff instructions; the runtime and tests remain byte-identical to that
captured revision. Shared core client
c6cf46c1fc8b47ab31fdae00f261db95bcfcec55 is also retained remotely.
CI pins reachable core a68b63035d0407c3fac121868e04bf067554ae47; its client
is byte-identical to c6 and its SHA256 is checked before the build.

The qualification harness verifies committed file hashes, creates exact Git
archives for the ToF package and pinned shared client, selects the immutable
base platform manifest, builds and checks OCI and common release labels, runs
--help and all 83 tests in a read-only 512 MiB container with no network or live
mounts, then saves the image and its SHA256. Only an isolated /tmp tmpfs is
writable. Pull requests build their actual head commit so image provenance
names a fetchable branch commit, rather than an ephemeral merge revision.
The image default stays in preparation; no live service or device operation
is started.

The output includes images.lock.json with schema biomerte-container-release/1
and image ID tof-coordinator. The shared BiomeRTE images.py verify, combine, publish
and render commands accept this lock; the named-context ToF builder adds no
second publication tool. image-receipt.json retains the secondary core source,
base manifest, exact source archive hashes and test results. The workflow now
publishes qualified main releases to private GitHub Container Registry packages.

The initial Mac build qualified linux/arm64 with the same pinned NumPy 1.26.4
and SciPy 1.11.4 as the native service. Pull requests and non-main manual runs
use separate native GitHub AMD64 and ARM64 runners. Each checks its actual
Docker platform, selects the matching fixed base manifest, and runs the complete
isolated suite. A trusted main publication job builds and tests both exact
images locally, using explicit ARM64 QEMU emulation on the AMD64 runner.
Other platforms fail explicitly. Linux/arm/v7 remains unqualified.

A future runtime or dependency change must update the source lock deliberately
and repeat qualification. This preserves the difference between the recorded
deployed baseline and new behavior. Local qualification still exports a
Docker-loadable archive for offline transfer. This public repository's Actions
artifacts retain only image/source receipts, common locks and build/test logs;
they never upload the binary image archive.

Stateful activation remains gated by the separate ToF checkpoint/fence and
native-reader handoff described in README.md. Passing build/tests is not a
claim of active failover.



## Private GHCR publication

The workflow publishes only for this repository's main branch. Pull requests
and manual dispatches on other branches run qualification without package
write permission. A main push or manual main dispatch runs one trusted job
that builds and tests both platforms before any image is published. A new
main run does not cancel an in-progress publication. No moving latest tag
is used.

The trusted job alone receives packages:write. Its temporary GitHub Actions
token is passed only to the private-visibility checks and registry-login
step. Checkout credentials are not persisted, and the token is not passed
to Docker builds or the isolated test containers. No user token or registry
secret is added to the source repository. The package is:

    ghcr.io/michaelrolphone-cmyk/tof-coordinator

The qualification harness accepts an explicit --platform for this cross-platform
job; omitting it preserves the existing Docker-server-platform default.
Both docker build and the isolated docker run checks use the selected platform.
QEMU setup is explicit and uses the maintained Docker setup action. Binary
wheels, source-lock verification and the original preparation-only runtime
remain unchanged.

The common release utility gives each platform a source-commit tag and records
its confirmed immutable registry digest. Before the first push, the shared
GHCR helper rejects an existing package unless its visibility is private.
An absent package is created with GitHub's default private visibility. A second
check requires the published package to exist and remain private.

Publication checks out the reviewed common release-tool revision separately
from the unchanged a68b630 shared host-client source. Both platform archives
stay on the same ephemeral runner. The common combine command verifies their
checksums and primary source and provides one release lock. The workflow
requires exactly tof-coordinator for linux/amd64 and linux/arm64; each
secondary core source proof, image receipt and successful test result must
match the checked-out source lock. The common publisher verifies all loaded
image provenance before its first push. Each confirmed push is recorded
atomically, so a later interruption does not erase earlier digest receipts.

The lightweight tof-ghcr-release artifact retains the combined digest lock,
private-visibility receipts, both platforms' provenance/test evidence and
service.prepared.json for 30 days. Read-only qualification metadata is kept
for seven days. Neither artifact contains image archives, Docker configuration
or credentials. Image binaries are transferred through authenticated GHCR
pulls rather than public Actions artifacts.

service.prepared.json stays inert and outside the live service catalog.
Publishing this image does not grant ToF ownership, transfer state, or activate
device output. Native ToF remains the production owner until the separate
checkpoint, pending-command custody, native-reader and output-fence handoff
has been qualified. Private pulls on a Mac or Pi require appropriately scoped
host credentials; a successful Actions publication does not establish those
credentials or physical-host compatibility.

## Qualification progress and completion

The qualification harness waits for actual subprocess completion, without fixed
wall-clock cutoffs. ARM64 emulation can legitimately take longer than a native
run. Build, help-smoke and verbose unittest output is streamed immediately to
the Actions log and flushed stage logs. A nonzero exit still fails qualification;
partial output remains available for diagnosis. The complete runtime test suite
and its network, filesystem, memory, CPU and process isolation remain unchanged.
Two small subprocess checks cover live log visibility before child exit and
retained stdout/stderr plus the actual failure exit code.
