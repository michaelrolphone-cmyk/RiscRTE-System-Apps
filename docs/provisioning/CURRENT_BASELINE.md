# Current-baseline automatic provisioning

This source checkpoint composes the opt-in provisioning controller and installed-app service with the native SDK6 System baseline. It preserves the current preference-store, RTC, generic file storage, crash handling, renderer, and toolchain selections. Activation still requires a product-owned feed, boot-profile request grant, selected service manifest, and the opt-in Home controller selection described below.

The controller first publishes/discovers the cached local catalog. It serializes foreground networking with other services, reads saved Wi-Fi credentials through a guarded read-only adapter, and retains ownership on uncertain cancellation. A missing removable volume leaves internal discovery available. First-fetch time is a private HTTP trust input; it does not seed or change the visible civil clock. The request is handed to the preserving next-boot Runtime path.

## Reproduce host gates

Set `RUNTIME_REPO` to the matching cumulative Runtime checkout and `TMPDIR` to a writable build directory, then run:

    bash scripts/test_automatic_foreground.sh
    bash scripts/test_installed_apps_automatic.sh
    bash scripts/test_installed_apps_boot_profile.sh

Repeat each with `SANITIZE=1 SANITIZERS=address,undefined ASAN_OPTIONS=detect_leaks=0`. Also run the existing installed-app service, install, media, feed and catalog-client regressions. Leak detection is disabled for the executor's ptrace limitation; address and undefined-behavior instrumentation remain active.

The selected matrix checks first-start and changed-profile transactions with 160-file interruption boundaries, private-time admission, base-mode cancellation isolation, boot-profile queued/pending handling, internal fallback, and immutable removable-volume mirrors. Normal and instrumented runs passed on 2026-10-16.

## Reproduce target service

Use the ESP32-S3 GCC15 wrapper, not the bare multi-target compiler:

    NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc python3 scripts/build_installed_apps.py \
      --runtime-repo "$RUNTIME_REPO" --output-dir /tmp/installed-apps \
      --internal-packages --native-feed --boot-profiles --time-server pool.ntp.org

The command verifies the private-time header against the selected Runtime SDK and audits dynamic imports. The explicit time-enabled service is version 0.15.0. A local SDK6/GCC15 link passed at 179016 bytes with only the audited ordinary memory/string imports. Product feed URLs and identities must be supplied for other devices. This is target-link evidence, not hardware execution, a complete product package, or publication of a feed.

## Sparse Home integration

`--automatic-provision --installed-catalog-volume-instance 0` selects the current
full sparse Home integration. The product remains responsible for authorizing
its installed-app service, Wi-Fi/settings namespaces and optional storage volume.
Runtime selects only one app-authorized optional volume at instance zero; absent
or ambiguous grants leave internal discovery available. Partial or uncertain
grants retain custody. Home does not encode an X4 SD instance.

Provisioning starts only in foreground after the first completed Home frame and
loaded Quick radio policy. Airplane mode or Wi-Fi-off prevents networking. It
pauses RF contexts and broadcast while active, and drains cancellation before
Quick, alarm, sleep/launch or final teardown can take over. Timer-only dark entry
cannot start provisioning. Current preference, RTC reconciliation, generic file,
crash and rendering paths are preserved.

The time hook reads a checked native snapshot through `PortableRealtimeClient`.
Strict UNSET selects the existing private first-fetch-time service suffix; valid
positive UTC selects the existing UTC path. Malformed snapshots, valid epoch zero
and native I/O fail closed. Context loss or uncertain release retains custody.
The hook never reads or writes the civil RTC and never seeds native time.

### Reproduce actual Home host tests

Use the following source locks, also pinned in
`.github/workflows/automatic-provisioning.yml`:

- Runtime `46ab0039f7a8f514716996115a770c2151383669`
- X4 `c9ecac4058ead7a82b0f74560ac5b19f0b858b3f`
- Reader `c98e4222048244b1e729869ed47aa99130025fd3`
- Utilities `e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d`

