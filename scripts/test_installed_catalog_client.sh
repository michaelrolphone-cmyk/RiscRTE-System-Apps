#!/usr/bin/env bash
set -eo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO}"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=undefined -fno-sanitize-recover=all);fi
cc -std=c11 -Wall -Wextra -Werror "${flags[@]}" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$root/lib/PortableApps/include" -I"$root/lib/NativeApps/include" "$root/lib/PortableApps/src/PortableInstalledCatalog.c" "$root/test/native_apps/installed_catalog_client_test.c" -o "$build/test"
"$build/test"
