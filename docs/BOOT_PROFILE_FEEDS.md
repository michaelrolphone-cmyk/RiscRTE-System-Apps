# Public core boot-profile feeds

`scripts/build_boot_profile_feed.py` is an offline publication-preparation tool.
It freezes an
**already qualified** product store and its matching bootfs image into the
existing Installed Apps feed envelope. It does not build or qualify firmware,
modify source policies, publish assets, queue provisioning, or access a device.

## Inputs and prerequisites

Provide all inputs explicitly:

- Exact admitted product store directory, including every valid icon/font/data
  asset and `cohort.json`; no synthetic substitutes or filtered manifest closure.
- Exact native firmware and exact packed bootfs image for that store.
- Runtime checkout supporting schema 4 compact image inventories and 160 files
  (`1dbd767` or a descendant containing that support). Runtime's physical image
  admission and complete read-only decoder are used from this selected checkout.
- Product identity matching `cohort.json`, profile `core`, and an immutable HTTPS
  asset directory ending in `/`. Its contents are supplied by the caller; the
  tool makes no network calls and cannot prove remote hosting is immutable.
- One to three explicit SNTP servers, each at most 63 bytes and valid for native
  `TimeInput`. There is no guessed default time service.

The output directory must not exist; its parent must exist. Input/output paths
must not be symlinks, and the output cannot be inside or contain the input store.

```sh
python3 scripts/build_boot_profile_feed.py \
  --runtime /path/to/Runtime \
  --store /path/to/qualified/store \
  --image /path/to/qualified/bootfs.bin \
  --firmware /path/to/qualified/firmware.bin \
  --base-url https://your-authorized-host.example/immutable-release/assets/ \
  --product twatch-s3 --profile core \
  --time-server your-explicit-time-server.example \
  --output /path/to/new-public-bundle
```

The hostname examples above are placeholders, not chosen hosting or SNTP
services. Each additional time server is another `--time-server` argument.

## Output contract

The bundle contains:

- `native-packages-v1.json`, at most 65,536 bytes
- `assets/bootfs-<sha256>.bin`, byte-identical to the supplied image
- Authorized optional `assets/image-<sha256>.elf` mirrors, byte-identical to their
  corresponding internal fallbacks; identical bytes share one hosted asset
- `receipt.json` with exact feed/asset hashes and sizes, inventory count, public
  profile size and remaining Wi-Fi headroom
- `SHA256SUMS` covering the feed, receipt and assets
- `COMPLETE`, written last; interrupted output is removed before a retry

The feed has exactly this top-level shape:

```json
{
  "schema": "riscrte.native-packages",
  "version": 1,
  "product": "<explicit product>",
  "apps": [],
  "profiles": {"core": []},
  "boot_profiles": {
    "core": {
      "native": {
        "layout": "<cohort layout>",
        "runtime_version": "<cohort runtime version>",
        "store_abi": 3,
        "firmware_size": 123,
        "firmware_sha256": "<64 lowercase hex characters>"
      },
      "profile": {
        "schema": "riscrte.provisioning",
        "schema_version": 4,
        "image": {"url": "<immutable HTTPS image URL>", "bytes": 5570560, "sha256": "<SHA-256>"},
        "files": [["<relative path>", 123, "<SHA-256>"]]
      },
      "time": {"schema": "riscrte.sntp", "schema_version": 1, "servers": ["<explicit server>"]},
      "mirrors": []
    }
  }
}
```

This is a shape illustration, not a valid feed or a candidate. All real sizes,
identities, layouts and hashes come from the supplied verified bytes.

The public profile is at most **15,872 bytes**, including its standalone JSON
newline. The native limit remains **16,384 bytes**. The service injects saved
owner Wi-Fi at queue time. Public inputs cannot contain Wi-Fi or credential
fields; no owner secrets or firmware bytes are copied into the feed. Recursive
JSON field screening is a guard, not a general secret scanner: the caller must
supply already reviewed, publishable store assets. Errors do not echo input
values. No private provisioning input or credentials are accepted by this CLI.

## Exact custody and native binding

The tool verifies layout/ABI agreement, complete cohort fields, product,
canonical source/version identity, firmware length and SHA-256, and embedded
Runtime version/store-ABI markers. It checks exact partition size, physical
SPIFFS page safety/capacity and **decoded full-image equality with every file in
the supplied store**. A missing, extra or changed asset fails even when no
manifest references it. All supplied valid assets and native policy fields stay
byte-for-byte in the frozen image. The inventory has at most 160 files, and
Runtime's SPIFFS path/size/capacity bounds remain enforced. Inputs are reread
before creating output; feed and assets are verified after writing.

