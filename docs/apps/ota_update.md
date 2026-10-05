# Firmware Update

## Purpose and scope

Firmware Update is the foundational OTA firmware-update workflow. Upstream version **1.0.2** requires firmware **1.1.22** and builds as `ota_update.elf`.

## Confirmed workflow

The app obtains `T5AppApi`, `T5OtaApi` v1, and `T5UiApi` v1, renders a checking screen, and calls `check_for_update()`. A failed check is shown with the numeric `t5_ota_result_t` code. If `is_update_newer()` is false, it reports that the device is current.

For an available update, `latest_version` is read into a 32-byte buffer; failure falls back to `Available`. Back cancels. Only a semantic Confirm event starts `install_update`; body/header taps cannot approve an update. Shared UI action controls resolve orientation and button mapping.

During installation, a provider callback reads `processed_size` and `total_size` and redraws byte and percentage progress. Installation failure is shown with its numeric result code. Success renders completion, polls up to 60 times at 50 ms, then calls the OTA provider's explicit `restart_after_update`.

## Host interfaces, network behavior, and errors

`T5OtaApi` v1 defines OK, no-update, HTTP error, JSON parse error, older-update error, internal-update error, OOM, and unavailable results. The header explicitly states that network selection is firmware-owned and these calls are exposed while the native app is active after Wi-Fi has already been connected by the firmware wrapper. The app contains no update endpoint, HTTP implementation, JSON parser, partition writer, or general reset primitive.

`T5UiApi.render_list` draws the one-row status/confirmation views; `poll_event` handles confirmation, Back and exit. Raw `T5AppApi.poll` remains used for result dismissal and the restart wait. Missing required APIs/functions causes immediate return. No app-owned persistent storage or file format is present.

## Source and build provenance

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` supplies the matching application source and the manifest is synchronized at version **1.0.2**. Source blob `2cd2738da338dbf52f28aaad611def8aba1bad43`; manifest blob `a777ffc968e481fecea8a51b9fa970fb856400d5`. The SDK/ABI baseline remains independently pinned in `sdk/baseline.json`.

Reader published release [`app-ota_update-v1.0.2`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-ota_update-v1.0.2/application-ota_update-1.0.2-xtensa-esp32s3.rte.zip) contains a `5108`-byte package with SHA-256 `ed71c3f7d2de88f48e0758e0000b03cbb35d86e75b91673b3fb2f7a8c9ce31de`. The downloaded package contains `ota_update.elf` (4060 bytes, SHA-256 `159845f5504f74cc60a3464f34968f07b098b916cebd678ee99373fe882d04d5`). Release metadata and the downloaded workflow artifact agree; the ELF identity matches the earlier 1.0.1 release, so no additional bump was needed. The release was produced from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; these app inputs remain unchanged at Reader master `3722a3f44a3294ba5e8adab830807a2523df3b03`.

Host fixtures exercise app/provider behavior; device operation and U1 runtime readiness are not established by this evidence.

## Portable capability build

`PORTABLE_UPDATE_APP` with `PORTABLE_UPDATE_FIRMWARE=1` selects the existing
shared [portable update controller](../PORTABLE_UPDATES.md). It requests only
`software.update.firmware@1`, saved-profile namespace 6 and foreground Wi-Fi.
Catalog rows lacking explicit compatible Runtime OTA metadata show USB install
only; the merged Watch 1.0.0 image cannot be installed through this app.

The initial Check, explicit Cancel/Install confirmation, progress, verified-bank
activation and Restart flows share the established portable presentation.
Ordinary builds without the define retain the Reader implementation below the
compile-time branch and reproduce its published ELF bytes. Portable development
manifest version is 1.1.0; no legacy manifest or production release is changed.

Springboard deployment manifest advances to 1.4.2 for the Firmware Update
launcher entry. The shared Springboard rendering/legacy implementation is unchanged.
