#!/usr/bin/env bash
set -eo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(mktemp -d)"; trap 'rm -rf "$build"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=${SANITIZERS:-address,undefined} -fno-omit-frame-pointer);fi
cc -std=c11 -Wall -Wextra -Werror "${flags[@]}" -I"$root/lib/InstalledCatalog/include" \
 "$root/lib/InstalledCatalog/src/InstalledCatalog.c" "$root/test/native_apps/installed_catalog_test.c" -o "$build/test"
"$build/test"
