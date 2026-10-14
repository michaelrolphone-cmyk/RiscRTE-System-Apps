# T023 first-feed TLS time

The selected foreground provisioning build can fetch its first HTTPS feed when
Home has a strictly validated UNSET native realtime snapshot. This does not set
an RTC, seed native realtime, change the displayed clock, clear reconciliation,
or choose a time server from an unverified feed.

## Explicit selection and interfaces

Build the Installed Apps service with `--native-feed --boot-profiles
--time-server time.cloudflare.com`, alongside the existing explicit product and
feed URL. The product explicitly selects `time.cloudflare.com`, as documented in
[Cloudflare's usage guide](https://developers.cloudflare.com/time-services/ntp/usage/).
The reusable builder accepts one bounded explicit DNS hostname and has no default.
This selection produces service
version 0.14.0 and records the exact server and source/header hashes in
`build.json`. It adds no manifest requirements or package grants.

The selected Runtime must expose the complete tagged `RiscHttpTimeV1.h` suffix
on its existing provider-only `platform.http-client@1` table. The builder checks
that the vendored System header is byte-identical to that Runtime SDK header.
`open_with_time` copies the single hostname and request and returns an ordinary
HTTP handle. Its private native sampler supplies only that session's TLS time;
normal read/info/close and ownership requirements apply. Sampling counts against
the original request deadline. Native transport and certificate qualification are
owned by Runtime, independently of this service's host-boundary tests.

The new `InstalledAppsAutomaticTimeV1.h` suffix follows the entire existing
boot-profile API. It exposes `begin_with_time(profile)`, and refuses before
arming work if the selected native suffix is absent or invalid. Existing
`automatic.begin(profile, utc)`, feed refresh, and `network_ready(utc)` still
reject zero UTC. Older consumers retain their original prefix/layout.

Home's private UTC hook uses zero only for a successfully read, strictly valid
UNSET snapshot. UNSET may retain `rtc_reconcile_pending=true`; the flag and all
clock/face state stay unchanged. A valid epoch of zero is refused rather than
mistaken for UNSET. VALID-with-pending and failed/malformed snapshots remain
refusals. The foreground controller checks the service suffix and then retains
its normal IP connection/service lease ordering before invoking the new entry.

## Transport and cleanup

A live explicit automatic mode selects the dispatcher, not the value of
`packageUtc` alone. Feed fetches, interrupted package recovery, and optional SD
mirrors share that dispatcher. Regular package networking remains UTC-gated.
The server is the compiled selection even if the downloaded boot profile contains
a different next-boot time configuration.

Cancellation keeps the mode and borrowed tables alive until pending downloads,
HTTP handles, cache writes, and media work drain. A retained result does not end
the network lease or release grants. Successful cancellation through either the base or automatic API clears the mode;
startup starts clear. An ordinary valid-UTC run uses the old HTTP open method
even in a build that supports the suffix.

SNTP is unauthenticated. It is not a new civil-time authority or an authenticated
clock source. Native HTTPS still checks the certificate chain, hostname, and
dates; this service never disables those checks or receives a raw socket.

## Verification

Set `RUNTIME_REPO` to the matching Runtime checkout, then run:

```sh
python -m unittest discover -s tests -v
bash scripts/test_automatic_foreground.sh
bash scripts/test_installed_apps_service.sh
bash scripts/test_installed_apps_automatic.sh
bash scripts/test_installed_apps_boot_profile.sh
bash scripts/test_installed_apps_media.sh
bash scripts/test_installed_apps_feed.sh
bash scripts/test_installed_apps_install.sh
SANITIZE=1 SANITIZERS=address,undefined bash scripts/test_automatic_foreground.sh
SANITIZE=1 SANITIZERS=address,undefined bash scripts/test_installed_apps_automatic.sh
SANITIZE=1 SANITIZERS=address,undefined bash scripts/test_installed_apps_boot_profile.sh
```

The automatic and boot-profile scripts run their complete existing interruption
matrices in both ordinary UTC and fresh-time modes. They cover 160 package
interruption/restart boundaries, 100 mirror interruptions, native UNKNOWN across
nine refreshes, retained media, and private credential cleanup. Additional checks
cover missing/truncated/wrong-tag/wrong-version/null-callback suffixes, no implicit
server, pending service leases, ownership loss, base cancel/quiesce followed by an
ordinary UTC-gated installation, and unchanged ordinary APIs.

Shared Home separately covers UNSET, VALID/pending, malformed and failed
snapshots, preservation of clock/face state, RTC recovery, native-height input,
and watch-face rendering. All tests and target links are software evidence;
physical hardware and network qualification remain separate.
