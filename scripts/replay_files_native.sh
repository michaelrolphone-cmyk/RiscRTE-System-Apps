#!/usr/bin/env bash
# Internal offline replay; no network, publication, device or original-input writes.
set -euo pipefail
: "${FILES_ROOT:?Verified final Files source tree required}"
: "${FILES_UTILITIES:?Verified f057 Utilities plus final Files background required}"
: "${EFFECTS_HOME:?Verified ba88 Home plus exact effects handoff required}"
: "${HOME_SYSTEM:?Verified bb0 System plus PortableResidentSettingsEffects.h required}"
: "${HOME_UTILITIES:?Verified 4fd Home dependency inputs required}"
: "${BOOTFS_TOOLS:?Verified ab9 Watch scripts directory required}"
: "${CC_TARGET:?Exact esp32s3 GCC15 wrapper required}"
: "${OUTPUT:?New isolated output directory required}"
: "${FILES_AUTHORITY:?Verified original-product-authority.json required}"
: "${EXPECTED_PINS_SHA:?Final internal replay pins digest required}"
INPUTS=${INPUTS:-/workspace/shared/buttons-idle-composition-20261101/inputs}
RUNTIME=${RUNTIME:-/workspace/shared/scanner-key-race-repair-20261101/Runtime}
X4_RUNTIME=${X4_RUNTIME:-/workspace/shared/buttons-idle-composition-20261101/published/RiscRTE-6a3b3d46f5b2857eacc6e9863799dec1f466b036}
PYTHON=${PYTHON:-python3}
export FILES_ROOT FILES_UTILITIES RUNTIME CC_TARGET EXPECTED_PINS_SHA FILES_AUTHORITY
"$PYTHON" - <<'PY'
import os,json,hashlib,pathlib
root=pathlib.Path(os.environ['FILES_ROOT']);p=root/'docs/files-native-replay-source-pins.json'
sha=lambda f:hashlib.file_digest(f.open('rb'),'sha256').hexdigest()
assert sha(p)==os.environ['EXPECTED_PINS_SHA'],'Replay pins differ'
assert sha(pathlib.Path(os.environ['FILES_AUTHORITY']))=='7c1efc1c6847e2b6e4b4da60ec31be53fde17ad4d50a126e7987307c610f291e','Original authority witness differs'
v=json.loads(p.read_text());roots={'files':root,'utilities':pathlib.Path(os.environ['FILES_UTILITIES']),'runtime':pathlib.Path(os.environ['RUNTIME'])}
for role,files in v['sources'].items():
 for n,h in files.items():
  rel=pathlib.Path(n);assert not rel.is_absolute() and '..' not in rel.parts
  f=roots[role]/rel;assert not f.is_symlink() and sha(f)==h,(role,n)
assert sha(pathlib.Path(os.environ['CC_TARGET']))==v['compiler_sha256']
PY
mkdir "$OUTPUT"
"$PYTHON" "$FILES_ROOT/scripts/qualify_files_scene.py" --runtime "$RUNTIME" --utilities "$FILES_UTILITIES" --target-cc "$CC_TARGET" --output "$OUTPUT/qualification" > "$OUTPUT/qualification.log" 2>&1
"$PYTHON" "$FILES_ROOT/scripts/link_files_scene.py" --qualification "$OUTPUT/qualification" --version 1.6.0 --alarm-api 1 --cc "$CC_TARGET" --output "$OUTPUT/files" > "$OUTPUT/link.log" 2>&1
# The authority record is the original exact product evidence, delivered as plain JSON.
: "${FILES_AUTHORITY:?Verified original-product-authority.json required}"
"$PYTHON" "$FILES_ROOT/scripts/prepare_files_selections.py" --authority "$FILES_AUTHORITY" --linked "$OUTPUT/files" --parser "$OUTPUT/qualification/manifest-test" --output "$OUTPUT/selections"
for lane in watch wave; do
 flags=();if [[ "$lane" == wave ]];then flags+=(--serial-transfer-mode);fi
 for pass in first repeat; do
  "$PYTHON" "$FILES_ROOT/scripts/build_files_effects_home.py" --home "$EFFECTS_HOME" --system "$HOME_SYSTEM" --utilities "$HOME_UTILITIES" --pins "$FILES_ROOT/docs/files-home-effects-source-pins.json" --cc "$CC_TARGET" --output "$OUTPUT/home/$lane/$pass" "${flags[@]}" > "$OUTPUT/home-$lane-$pass.log" 2>&1
 done
done
"$PYTHON" - "$FILES_ROOT" "$OUTPUT" <<'PY'
import json,hashlib,pathlib,sys
r,o=map(pathlib.Path,sys.argv[1:]);p=json.loads((r/'docs/files-native-replay-source-pins.json').read_text())
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
assert sha(o/'files/file_browser.elf')==p['target_sha256'],'Canonical target differs'
for lane in ('watch','wave'):
 for run in ('first','repeat'):assert sha(o/'home'/lane/run/'default.elf')==p['home_sha256'][lane],(lane,run)
PY
"$PYTHON" "$FILES_ROOT/scripts/assemble_files_products.py" --inputs "$INPUTS" --selections "$OUTPUT/selections" --home-packages "$OUTPUT/home" --home-pins "$FILES_ROOT/docs/files-home-effects-source-pins.json" --bootfs-tools "$BOOTFS_TOOLS" --output "$OUTPUT/products" > "$OUTPUT/assembly.log" 2>&1
for mode in normal sanitized; do
 flags=();if [[ "$mode" == sanitized ]];then flags+=(--sanitize);fi
 "$PYTHON" "$FILES_ROOT/scripts/test_files_product_admission.py" --runtime "$RUNTIME" --x4-runtime "$X4_RUNTIME" --assembly "$OUTPUT/products" --source-pins "$FILES_ROOT/docs/files-product-runtime-source-pins.json" --output "$OUTPUT/admission-$mode" "${flags[@]}" > "$OUTPUT/admission-$mode.log" 2>&1
done
printf 'Private product replay complete: %s\n' "$OUTPUT"
