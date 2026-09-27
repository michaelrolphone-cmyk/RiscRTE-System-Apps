# Firmware Update

## Purpose and scope

Firmware Update is the foundational OTA firmware-update workflow. Upstream version **1.0.0** requires firmware **1.1.22** and builds as `ota_update.elf`.

## Confirmed workflow

The app obtains `T5AppApi`, `T5OtaApi` v1, and `T5UiApi` v1, renders a checking screen, and calls `check_for_update()`. A failed check is shown with the numeric `t5_ota_result_t` code. If `is_update_newer()` is false, it reports that the device is current.

For an available update, `latest_version` is read into a 32-byte buffer; failure falls back to `Available`. Back cancels. Confirm, or a tap whose shared-UI hit test resolves to a row, starts `install_update`.

During installation, a provider callback reads `processed_size` and `total_size` and redraws byte and percentage progress. Installation failure is shown with its numeric result code. Success renders completion, polls up to 60 times at 50 ms, then calls the OTA provider's explicit `restart_after_update`.

## Host interfaces, network behavior, and errors

`T5OtaApi` v1 defines OK, no-update, HTTP error, JSON parse error, older-update error, internal-update error, OOM, and unavailable results. The header explicitly states that network selection is firmware-owned and these calls are exposed while the native app is active after Wi-Fi has already been connected by the firmware wrapper. The app contains no update endpoint, HTTP implementation, JSON parser, partition writer, or general reset primitive.

`T5UiApi.render_list` draws the one-row status/confirmation views; `hit_test` is used for confirmation taps. Raw `T5AppApi.poll` handles Back/Confirm/exit and result dismissal. Missing required APIs/functions causes immediate return. No app-owned persistent storage or file format is present.

## Source identity

Audited against upstream `T5S3-Reader` commit `525e32689203502a7b22f6350b7ef04f272271db`: source `5f2163083b41aebfd3bb472a7d1148fc3848e833`, manifest `6e4b57b7c1502ad87cb4f9fdd32c1ba54e7b02a5`, `T5OtaApi.h` `257c52496b0b3a2dfc651714b626dd0758af11b7`, and `T5UiApi.h` `ef09b405fc2518ee7ecf039f8b251939d80b14a6`.

No independent destination build or release artifact has been established yet.
