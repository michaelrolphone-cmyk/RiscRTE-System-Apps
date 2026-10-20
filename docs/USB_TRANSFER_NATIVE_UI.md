# USB SD Transfer native-color source qualification

This opt-in source profile lays out and rasterizes the dedicated USB app in
capability-reported RGB565 pixels. It does not select a product build, change a
released artifact or install anything. The legacy app manifest remains 0.1.11;
the new build receipt identifies `usb-transfer-native-v1` separately. Physical
USB host/card behavior and product delivery remain unqualified.

## Native layout and preserved behavior

`--native-ui` selects `PORTABLE_USB_TRANSFER_NATIVE_UI`, Nova native primitives
and the existing 0/1/2 font subset. No new Runtime capability or ABI is added.
Coordinates and hit targets come from `Apps/UsbTransferLayout.h`. The two
controls anchor to the bottom edge with fixed native glyph sizes. Short screens
use 32px controls and 12px body type; ordinary screens use 44px controls and 15px
body type. The title uses the existing 14px Orbitron face, or the existing 10px
face on narrow displays. Text is measured and wrapped; no completed viewport,
glyph-fit squash or intermediate 480×800 bitmap is scaled.

The app's native selection is a hidden, read-only RGB565 query. Paper and
feature-off color retain the original rendering and hit-test paths. The only
shared presenter change admits RGB565 displays at least 160×160 when both the
dedicated USB app and native UI selectors are enabled. The existing unselected
240×240 minimum and all monochrome eligibility remain unchanged. The production
adapter, toolbar, provider and shared build helpers are not edited.

The USB state machine, service cadence and preparation sequence are unchanged.
The same foreground input/presentation path still owns the asynchronous display
wait and ordered contact stream. In particular:

- Start remains explicit and transition input must reach neutral.
- A Cancel completed during the Preparing frame wins before SD preparation.
- Each preparation step follows the settled screen and an input poll.
- Connected service waits remain 1ms; other foreground waits remain 2ms.
- Suspend/reset/unconfigure do not establish cable removal.
- Configured Stop refusal opens a separate Cable Removed confirmation.
- Keep Transferring and Back never confirm cable removal.
- Successful end precedes checked grant release and navigation.
- Tokenless or uncertain custody and failed release continue blocking exit.
- Local SD readiness is displayed only from the provider's confirmation.

On compact screens the complete concise unplug/data-loss instruction takes
priority over provider diagnostic detail. Larger screens also show the bounded
provider reason. Diagnostic text is visibly ellipsized when necessary; the
underlying detail and checkpoint data are unchanged.

## Qualification commands

Use the canonical MSC header from
`michaelrolphone-cmyk/T5S3-Reader`, commit
`322fc9c0fbe6713a9fc0f3175a19550970682eaa`,
`sdk/driver/RiscUsbDeviceMscV1.h`, SHA-256
`d55a9340c3ff64e8679c4d9332812f073c30b8aa08ae3e3fb04f9010cf517ed6`.

```sh
python3 scripts/test_usb_transfer_native.py \
  --msc-sdk /path/to/msc-sdk --output-dir /tmp/usb-native
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
python3 scripts/build_portable_usb_transfer.py --native-ui --touch-only \
  --msc-sdk /path/to/msc-sdk --output-dir /tmp/usb-native-target
```

The host qualification runs the actual app and shared raster/adapter with
capability-provider fixtures at 160×160, 240×240, 240×280,320×240 and 410×502.
Each of 31 lifecycle cases runs with navigation/touch-only, direct/snapshot,
and ordinary/ASan+UBSan execution: 1240 native-color cases. Another 496 paper
cases compare selected/unselected native flags, and 248 feature-off RGB cases
retain the old presentation. Frame digests must agree across direct/snapshot
and sanitized execution. 124 additional runs compare every paper and feature-off
frame to pristine System commit
`ff6151285ee41bdbeff75ebd31ee2621d524e899`.

The same command checks 16 selected/unselected admission boundaries and two
normal/sanitized instrumented-view runs. Each view run checks 90 state/confirmation
frames, complete warning text, glyph/rectangle bounds, all pixels in both hit
maps and a wider dimension sweep. Actual raster PNGs cover six lifecycle
scenarios at every native size. Logs, command lists, source hashes and result
JSON are emitted alongside those captures.

These fixtures exercise explicit preparation cancellation, host eject, cable
confirmation/cancel, suspend, retained cleanup, absent media, refused/retried
begin, grant-release failure, tokenless retained begin, malformed preparation
and diagnostics suffixes, host-sector observations, prompt eject navigation,
checkpoint truncation and Runtime authority revocation. They do not certify
physical CSW timing, host compatibility, media integrity, remount or wiring.

## Historical delivered binding

The shipped 0.1.11 version has at least two distinct artifacts:

- Waveshare `.35`: 64808-byte compact ELF, SHA-256
  `6cfbd343633677a2498456d2a6664632ed83f2c7e6f966c7cba4349bfbc28e86`;
  touch-only, non-resident, three required capabilities.
- X4 `.118`: 72832-byte compact ELF, SHA-256
  `cfff8f91f169cd555b5f7613c1f860cfd6b22619c528396d28c662d0fc97e6d6`;
  navigation/resident-client policy plus tagged alarm API 2.

Both receipts bind clean System commit
`5f224abf9a8693a443c94c74d282e97f79836bf9`, GCC 8.4.0
`esp-2021r2-patch5` and 299 exact source hashes. Independently downloaded factory
images reproduce the archive ELF and manifest identities when their SPIFFS
stores are extracted read-only. The app, manifest and PaperPresentation header
match the current baseline; the shared adapter, presenter and recipe differ.
Therefore current-source parity is a separate result from shipped-artifact
provenance. The historical original CLI and compiler-binary hash are unavailable;
no historical rebuild-byte parity is claimed.

Official evidence releases:
- https://github.com/michaelrolphone-cmyk/RiscRTE-Wavshare-S3-Touch-AMOLED-2.06/releases/tag/test-t053-usb-0.1.35
- https://github.com/michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO/releases/tag/test-t053-usb-0.1.118

This source slice does not resume the paused generic-storage migration, change
MSC custody/security policy, modify product/.64 recipes or release new assets.
