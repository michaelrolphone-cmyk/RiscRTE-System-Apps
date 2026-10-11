#!/usr/bin/env bash
set -eo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO}"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=undefined -fno-sanitize-recover=all);fi
cc -std=c11 -Wall -Wextra -Werror "${flags[@]}" -I"$root/lib/PortableApps/include" -I"$root/lib/NativeApps/include" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" "$root/lib/PortableApps/src/PortableInstalledCatalog.c" "$root/test/native_apps/installed_catalog_client_test.c" -o "$build/test"
"$build/test"
cc -std=c11 -DPORTABLE_INSTALLED_AUTHORITY_STYLES -Wall -Wextra -Werror "${flags[@]}" -I"$root/lib/PortableApps/include" -I"$root/lib/NativeApps/include" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" "$root/lib/PortableApps/src/PortableInstalledCatalog.c" "$root/test/native_apps/installed_catalog_client_test.c" -o "$build/test"
"$build/test"
cc -std=c11 -DTEST_IDLE_ELIGIBILITY -Wall -Wextra -Werror -Wno-unused-function "${flags[@]}" -I"$root/lib/PortableApps/include" -I"$root/lib/NativeApps/include" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" "$root/test/native_apps/installed_resident_checkpoint_test.c" "$root/lib/PortableApps/src/PortableInstalledCatalog.c" -o "$build/resident"
"$build/resident"
