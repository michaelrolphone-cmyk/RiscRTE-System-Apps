#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${OUTPUT_DIR:-$(mktemp -d /tmp/portable-file-setup.XXXXXX)}"
mkdir -p "$output"
flags=(-std=c11 -g -Wall -Wextra -Werror -Wpedantic -Ilib/PortableApps/include)
if [[ "${SANITIZE:-0}" == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
fi
"${CC:-cc}" "${flags[@]}" lib/PortableApps/src/PortableFileSetup.c test/native_apps/portable_file_setup_test.c -o "$output/portable-file-setup-test"
"$output/portable-file-setup-test"
python - <<'PY'
import hashlib, json
from pathlib import Path
root = Path('lib/PortableApps')
source = json.loads((root / 'SOURCES.json').read_text())['RiscBluetoothSessionSetupV1.h']
assert source['commit'] == '09b56e84e723368d10c4f14cec494e4da0bbead0'
assert source['path'] == 'sdk/driver/RiscBluetoothSessionSetupV1.h'
assert hashlib.sha256((root / 'include/RiscBluetoothSessionSetupV1.h').read_bytes()).hexdigest() == source['sha256']
print('canonical setup header provenance passed')
PY
