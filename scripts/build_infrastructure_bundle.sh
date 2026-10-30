#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
: "${RISC_RUNTIME_ROOT:?Set RISC_RUNTIME_ROOT to the pinned runtime checkout}"
: "${IDF_PATH:?Source the pinned ESP-IDF6.1 export.sh first}"
: "${IDF_TOOLS_PATH:?Set the installed ESP-IDF tools path}"
: "${INFRA_OUTPUT:?Set a new output bundle directory}"
build="${INFRA_NATIVE_BUILD:-$root/../native-build}"
export IDF_COMPONENT_MANAGER=0
idf.py -C "$RISC_RUNTIME_ROOT/idf" -B "$build" -D "RISC_PRODUCT_PROFILE=$root/profiles/esp32s3-infrastructure/native.cmake" reconfigure
cmake --build "$build" -- -j1
python "$root/scripts/package_infrastructure.py" --runtime "$RISC_RUNTIME_ROOT" --native-build "$build" --idf "$IDF_PATH" --cc "$(command -v xtensa-esp32s3-elf-gcc)" --output "$INFRA_OUTPUT" "$@"
