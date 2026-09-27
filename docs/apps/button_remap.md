# Remap Front Buttons

## Purpose and classification

Remap Front Buttons is the RiscRTE settings application for assigning the four logical front-button roles Back, Confirm, Left, and Right to physical front buttons. It is classified as a foundational System App because it configures the device's core input mapping.

Manifest metadata:
- version **1.0.0**
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

Up invokes `reset_defaults`, displays either **Default mapping restored** or **Could not save defaults**, redraws once, and exits. Down cancels and exits without applying the pending mapping. The chrome labels these actions **Reset** and **Cancel**.

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

## Source and interface identities

At upstream commit `525e32689203502a7b22f6350b7ef04f272271db`:
- `Apps/button_remap.c`: `e9864c464ea9c3f1919d275de31deccaa19c2acc`
- `Apps/button_remap.json`: `23b2d0c1bfa48ec1bdfb6971f3c670cba5936add`
- `T5ButtonRemapApi.h`: `792ca01aac6055d0a2a707ac01e4df54063d2b64`
- current `T5AppApi.h`: `fda810300de5cadff16e81efd42ba7efff8fe33b`
- `T5UiApi.h`: `ef09b405fc2518ee7ecf039f8b251939d80b14a6`
