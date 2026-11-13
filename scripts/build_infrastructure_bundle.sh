#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
: "${RISC_RUNTIME_ROOT:?Set RISC_RUNTIME_ROOT to the pinned runtime checkout}"
: "${IDF_PATH:?Source the pinned ESP-IDF6.1 export.sh first}"
: "${IDF_TOOLS_PATH:?Set the installed ESP-IDF tools path}"
: "${INFRA_OUTPUT:?Set a new output bundle directory}"
build="${INFRA_NATIVE_BUILD:-$root/../native-build}"
export IDF_COMPONENT_MANAGER=1
idf.py -C "$RISC_RUNTIME_ROOT/idf" -B "$build" -D "RISC_PRODUCT_PROFILE=$root/profiles/esp32s3-infrastructure/native.cmake" reconfigure
cmake --build "$build" -- -j1
# IDF's boot constructor aborts if a strong new-I2C implementation and the
# legacy driver are both linked. Check the actual ELF before packaging.
grep -qx 'CONFIG_SCCB_HARDWARE_I2C_DRIVER_LEGACY=y' "$build/sdkconfig"
xtensa-esp32s3-elf-nm --defined-only "$build/riscrte_native.elf" > "$build/camera-symbols.txt"
grep -Eq ' [Tt] SCCB_Init$' "$build/camera-symbols.txt"
grep -Eq ' [Tt] i2c_driver_install$' "$build/camera-symbols.txt"
if grep -Eq ' [Tt] i2c_acquire_bus_handle$' "$build/camera-symbols.txt"; then
  echo "Conflicting I2C driver linked in camera firmware" >&2
  exit 1
fi
python "$root/scripts/package_infrastructure.py" --runtime "$RISC_RUNTIME_ROOT" --native-build "$build" --idf "$IDF_PATH" --cc "$(command -v xtensa-esp32s3-elf-gcc)" --output "$INFRA_OUTPUT" "$@"
