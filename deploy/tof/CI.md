# ToF image qualification CI

This change is based directly on main and includes only the tested standalone
ToF runtime/tests and its container preparation. It does not import the
unmerged ESP32 firmware, OTA provisioning or earlier firmware workflow edits
from the original deployment worktree.

source-lock.json records exact hashes for all31 captured runtime/test/package
files from verified image source72a725bf2f7fce388d1a6cf761fe7b70b0965ad8.
The public source branch preserves that commit. Shared core client
c6cf46c1fc8b47ab31fdae00f261db95bcfcec55 is also retained remotely.
CI pins reachable corea68b63035d0407c3fac121868e04bf067554ae47; its client
is byte-identical to c6 and its SHA256 is checked before the build.

The qualification harness verifies committed file hashes, creates exact Git
archives for the ToF package and pinned shared client, selects the immutable
base platform manifest, builds and checks OCI source/revision labels, runs
--help and all83 tests in a read-only512MiB container with no network or live
mounts, then saves the image and a SHA256 receipt. Only an isolated /tmp tmpfs
is writable. The image default stays in preparation and no live service or
device operation is started. Registry publication remains in the common
BiomeRTE release pipeline; this CI has no publish/deploy command.

The initial Mac build qualified Linuxarm64 with the same pinned NumPy1.26.4
and SciPy1.11.4 as the native service. GitHub's standard Linux runner qualifies
its actual Docker platform, normally linux/amd64, against the corresponding
fixed base manifest. Other platforms fail explicitly. Linuxarm/v7 remains
unqualified.

A future runtime/dependency change must update the source lock deliberately
and repeat qualification. This preserves the difference between the recorded
deployed baseline and new behavior. CI artifacts retain the image archive,
image/source receipts and bounded build/test logs for seven days, so an image
registry is not required to transfer a qualified candidate.

Stateful activation remains blocked on the separate ToF checkpoint/fence and
native-reader handoff described in README.md. Passing build/tests is not a
claim of active failover.
