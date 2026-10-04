# Wi-Fi Networks

## Portable 1.1.0 opt-in

The current shared manifest is **1.1.0**. An explicit portable build now provides
app-owned scan/manual entry, connect/status/cancel/retry/disconnect, explicit
Save/Forget, and retained alarm/sleep lifecycle support. See
[Portable Wi-Fi Settings](../PORTABLE_WIFI.md) for the capability, storage and
verification contracts. The legacy Reader branch below is preserved byte-for-byte
inside the non-portable preprocessor branch; the historical release identities
below remain historical evidence, not a new 1.1.0 publication claim.

## Legacy Reader behavior

## Purpose and classification

Wi-Fi Networks is the RiscRTE front end for the firmware-owned wireless network selection workflow. Its manifest identifies it as `wifi_settings.elf`, version **1.0.3**, minimum firmware **1.1.24**, display name **Wi-Fi Networks**, icon `solid:f1eb`, categories `Connectivity` and `Settings`.

The implementation is classified as a foundational System App because it exposes core network configuration used by normal system/app workflows. It does not implement Wi-Fi scanning, credential entry, association, or persistence itself; those operations are delegated to firmware through the System UI and Network APIs.

## User-visible workflow

On entry the app obtains the App, Network, System UI, and UI APIs. If any required interface/function is unavailable, it returns without rendering a workflow.

The app reads the current connection state through `network->wifi_connected()` and then checks whether a previous firmware Wi-Fi-selection request has a result through `system_ui->wifi_take_result()`.

If no result is waiting or the returned cookie does not match `WIFI_COOKIE`, the app requests the firmware Wi-Fi selector with `wifi_request(WIFI_COOKIE)` and returns. The fixed cookie is `0x5749464900000001ULL`.

The app renders a two-row list only when a matching result is available:
1. **Wi-Fi status** — shows whether the selector was cancelled or completed, and whether the device is connected.
2. **Choose another network** — opens the firmware Wi-Fi selector again.

The connected status row sets `T5_UI_LIST_HIGHLIGHT_VALUE` only when the device is connected.

## Navigation and input

The list starts with row 0 selected.

- Up or Left selects the previous row through `ui->previous_index`.
- Down or Right selects the next row through `ui->next_index`.
- A touch hit on row 1 requests the Wi-Fi selector and exits.
- Confirm requests the Wi-Fi selector only when row 1 is selected.
- Back, `exit_requested`, or a failed/ended input poll exits the app.

Input is polled with a 50 ms wait.

## Host APIs and interfaces

### `T5AppApi`

The app obtains `t5_app_api_v1` with `t5_app_get_api(T5_APP_ABI_VERSION)` and requires `app->poll`.

### `T5NetworkApi`

The app obtains `t5_network_api_v1` with `t5_network_get_api(T5_NETWORK_API_VERSION)` and requires `network->wifi_connected`. The app uses this only to read the current connected/not-connected state.

### `T5SystemUiApi`

The app obtains `t5_system_ui_api_v1` with `t5_system_ui_get_api(T5_SYSTEM_UI_API_VERSION)` and requires:
- `wifi_request`
- `wifi_take_result`

`wifi_request` transfers control of network selection to firmware. `wifi_take_result` supplies three outputs used by the app: connected state, cancellation state, and a returned cookie. Version 1.0.2 accepts only a result whose cookie equals `WIFI_COOKIE`; a mismatched result triggers a fresh selector request and immediate return instead of rendering another request’s result.

### `T5UiApi`

The app obtains `t5_ui_api_v1` with `t5_ui_get_api(T5_UI_API_VERSION)` and requires:
- `render_list`
- `hit_test`
- `next_index`
- `previous_index`

The chrome title is **Wi-Fi Networks**, subtitle **Manage the active wireless connection**, and the labels are Back / Select / Up / Down.

## Network behavior

No Wi-Fi scan, SSID enumeration, credential handling, socket I/O, DNS, HTTP, or other endpoint logic exists in this app source. All network-selection behavior beyond reading `wifi_connected()` is owned by the firmware selector invoked through `T5SystemUiApi`.

## Storage and persistence

The app performs no direct storage operations and defines no persistent file format. Wi-Fi credential persistence and network configuration storage, if any, are not established by this source and remain firmware/provider responsibilities.

## Failure handling and limits

Confirmed behavior:
- missing any required API pointer or required function causes an immediate return;
- failure to obtain a pending selector result, or a cookie mismatch, is treated as a fresh handoff case: the app requests the selector and returns;
- the UI contains exactly two rows;
- return values from `wifi_request` are intentionally ignored by the current source;
- no retry loop, timeout, or error-specific status text is implemented here.

## Source and release identity

Synchronized from Reader commit `be82695ea0ecb14525c0de1ddc78cd0c77e4614b`:
- `Apps/wifi_settings.c`: `33fd4a4045daecce210e1e347bedd599e146245d`
- `Apps/wifi_settings.json`: `d4ea5dbb60de7fa178efd951dd07ae4f3572ccd3`

The audited upstream release-index snapshot lists [Reader release `app-wifi_settings-v1.0.2`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-wifi_settings-v1.0.2), `wifi_settings.elf`, **3,084 bytes**, SHA-256 `e50adecc5308600b5637db6806aa933d24e70686ad86c77e34b755afd9998f32`. These are upstream published metadata; destination development builds are not independent releases and do not establish runtime parity.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.3**; the application C source is synchronized without source edits. Source blob `33fd4a4045daecce210e1e347bedd599e146245d`; manifest blob `4bce101a4b1db443765d1e2aa82c2e8be8064fbd`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-wifi_settings-v1.0.3`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-wifi_settings-v1.0.3/application-wifi_settings-1.0.3-xtensa-esp32s3.rte.zip) has a **4164**-byte package with SHA-256 `d382883cf73e7d1969d38b6470001e36f81677f629216e21d190d8826f7ec8d6`. Its embedded `wifi_settings.elf` is **3084** bytes with SHA-256 `e50adecc5308600b5637db6806aa933d24e70686ad86c77e34b755afd9998f32`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
