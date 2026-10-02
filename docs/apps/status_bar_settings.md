# Customize Status Bar

## Purpose and scope

Customize Status Bar is a foundational appearance/settings app for activating firmware-defined status-bar items. Upstream version **1.0.2** requires firmware **1.1.24** and builds as `status_bar_settings.elf`.

## Confirmed workflow

The app obtains `T5AppApi`, `T5StatusBarApi` v1, and `T5UiApi` v1. It reads up to `T5_STATUS_BAR_ITEM_COUNT` provider items and renders their provider-supplied label/value pairs under **Customize Status Bar** / **Reader status information**.

Back or `exit_requested` exits. Up/Left and Down/Right move through rows using the shared UI index helpers. Confirm activates only on a newly pressed edge: holding Confirm across multiple polls does not repeatedly toggle the selected item. Releasing and pressing again activates it again. Tapping a valid row selects, activates, and redraws immediately.

## Host interfaces and limits

`T5StatusBarApi` v1 exposes `item_count`, `item_get`, and `item_activate`. Its interface defines **7** maximum items and 64-byte label/value fields. The app uses static arrays of exactly that count and truncates a larger provider count. It uses `T5AppApi.poll` plus `T5UiApi.render_list`, `hit_test`, `next_index`, and `previous_index`.

The source does not define how `item_activate` changes or persists firmware state. It does not access settings files, network services, or external hardware directly. Missing required APIs/functions causes immediate return. A failed individual `item_get` leaves that zero-initialized row without a separate error screen.

## Source identity

Audited against Reader master `ca66db298c2e735f45e5029083a9bfbd7b6740bd`: source `3a9167d761b6cbc0af01e88e4feec054b7375e88`, manifest `d406b3b5c1365c90b148037c0d85b74dbeaf8b17`, `T5StatusBarApi.h` `d0cd8e4d77671a596ffa2b1a05daf07b3a0083b2`, and `T5UiApi.h` `ef09b405fc2518ee7ecf039f8b251939d80b14a6`.

The upstream held-confirm regression fixture is pinned to this source commit and verifies one activation per press across repeated held polls. The independent Xtensa build reproduces the existing published 1.0.2 ELF byte-for-byte: 2,920 bytes, SHA-256 `ef294e50c5007b301245b0a5aaf3e2abcfb19328a91ce24ab718af7325ad4e0b`. This host/build evidence does not establish U1 ZIP/runtime compatibility or external cutover readiness.
