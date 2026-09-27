# Customize Status Bar

## Purpose and scope

Customize Status Bar is a foundational appearance/settings app for activating firmware-defined status-bar items. Upstream version **1.0.0** requires firmware **1.1.24** and builds as `status_bar_settings.elf`.

## Confirmed workflow

The app obtains `T5AppApi`, `T5StatusBarApi` v1, and `T5UiApi` v1. It reads up to `T5_STATUS_BAR_ITEM_COUNT` provider items and renders their provider-supplied label/value pairs under **Customize Status Bar** / **Reader status information**.

Back or `exit_requested` exits. Up/Left and Down/Right move through rows using the shared UI index helpers. Confirm calls `item_activate(selected)` and redraws. Tapping a valid row selects, activates, and redraws immediately.

## Host interfaces and limits

`T5StatusBarApi` v1 exposes `item_count`, `item_get`, and `item_activate`. Its interface defines **7** maximum items and 64-byte label/value fields. The app uses static arrays of exactly that count and truncates a larger provider count. It uses `T5AppApi.poll` plus `T5UiApi.render_list`, `hit_test`, `next_index`, and `previous_index`.

The source does not define how `item_activate` changes or persists firmware state. It does not access settings files, network services, or external hardware directly. Missing required APIs/functions causes immediate return. A failed individual `item_get` leaves that zero-initialized row without a separate error screen.

## Source identity

Audited against upstream `T5S3-Reader` commit `525e32689203502a7b22f6350b7ef04f272271db`: source `b3a9b2e4a1259c1ccb1f5c6b39f747387025dfc1`, manifest `ad5cb1756d4d067237f2cfc77873e86c91673fcf`, `T5StatusBarApi.h` `d0cd8e4d77671a596ffa2b1a05daf07b3a0083b2`, and `T5UiApi.h` `ef09b405fc2518ee7ecf039f8b251939d80b14a6`.

No independent destination build or release artifact has been established yet.
