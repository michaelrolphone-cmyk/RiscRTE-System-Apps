#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO to matching Runtime}"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=${SANITIZERS:-undefined} -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
cc -std=c11 -Wall -Wextra -Werror ${flags[@]+"${flags[@]}"} -I"$root/lib/InstalledCatalog/include" -c "$root/lib/InstalledCatalog/src/InstalledCatalog.c" -o "$build/catalog.o"
c++ -std=c++17 -Wall -Wextra -Werror ${flags[@]+"${flags[@]}"} -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" -I"$runtime/lib/ArduinoJson/src" -I"$root/lib/PortableApps/include" -I"$root/lib/InstalledCatalog/include" "$root/test/native_apps/installed_apps_boot_profile_test.cpp" "$runtime/src/runtime/storage/PackageFiles.cpp" "$build/catalog.o" -o "$build/test"
"$build/test"
