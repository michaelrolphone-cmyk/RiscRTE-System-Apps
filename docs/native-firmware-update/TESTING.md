# Native Firmware Update checks

The standalone native selector is `PORTABLE_UPDATE_NATIVE_UI` together with
`PORTABLE_UPDATE_FIRMWARE=1`, an explicit `PORTABLE_UPDATE_VERSION`, Nova,
`PORTABLE_NATIVE_CANVAS`, and `PORTABLE_NATIVE_CUSTODY_FENCE`. The ordinary
selector-off recipes and their released product bindings are unchanged.

The native app uses the existing shared native list renderer. Layout, glyph and
icon rasterization, pagination, footer controls, header Back, and hit testing all
use capability-reported width and height. A completed 240-pixel viewport is not
scaled. Presentation owner 2 remains attached to its actual submitted native
frame; no new shared adapter owner or scene/intent architecture is introduced.

## Controller and raster matrix

```sh
python scripts/test_native_firmware_update.py --runtime ../runtime \
  --output build/native-firmware-update/controller/normal --captures
python scripts/test_native_firmware_update.py --runtime ../runtime \
  --output build/native-firmware-update/controller/sanitized --sanitize
python scripts/test_native_firmware_update.py --runtime ../runtime \
  --output build/native-firmware-update/controller/snapshot --snapshot --captures
python scripts/test_native_firmware_update.py --runtime ../runtime \
  --output build/native-firmware-update/controller/snapshot-sanitized --snapshot --sanitize
```

Every mode runs 56 cases at 240×240, 410×502, 160×160, 320×180 and 502×410,
with both raw-touch rotations. That is 2,240 separate executions. The production
controller, adapter, native glyph/icon raster and hit rectangles compile directly
from the source under test; synthetic hardware/service providers supply bounded
input and fault stimuli. Tests verify every pixel's hit classification, rendered
glyph/icon ink, paged releases and catalog/confirmation/progress/failure/cleanup/
restart scenes.

Safety cases cover explicit firmware capability selection with no apps fallback,
changed release identity before confirmation or installation, malformed provider
strings, USB-only releases, repeated entry, Back and raw Cancel at the transition
to activation-ready, pending-frame input suppression, frame/token ownership,
failed or superseded presentation, timeout, failed submission, failed radio or
HTTP cleanup, and retained invocation behavior without subsequent callbacks.
Malformed displayed rows are unavailable; they never become an install candidate.
The final selected-row read is validated again before it opens confirmation.

Captures are deliberately bounded to the explicit scene case at rotation zero.
Source hashes, generated-fixture hashes, actual compiler commands and per-case
results accompany each receipt. Source changes during a run invalidate the
qualification. Synthetic tests do not execute a hardware update.

## Overlay and shared updater preservation

```sh
python scripts/test_native_firmware_update_overlays.py --runtime ../runtime \
  --output build/native-firmware-update/overlays-normal
python scripts/test_native_firmware_update_overlays.py --runtime ../runtime \
  --output build/native-firmware-update/overlays-sanitized --sanitize
python scripts/test_native_firmware_update_regressions.py \
  --output build/native-firmware-update/regressions
python scripts/test_native_app_store.py --runtime ../runtime \
  --output build/native-firmware-update/appstore
```

Overlay tests run the real native Quick Controls/alarm and atomic-preference
controllers over a firmware confirmation screen. Five cases, five dimensions,
two raster modes and normal/sanitized builds give 100 overlay executions. The
underlying confirmation, selection and update authority must survive unchanged;
old pending-output input cannot authorize installation.

The shared regression runner executes all 312 existing legacy-layout cases
without writing a bulk raster archive. It also executes the original catalog
mutation, 46 paired-cohort cases, 29 service cases per update kind, and 38
controller cases per kind/rotation/view in normal and sanitized modes. Provider,
bank/cohort and receipt contracts are unchanged and independently bound in the
target build record. These results do not establish a physical paired-bank
transaction or durable receipt from a device.

The existing twelve selector-off ELF comparisons and full retained App Store and
Firmware Update recipe comparisons run unchanged in the CI workflow. They compare
actual target bytes with the immutable legacy baseline using the same compiler.
The sibling native App Store matrix remains a separate preservation check.

## Strict target and admission

See [README.md](README.md) for the standalone target recipe and immutable external
inputs. `native-firmware-update.yml` runs the controller matrix, overlays,
regressions, selector-off controls, target negative tests, actual Xtensa build and
pinned Runtime admission checks. It has read-only repository permissions and no
publication, installation or product-recipe step.

Passing source and host/ELF checks do not qualify installed provider bindings,
network transport, physical memory fit, touch-panel behavior, activation or
restart on any device. Hardware and product delivery remain separate.
