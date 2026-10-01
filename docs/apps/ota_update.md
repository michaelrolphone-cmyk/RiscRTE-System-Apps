# Firmware Update

## Purpose and scope

Firmware Update is the foundational OTA firmware-update workflow. Upstream version **1.0.1** requires firmware **1.1.22** and builds as `ota_update.elf`.

## Confirmed workflow

The app obtains `T5AppApi`, `T5OtaApi` v1, and `T5UiApi` v1, renders a checking screen, and calls `check_for_update()`. A failed check is shown with the numeric `t5_ota_result_t` code. If `is_update_newer()` is false, it reports that the device is current.

For an available update, `latest_version` is read into a 32-byte buffer; failure falls back to `Available`. Back cancels. Only a semantic Confirm event starts `install_update`; body/header taps cannot approve an update. Shared UI action controls resolve orientation and button mapping.

During installation, a provider callback reads `processed_size` and `total_size` and redraws byte and percentage progress. Installation failure is shown with its numeric result code. Success renders completion, polls up to 60 times at 50 ms, then calls the OTA provider's explicit `restart_after_update`.

## Host interfaces, network behavior, and errors

`T5OtaApi` v1 defines OK, no-update, HTTP error, JSON parse error, older-update error, internal-update error, OOM, and unavailable results. The header explicitly states that network selection is firmware-owned and these calls are exposed while the native app is active after Wi-Fi has already been connected by the firmware wrapper. The app contains no update endpoint, HTTP implementation, JSON parser, partition writer, or general reset primitive.

`T5UiApi.render_list` draws the one-row status/confirmation views; `poll_event` handles confirmation, Back and exit. Raw `T5AppApi.poll` remains used for result dismissal and the restart wait. Missing required APIs/functions causes immediate return. No app-owned persistent storage or file format is present.

## Source and build provenance

Source and manifest synchronized from Reader `1e0188c1ff0234dd33fe054c9a6fb4fde36596df`:
- `Apps/ota_update.c`: `2cd2738da338dbf52f28aaad611def8aba1bad43`
- `Apps/ota_update.json`: `7e43fc14f94b59bd02b3d46f937f9d6df08495e7`

The SDK/ABI baseline remains independently pinned in `sdk/baseline.json`.
The matching app host fixture is pinned there to the inspected Reader master.
Published version 1.0.1 is copied unchanged, with canonical ELF identity from
release-index `572746f4fcf3fde19947a066b7e5c8028cd76d21` recorded in `sdk/release-baseline.json`.
Host fixtures exercise app/provider behavior; firmware touch/orientation dispatch
and hardware operation are not qualified by this external repository run.
