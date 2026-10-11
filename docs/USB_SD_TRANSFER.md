# Dedicated USB SD Transfer

`Apps/usb_sd_transfer.c` owns the generic `usb.device.msc@1` session. The
computer is the USB host, and the device shares its SD card. The app contains
no USB stack, PHY, GPIO, SD command or filesystem implementation. It requires
only display output, raw touch, navigation and USB MSC. Start is explicit.

The paper screen distinguishes waiting, connected, suspended, ejected,
disconnected, unavailable media and retained cleanup. It names the two X4
boot-log files to copy. End success means checked ownership release, while
only a provider `local_media_ready` status claims that local media is ready.
A safely released but absent card does not strand the user in the transfer app.

The owner polls every 2 ms and during asynchronous display presentation. The
app requires an async-capable display so a synchronous panel wait cannot
starve USB. UI callbacks never infer unplug from suspend or missing SOF.
Configured Stop first asks the provider for ordinary cleanup. Refusal opens a
separate cable-removed confirmation; Home/Back and the cancel button cannot
produce that confirmation. Fresh neutral input is required after transitions.
Navigation stays blocked while a session, uncertain begin, or failed grant
release remains. Retained faults permit explicit end retry. A missing token
on retained/successful begin holds the invocation and grant indefinitely.

The app owns no sleep API. A terminal input/display failure cannot return
while the app still owns USB custody. The provider's successful end precedes
capability release, and release precedes Home/Back handoff.

## Quick Actions and catalog

`--quick-usb-transfer` selects a paper-only row below the existing six tiles;
brightness, the on/off button, notifications and all tiles keep their existing
positions. This flag adds only a launch intent, never an MSC grant. The catalog
bound grows from 17 to 18 within the existing 128-entry app storage and two
15-entry pages. `--quick-usb-transfer` reserves Springboard 1.7.17, while the
unselected version remains 1.7.16. Fresh remote checks on 2026-10-09 found
System HEAD `0b5aa15a57b7ba248840ca8ab9dee0be13267c19` at Springboard 1.5.0,
with highest published matching tag 1.4.9; the selected local base was 1.7.16.

Only rebuilt apps expose this row. For the `.30`-based product candidate,
Springboard is rebuilt; frozen Clock and Settings remain unchanged. USB
Transfer is also the third icon on Springboard page 2.

## Build and verification

Build just the new app with the canonical shared provider header:

```
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
python3 scripts/build_portable_usb_transfer.py \
  --msc-sdk /path/to/Reader/sdk/driver --output-dir /path/to/output
```

The output contains the ELF, four-capability manifest, exact compiler/source/
SDK/ELF receipt and font licenses. It uses the production structural ELF
validator and a bounded import/export allowlist. No whole-cohort build,
publication, device flash or hardware qualification occurs here.

Relevant tests:

- `test_usb_transfer.py --msc-sdk ...`: real app and adapter, provider callback
  fixtures; normal/ASan/UBSan waiting cancel, host eject, explicit confirmation
  and cancel, suspend, retained end retry, absent media, begin refusal/retry,
  failed grant release, tokenless retained begin. Async frame waits check a
  maximum simulated 2 ms gap between owner polls. PNGs retain actual raster
  output for status, recovery and touch-target review.
- `test_quick_usb_transfer.py`: real Springboard and sheet, selected/unselected
  tap/drag, brightness slider/on-off, no USB acquisition in the launch owner.
- `test_touch_scroll_springboard.py`: current paged Springboard and adapter;
  18-entry catalog, page-2 USB target and existing interrupted/repeated flows.
- Existing paper Quick Actions/Home regressions and repository unit tests.

The callbacks simulate host/media events. They do not establish real USB host
compatibility, transfer speed, electrical behavior or physical card remount.

## Transfer 0.1.1: explicit SD preparation

The dedicated app now requires the size/tag/version-checked preparation suffix
`risc_usb_device_msc_api_v1_prepare` (`0x554d5031`, version 1) before calling
begin. A legacy or malformed provider is refused before it can export media.
Successful begin owns a PREPARING lease; it does not claim the USB PHY.

The app renders and completes the Preparing SD frame, then handles input before
calling at most one `prepare_step` per app-loop/input-poll iteration. The
existing display/input service hook only polls the provider. It never advances
SD preparation. Each provider step is bounded by the shared contract; no timing
claim about physical media is inferred from the host test. A completed Cancel
tap captured during the preparing frame wins before the first SD transaction.
Pending preparation is cancellable through checked end. A clean media failure
preserves the provider reason and allows a new Start after cleanup; uncertain
cleanup keeps the lease and blocks navigation until explicit Stop succeeds.

Plain bounded diagnostic lines identify begin, each explicit preparation step,
state changes, cancellation and failures, including the provider's last error.
The UI does not infer a completion percentage from host read/write counters.
Those counters remain host I/O only.

USB Transfer 0.1.1 was reserved after a fresh 2026-10-09 06:04 UTC remote check:
System HEAD remained `0b5aa15a57b7ba248840ca8ab9dee0be13267c19` and no matching
USB Transfer tags existed. Springboard/Home, Runtime and capability grants are
unchanged by this narrow app revision. Build-record `sd_preparation` binds the
explicit-step and settled-screen selection; the canonical SDK hash binds the
complete actual suffix.

The focused fixture now runs 36 normal/ASan/UBSan app-and-adapter cases. Added
cases cover multi-step preparation, cancellation, clean-failure retry,
retained-failure cleanup retry, old/short/bad-tag/bad-version/null-callback
rejection, and Cancel captured during the pending display frame. Provider
doubles assert a settled preparing frame, no preparation during display waits,
and at most one transaction per input poll. Existing transfer/eject/suspend and
unsafe-stop cases remain covered. `preparing.png` is actual rendered output.


`test_usb_prepare_integration.py --reader ... --tinyusb ...` additionally links
this actual app and adapter to the production USB provider and real TinyUSB
state machine, using that provider's independently checked SD/PHY/DCD doubles.
Eight normal/ASan/UBSan cases exercise preparation progress, cancellation,
clean refusal and retained cleanup retry. Every provider storage step checks
that the preparing frame settled first, while repeated status polls prove they
do not advance preparation. The evidence JSON hashes each source actually
compiled, including the external provider and canonical headers.
