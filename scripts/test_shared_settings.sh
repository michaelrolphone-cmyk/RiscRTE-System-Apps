#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO to the matching Runtime source}"
out="$(mktemp -d)";trap 'rm -rf "$out"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
cc -std=c11 -Wall -Wextra -Werror "${flags[@]}" -I"$root/lib/InstalledCatalog/include" -c "$root/lib/InstalledCatalog/src/InstalledCatalog.c" -o "$out/catalog.o"
c++ -std=c++17 -Wall -Wextra -Werror "${flags[@]}" -I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" -I"$runtime/lib/ArduinoJson/src" -I"$root/lib/PortableApps/include" -I"$root/lib/InstalledCatalog/include" "$root/test/native_apps/shared_settings_test.cpp" "$out/catalog.o" -o "$out/shared"
"$out/shared"
