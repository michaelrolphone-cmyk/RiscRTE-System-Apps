#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="$(mktemp -d)";trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -Wall -Wextra -Werror -Ilib/PortableApps/include -Ilib/NativeApps/include -Isdk/driver)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=undefined -fno-sanitize-recover=all);fi
for fixture in protected_settings_path protected_settings_legacy;do
 cc "${flags[@]}" "test/native_apps/${fixture}_test.c" -o "$out/$fixture"
 "$out/$fixture"
done
