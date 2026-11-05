#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO to matching Runtime}"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=${SANITIZERS:-undefined} -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
cc -std=c11 -Wall -Wextra -Werror ${flags[@]+"${flags[@]}"} -I"$root/lib/InstalledCatalog/include" -c "$root/lib/InstalledCatalog/src/InstalledCatalog.c" -o "$build/catalog.o"
c++ -std=c++17 -Wall -Wextra -Werror ${flags[@]+"${flags[@]}"} -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" -I"$runtime/lib/ArduinoJson/src" -I"$root/lib/PortableApps/include" -I"$root/lib/InstalledCatalog/include" "$root/test/native_apps/installed_apps_boot_profile_test.cpp" "$runtime/src/runtime/storage/PackageFiles.cpp" "$build/catalog.o" -o "$build/test"
"$build/test"
for feed in "$@"; do "$build/test" "$feed"; done

# Same full interruption/regression matrix with explicit first-fetch time.
c++ -DTEST_AUTOMATIC_FRESH_TIME=1 -DINSTALLED_APPS_BOOT_PROFILES=1 '-DINSTALLED_APPS_TIME_SERVER="time.cloudflare.com"' -std=c++17 -Wall -Wextra -Werror ${flags[@]+"${flags[@]}"} -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" -I"$runtime/lib/ArduinoJson/src" -I"$root/lib/PortableApps/include" -I"$root/lib/InstalledCatalog/include" "$root/test/native_apps/installed_apps_boot_profile_test.cpp" "$runtime/src/runtime/storage/PackageFiles.cpp" "$build/catalog.o" -o "$build/time-test"
"$build/time-test"
for feed in "$@"; do "$build/time-test" "$feed"; done
