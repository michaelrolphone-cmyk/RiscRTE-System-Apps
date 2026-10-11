#!/usr/bin/env bash
set -eo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=(-std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=undefined -fno-sanitize-recover=all -I"$root/lib/PortableApps/include" -I"$root/lib/NativeApps/include")
cc "${flags[@]}" "$root/test/native_apps/update_installed_catalog_test.c" -o "$build/facade"
"$build/facade"
cc "${flags[@]}" -DTEST_IDLE_ELIGIBILITY -DPORTABLE_UPDATE_FIRMWARE=0 -DPORTABLE_APP_STORE_CATALOG "$root/test/native_apps/portable_update_app_test.c" -o "$build/controller"
for ((i=0;i<38;i++));do "$build/controller" "$i" >/dev/null;done
"$build/controller" 50
printf '%s\n' 'App Store installed inventory: 38 existing controller cases plus cached installed-list interaction PASS (UBSan)'
