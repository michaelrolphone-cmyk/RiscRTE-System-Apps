#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO to a compact schema-4/160-file Runtime checkout}"
build="$(mktemp -d)"; trap 'rm -rf "$build"' EXIT
flags=()
if [[ "${SANITIZE:-0}" == 1 ]]; then
  flags=(-fsanitize="${SANITIZERS:-address,undefined}" -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
fi
c++ -std=c++17 -Wall -Wextra -Werror "${flags[@]}" \
  -I"$runtime/src" -I"$runtime/lib/ArduinoJson/src" \
  "$runtime/src/runtime/provisioning/Profile.cpp" "$root/tests/boot_profile_feed_native_test.cpp" \
  -o "$build/native-test"
export BOOT_PROFILE_NATIVE_TEST="$build/native-test"
if [[ -n "${BOOT_PROFILE_CONSUMER_REPO:-}" ]]; then
  consumer="$BOOT_PROFILE_CONSUMER_REPO"
  cc -std=c11 -Wall -Wextra -Werror "${flags[@]}" -I"$consumer/lib/InstalledCatalog/include" \
    -c "$consumer/lib/InstalledCatalog/src/InstalledCatalog.c" -o "$build/catalog.o"
  # Existing Runtime source uses compact multi-statement lines. Keep every
  # other warning fatal and keep this warning enabled for the new harness.
  c++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation "${flags[@]}" \
    -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" \
    -c "$runtime/src/runtime/storage/PackageFiles.cpp" -o "$build/package-files.o"
  c++ -std=c++17 -Wall -Wextra -Werror "${flags[@]}" \
    -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" \
    -I"$runtime/lib/ArduinoJson/src" -I"$consumer/lib/PortableApps/include" \
    -I"$consumer/lib/InstalledCatalog/include" -I"$consumer/test/native_apps" \
    "$root/tests/boot_profile_feed_consumer_test.cpp" "$build/package-files.o" \
    "$build/catalog.o" -o "$build/consumer-test"
  export BOOT_PROFILE_CONSUMER_TEST="$build/consumer-test"
fi
python3 "$root/tests/test_boot_profile_feed.py"