Compiled native images may also contain a standalone NUL-terminated
`RISC_RUNTIME_VERSION:` or `RISC_PAIRED_STORE_ABI:` scan-prefix literal. Only
these exactly empty literals are ignored. Each field must still have at least
one genuine marker, and every nonempty occurrence must match the cohort value
exactly, including its terminating NUL. Conflicting, malformed or truncated
occurrences fail even when another matching marker is present.

These custody checks do not replace native board, graph, grant, manifest or ELF
admission. A matching firmware hash alone does not prove a firmware image is
executable or qualified. Continue to use the product's independent admission and
hardware qualification pipeline.

## Optional SD mirrors

Only a boot-owned app/driver row with explicit `image_source.internal_fallback:
true` authorizes a new mirror. Such a row must select `volume_instance: 12`, retain
its verified internal image, and already name the exact relative path:

- Apps: `Apps/<manifest-id>/image-<internal-ELF-sha256>.elf`
- Drivers: `Drivers/<manifest-id>/image-<internal-ELF-sha256>.elf`

The feed exposes the same paths with a leading `/`. Mirror IDs must use only
lowercase letters, digits, hyphens and underscores, matching the current
consumer. Each descriptor has exactly `driver` (boolean), `path`, `url`, `size`
and `sha256`; there are at most 64 distinct mirrors, each at most 2 MiB. The
builder never invents a source policy or exports a manifest/grant with a mirror.
Product composition must keep storage providers and their native dependency
closure internal. Runtime app fallback requires the corresponding app-fallback
support (`a297018` or a descendant containing it).

Existing external-only source rows with absent/false `internal_fallback` remain
unchanged and are excluded from new mirrors. Their internal ELF may be absent;
for example, a previously admitted SD-only app remains SD-only. An internal-only
product yields `mirrors: []`. SD absence or failure and retained-ownership handling
remain the consumer/Runtime's responsibility; the builder does not simulate or
alter those policies.

## Verification

The tests use clearly labeled nonexecutable fixtures in temporary directories.
No candidate firmware, target ELF or production bootfs is fabricated or
published. A small test-only SPIFFS serializer exercises the real decoder and
physical image checker.

```sh
RUNTIME_REPO=/path/to/Runtime tests/run_boot_profile_feed_test.sh

# Also validate generated feeds against the existing boot-profile consumer.
RUNTIME_REPO=/path/to/Runtime \
BOOT_PROFILE_CONSUMER_REPO=/path/to/boot-profile-enabled-SystemApps \
tests/run_boot_profile_feed_test.sh

# ASan + UBSan for both native/consumer C++ harnesses.
RUNTIME_REPO=/path/to/Runtime \
BOOT_PROFILE_CONSUMER_REPO=/path/to/boot-profile-enabled-SystemApps \
SANITIZE=1 tests/run_boot_profile_feed_test.sh
```

In a traced/container executor where LeakSanitizer reports that it cannot run,
use `ASAN_OPTIONS=detect_leaks=0` for the sanitizer command and record that leak
checking was unavailable. Address and undefined-behavior checks remain enabled;
do not describe that run as leak-checked.

The runner compiles the actual Runtime schema-4 parser and, when selected, the
actual Installed Apps consumer through its existing host fixture. Both consume
the builder's generated 160-file feed and the exact 15,872-byte public-profile
boundary fixture. Native parsing also exercises maximum escaped Wi-Fi injection.
Python coverage includes deterministic output, full inventory/asset custody,
all supported layouts, native/cohort mismatch, physical/decoded image corruption,
credential fields, unsafe paths/URLs, required time input, source-only policy
preservation, mirror limits, 161-file overflow, input mutation, interruption
cleanup and successful retry. The test runner performs no network activity.

To include the known Wave 0.3.4 and Watch 0.2.24 CI firmware binaries in the
marker regression suite, additionally set `BOOT_PROFILE_WAVE_FIRMWARE` and
`BOOT_PROFILE_WATCH_FIRMWARE` to those exact local `firmware.bin` paths. The tests
read and hash these real binaries without modifying them or creating a candidate.
Both must contain ABI 3 and their respective genuine version marker; the empty
Runtime scan literal is explicitly checked to reproduce the prior rejection.
