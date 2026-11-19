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
and image ID tof-coordinator. The shared BiomeRTE images.py verify, publish
and render commands accept this lock; the named-context ToF builder adds no
second publication tool. image-receipt.json retains the secondary core source,
base manifest, exact source archive hashes and test results. Publication is
an explicit later operation after a registry is available.

The initial Mac build qualified linux/arm64 with the same pinned NumPy 1.26.4
and SciPy 1.11.4 as the native service. GitHub's standard Linux runner qualifies
its actual Docker platform, normally linux/amd64, against the corresponding
fixed base manifest. Other platforms fail explicitly. Linux/arm/v7 remains
unqualified.

A future runtime or dependency change must update the source lock deliberately
and repeat qualification. This preserves the difference between the recorded
deployed baseline and new behavior. CI artifacts retain the image archive,
common release lock, image/source receipts and bounded build/test logs for
seven days, so an image registry is not required to transfer a qualified
candidate.

Stateful activation remains gated by the separate ToF checkpoint/fence and
native-reader handoff described in README.md. Passing build/tests is not a
claim of active failover.
