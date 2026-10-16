#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RUNTIME_REPO:?Set RUNTIME_REPO}"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=();if [[ "${SANITIZE:-0}" == 1 ]];then flags=(-fsanitize=${SANITIZERS:-undefined} -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
cc -std=c11 -Wall -Wextra -Werror ${flags[@]+"${flags[@]}"} -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$root/lib/PortableApps/include" -I"$root/lib/NativeApps/include" "$root/test/native_apps/automatic_foreground_test.c" "$root/lib/PortableApps/src/PortableAutomaticProvision.c" -o "$build/test"
"$build/test"
