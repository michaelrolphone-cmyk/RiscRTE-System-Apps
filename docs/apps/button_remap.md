# Remap Front Buttons

## Purpose and classification

Remap Front Buttons is the RiscRTE settings application for assigning the four logical front-button roles Back, Confirm, Left, and Right to physical front buttons. It is classified as a foundational System App because it configures the device's core input mapping.

Manifest metadata:
- version **1.0.1**
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

Source and manifest synchronized from Reader `1e0188c1ff0234dd33fe054c9a6fb4fde36596df`:
- `Apps/button_remap.c`: `ead8a4d0a346fc42b27bbf525c1ec91855d08c5f`
- `Apps/button_remap.json`: `7fed7c136db258c971b7b76320e06f7dce753efc`

The SDK/ABI baseline remains independently pinned in `sdk/baseline.json`.
The matching app host fixture is pinned there to the inspected Reader master.
Published version 1.0.1 is copied unchanged, with canonical ELF identity from
release-index `572746f4fcf3fde19947a066b7e5c8028cd76d21` recorded in `sdk/release-baseline.json`.
Host fixtures exercise app/provider behavior; firmware touch/orientation dispatch
and hardware operation are not qualified by this external repository run.
