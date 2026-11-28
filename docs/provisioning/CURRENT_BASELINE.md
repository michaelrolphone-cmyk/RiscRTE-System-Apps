# Current-baseline automatic provisioning

This source checkpoint composes the opt-in provisioning controller and installed-app service with the native SDK6 System baseline. It preserves the current preference-store, RTC, generic file storage, crash handling, renderer, and toolchain selections. Activation still requires a product-owned feed, boot-profile request grant, selected service manifest, and Home controller integration.

The controller first publishes/discovers the cached local catalog. It serializes foreground networking with other services, reads saved Wi-Fi credentials through a guarded read-only adapter, and retains ownership on uncertain cancellation. A missing removable volume leaves internal discovery available. First-fetch time is a private HTTP trust input; it does not seed or change the visible civil clock. The request is handed to the preserving next-boot Runtime path.

## Reproduce host gates

Set `RUNTIME_REPO` to the matching cumulative Runtime checkout and `TMPDIR` to a writable build directory, then run:

    scripts/test_automatic_foreground.sh
    scripts/test_installed_apps_automatic.sh
    scripts/test_installed_apps_boot_profile.sh

Repeat each with `SANITIZE=1 SANITIZERS=address,undefined ASAN_OPTIONS=detect_leaks=0`. Also run the existing installed-app service, install, media, feed and catalog-client regressions. Leak detection is disabled for the executor's ptrace limitation; address and undefined-behavior instrumentation remain active.

The selected matrix checks first-start and changed-profile transactions with 160-file interruption boundaries, private-time admission, base-mode cancellation isolation, boot-profile queued/pending handling, internal fallback, and immutable removable-volume mirrors. Normal and instrumented runs passed on 2026-10-16.

## Reproduce target service

Use the ESP32-S3 GCC15 wrapper, not the bare multi-target compiler:

    NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc python3 scripts/build_installed_apps.py \
      --runtime-repo "$RUNTIME_REPO" --output-dir /tmp/installed-apps \
      --internal-packages --native-feed --boot-profiles --time-server pool.ntp.org

The command verifies the private-time header against the selected Runtime SDK and audits dynamic imports. The explicit time-enabled service is version 0.15.0. A local SDK6/GCC15 link passed at 179016 bytes with only the audited ordinary memory/string imports. Product feed URLs and identities must be supplied for other devices. This is target-link evidence, not hardware execution, a complete product package, or publication of a feed.
