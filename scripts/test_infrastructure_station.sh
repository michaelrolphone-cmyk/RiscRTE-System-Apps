#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RISC_RUNTIME_ROOT:-$root/../runtime}"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=(-std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -Wno-misleading-indentation -Wno-deprecated-declarations -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-pie -no-pie);fi
includes=(-I"$runtime/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" -I"$runtime/lib/ArduinoJson/src" -I"$runtime/test/drivers/stubs")
"${CXX:-c++}" "${flags[@]}" "${includes[@]}" "$root/test/infra_node/station_binding.cpp" "$root/Services/infra_node/station.cpp" "$runtime/src/ports/esp32s3/CpuPort.cpp" -Wl,--gc-sections -o "$build/station"
"$build/station" "$root/profiles/esp32s3-infrastructure/fixtures/bootfs/board.json"
