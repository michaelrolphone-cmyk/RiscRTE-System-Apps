#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="$(cd "${1:?Pass the pinned minimal RiscRTE checkout}" && pwd)"
expected=a3d23da9cdc1b3a66c6429f29781856fa7fc8f75
[[ "$(git -C "$runtime" rev-parse HEAD)" == "$expected" ]]
# Never silently test uncommitted runtime behavior against the named pin.
git -C "$runtime" diff --exit-code HEAD --
cmp "$root/Apps/paper_space/sdk/RiscRuntimeV1.h" "$runtime/sdk/app/RiscRuntimeV1.h"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
app="$root/Apps/paper_space"
sanitizers=(-fsanitize=undefined -fno-sanitize-recover=all)
incs=(-I"$app" -I"$app/sdk")
cc -std=c11 -Wall -Wextra -Werror "${sanitizers[@]}" "$root/test/paper_space/cleanup_test.c" -o "$build/cleanup-test"
"$build/cleanup-test"
"$build/cleanup-test" retry
flags=(-std=c11 -Wall -Wextra -Werror -g -fPIC -fvisibility=hidden -shared)
for mode in navigation both touch; do
  defs=()
  [[ "$mode" != touch ]] && defs+=(-DPAPER_SPACE_NAVIGATION)
  [[ "$mode" != navigation ]] && defs+=(-DPAPER_SPACE_TOUCH)
  cc "${flags[@]}" "${sanitizers[@]}" "${defs[@]}" "${incs[@]}" "$app/main.c" "$root/test/paper_space/catalog.c" -o "$build/$mode-default.elf"
done
python - "$app/catalog.example.json" "$build/preview-catalog.c" <<'PYEXAMPLE'
import json,sys
from pathlib import Path
items=json.loads(Path(sys.argv[1]).read_text())
rows=['{'+json.dumps(x['label'])+','+json.dumps(x['path'])+'}' for x in items]
Path(sys.argv[2]).write_text('#include "PaperSpace.h"\nconst paper_space_item paper_space_items[]={'+','.join(rows)+'};\nconst unsigned paper_space_item_count='+str(len(items))+';\n')
PYEXAMPLE
cc "${flags[@]}" "${sanitizers[@]}" -DPAPER_SPACE_NAVIGATION "${incs[@]}" "$app/main.c" "$build/preview-catalog.c" -o "$build/preview-default.elf"
cp "$build/navigation-default.elf" "$build/default.elf"
rm "$build/navigation-default.elf"
for slot in 0 1 2; do
  names=(display navigation touch)
  caps=(display.output input.navigation input.touch.raw)
  cc "${flags[@]}" "${sanitizers[@]}" "${incs[@]}" -DTEST_SLOT="$slot" -DTEST_ID="\"${names[$slot]}\"" \
    -DTEST_CAPABILITY="\"${caps[$slot]}\"" "$root/test/paper_space/provider.c" -o "$build/${names[$slot]}.elf"
done
cc "${flags[@]}" "${sanitizers[@]}" "${incs[@]}" "$root/test/paper_space/child.c" -o "$build/child.elf"
mkdir "$build/test-sdk"
cp "$app/sdk/"{RiscDisplayOutputV1.h,RiscInputNavigationV1.h,RiscTouchV1.h} "$build/test-sdk/"
c++ -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -g -rdynamic "${sanitizers[@]}" \
  -I"$runtime/sdk/driver" -I"$runtime/sdk/app" -I"$runtime/src" -I"$runtime/sdk/hardware" -I"$build/test-sdk" \
  -I"$runtime/test/drivers/stubs" -I"$runtime/lib/ArduinoJson/src" \
  "$runtime/src/bootstrap/Json.cpp" "$runtime/src/bootstrap/Board.cpp" "$runtime/src/bootstrap/Runtime.cpp" \
  "$runtime/src/runtime/drivers/ProviderGraphV2.cpp" "$runtime/src/runtime/drivers/ProviderModuleV2.cpp" \
  "$root/test/paper_space/runtime_test.cpp" -ldl -o "$build/test"
"$build/test" "$build" "$root/build/paper-space/previews"
