#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=(-g -Wall -Wextra -Werror)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize="${SANITIZERS:-undefined}" -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-pie -no-pie);fi
vendor="$root/lib/RemoteFiles/vendor/tinydtls_sha2"
cc "${flags[@]}" -std=c11 -I"$vendor" -c "$vendor/sha2.c" -o "$build/sha2.o"
c++ "${flags[@]}" -std=c++17 -I"$root" -I"$vendor" -I"$root/lib/PortableApps/include" "$root/test/wifi_reconcile_model_test.cpp" "$build/sha2.o" -o "$build/test"
"$build/test"
