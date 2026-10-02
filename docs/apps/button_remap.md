# Remap Front Buttons

## Purpose and classification

Remap Front Buttons is the RiscRTE settings application for assigning the four logical front-button roles Back, Confirm, Left, and Right to physical front buttons. It is classified as a foundational System App because it configures the device's core input mapping.

Manifest metadata:
- version **1.0.2**
- minimum firmware **1.1.24**
- artifact `button_remap.elf`
- icon `solid:f11c`
- categories `Settings`, `Input`

## Host interfaces

### `T5AppApi`

The app obtains ABI version 1 and requires `poll` and `set_back_exits_app`. Input is polled every 50 ms. During the mapping workflow it calls `set_back_exits_app(false)` so the Back role can be captured as input, then restores `set_back_exits_app(true)` before returning.

The source consumes Back, Confirm, Left, Right, Up, and Down button masks from `t5_app_input_t`.

### `T5ButtonRemapApi`

The app obtains API version 1 and requires `read_mapping`, `apply_mapping`, and `reset_defaults`. The provider interface defines four roles and `t5_button_remap_mapping_t.role_to_hardware[4]`.

The current provider mapping is copied to `original`; a separate `pending` mapping is assembled by the app. Persistence and hardware-remap semantics belong to the provider and are not specified by this app.

### `T5UiApi`

The app obtains UI API version 1 and requires `render_list` and `hit_test`. Four list rows are rendered in Back, Confirm, Left, Right order.

## Workflow and input behavior

The pending mapping is initialized to `0xFF`, rendered as **Unassigned**, and the Back role starts selected. A row tap changes the role being assigned.

When one of the four logical role buttons is pressed, `physical_from_input` resolves the physical button number from the original mapping. A physical button already used by another pending role is rejected with **Button N is already assigned**. Otherwise it is assigned to the selected role and the workflow advances.

After all four roles have assignments, `apply_mapping` is called. Success exits. Failure returns selection to the last role and shows **Could not save button mapping** so the user can try again.

Up invokes `reset_defaults`, displays either **Default mapping restored** or **Could not save defaults**, redraws once, and exits. Down cancels and exits without applying the pending mapping. The subtitle explicitly labels these actions **Side Up: Reset | Side Down: Cancel**. Front-button action hints are blank; the initial status says **Press a front button for the selected role**.

## Failure handling

Confirmed behavior:
- missing required App, remap, or UI interfaces causes an immediate return;
- failure to read the original mapping causes an immediate return;
- duplicate physical assignments are rejected before apply;
- apply failure remains in the workflow;
- reset-defaults failure is reported before exit.

There is no app-owned retry policy beyond the user's subsequent input, no rollback protocol beyond retaining the pending mapping after apply failure, and no app-defined persistence format.

## State, storage, network, and limits

Working state is static/in-memory: original and pending four-entry mappings, four UI rows, four 24-byte value buffers, one 96-byte status buffer, and the current-role index.

The app performs no direct file-system access and no network I/O.

## Source and build provenance

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` supplies the matching application source and the manifest is synchronized at version **1.0.2**. Source blob `ead8a4d0a346fc42b27bbf525c1ec91855d08c5f`; manifest blob `2ed1cbc32dbf261f229a57e68e61e78d4e73112b`. The SDK/ABI baseline remains independently pinned in `sdk/baseline.json`.

Reader published release [`app-button_remap-v1.0.2`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-button_remap-v1.0.2/application-button_remap-1.0.2-xtensa-esp32s3.rte.zip) contains a `4881`-byte package with SHA-256 `34900287855c4ae0f14d61defb99bca05dc539fe9474fedb8eae5aea252135ab`. The downloaded package contains `button_remap.elf` (3812 bytes, SHA-256 `46ff711c7a3cf9ca5ed2bab41088b57e0821dde6c5be0e3abb891c6a5f17f154`). Release metadata and the downloaded workflow artifact agree; the ELF identity matches the earlier 1.0.1 release, so no additional bump was needed. The release was produced from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; these app inputs are unchanged at current Reader master.

Host fixtures exercise app/provider behavior; device operation and U1 runtime readiness are not established by this evidence.