With these checkouts:

    python3 scripts/test_home_provision.py --runtime "$RUNTIME_REPO" --x4 "$X4_REPO" \
      --sdk "$READER_REPO/sdk/driver" --utilities "$UTILITIES_REPO" --output /tmp/home-provision

The runner compiles the actual Clock, adapter, controller, native-time client,
network session and product sleep code; only native/provider/service boundaries
are doubled. It runs 28 process cases in normal and ASan/UBSan modes (56 total),
including real cold startup with native UNSET/valid time, pending cancellation
at Quick/alarm/finalization, no-volume fallback, radio/foreground gates, malformed
time and retained ownership. Source and staged public-header hashes are recorded.
The same workflow separately runs the three service/controller gates above,
normal and sanitized; it supplements existing workflows.

### Reproduce Home target

The delivered X4 Home recipe uses ESP32-S3 GCC 8.4.0 (`esp-2021r2-patch5`),
separately from Runtime's SDK6/GCC15 recipe. Select that compiler explicitly:

    python3 scripts/build_home_provision.py --runtime "$RUNTIME_REPO" --x4 "$X4_REPO" \
      --sdk "$READER_REPO/sdk/driver" --utilities "$UTILITIES_REPO" \
      --reader-scripts "$READER_REPO/scripts" --compiler /path/to/xtensa-esp32s3-elf-gcc \
      --output /tmp/home-provision-target

This stages a private SDK overlay, records the full command and existing
`PORTABLE_ALARM_BUSY_DEFER` wrapper define, then runs the canonical Reader
relocation normalizer and structural/import checks. No immutable release inputs
are modified. The selected Home version is 0.4.20, with 21 capability requirements.

On 2026-10-16, two independent recipe invocations produced byte-identical
623060-byte ELFs, SHA-256
`08d213f337fc061d63144e25371154679279e3b21a67ba34de30b6a05c98fb26`.
The ordinary approved imports were `free`, `malloc`, `memchr`, `memcmp`, `memcpy`,
`memmove`, `memset`, `risc_runtime_get_api`, `snprintf`, `strcmp`, `strcpy`, `strlen`
and `strncmp`. The current RTC regression passed 20 lifecycle cases and 54 rendered
comparisons in each normal/sanitized run; current crash and 47 atomic-setting
cases also passed. The unchanged deferred-radio lifecycle gate passed all 58
normal/sanitized cases with the full selected preference/availability profile;
its fixture now models clean atomic-writer absence without legacy fallback.

SDK6/GCC15 compiled this Home source but its linker aborted with
`double free or corruption (out)`, including the unchanged Home baseline without
provisioning. This does not change the delivered Home compiler selection and is
not GCC15 Home qualification. Hardware execution, a complete product package and
published feed activation remain separate product gates.

## Product-owned immutable mirror volume

`build_boot_profile_feed.py --mirror-volume-instance 9` explicitly selects X4's
already-admitted optional SD volume; another product may explicitly select 12.
The publisher requires a single matching boot-admitted `storage.volume@1`
provider and every `internal_fallback: true` image source must name that same
concrete positive instance. Missing selection, mismatches, multiple mirror
volumes, absent/ambiguous providers and invalid instances fail before output.
External-only image policies do not authorize new mirrors. No selection is
needed when there are no fallback mirrors. Existing boot/grant/manifests and
SPIFFS image bytes remain exact; the feed's immutable mirror schema is unchanged.

Reproduce the generator and actual service parser checks:

    python3 scripts/test_boot_profile_feed.py --runtime "$RUNTIME_REPO" --output /tmp/boot-feed-test
    bash scripts/test_installed_apps_boot_profile.sh /tmp/boot-feed-test/volume-9.json /tmp/boot-feed-test/volume-12.json

The generator uses deterministic synthetic inputs with the actual Runtime image
decoder and capacity checker. It proves volume 9/12 acceptance, 24 rejected
policies, four no-output build refusals, deterministic content-addressed mirrors
and unchanged boot/grant bytes. The shell gate runs the full existing service
matrix and both generated feeds through the production parser, also with the
explicit first-fetch-time suffix. Repeat with the sanitizer environment above.
These fixture bytes do not claim product graph, ELF or hardware admission.
