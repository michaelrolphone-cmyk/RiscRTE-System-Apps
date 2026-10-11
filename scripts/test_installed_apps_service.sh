#!/usr/bin/env bash
set -eo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO to the matching runtime source}"
build="$(mktemp -d)"; trap 'rm -rf "$build"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=${SANITIZERS:-undefined} -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
cc -std=c11 -Wall -Wextra -Werror "${flags[@]}" -I"$root/lib/InstalledCatalog/include" -c "$root/lib/InstalledCatalog/src/InstalledCatalog.c" -o "$build/catalog.o"
c++ -std=c++17 -Wall -Wextra -Werror "${flags[@]}" -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" -I"$runtime/lib/ArduinoJson/src" -I"$root/lib/PortableApps/include" -I"$root/lib/InstalledCatalog/include" "$root/test/native_apps/installed_apps_service_test.cpp" "$build/catalog.o" -o "$build/test"
"$build/test"
