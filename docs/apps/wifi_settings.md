# Wi-Fi Networks

## Purpose and classification

Wi-Fi Networks is the RiscRTE front end for the firmware-owned wireless network selection workflow. Its manifest identifies it as `wifi_settings.elf`, version **1.0.1**, minimum firmware **1.1.24**, display name **Wi-Fi Networks**, icon `solid:f1eb`, categories `Connectivity` and `Settings`.

The implementation is classified as a foundational System App because it exposes core network configuration used by normal system/app workflows. It does not implement Wi-Fi scanning, credential entry, association, or persistence itself; those operations are delegated to firmware through the System UI and Network APIs.

## User-visible workflow

On entry the app obtains the App, Network, System UI, and UI APIs. If any required interface/function is unavailable, it returns without rendering a workflow.

The app reads the current connection state through `network->wifi_connected()` and then checks whether a previous firmware Wi-Fi-selection request has a result through `system_ui->wifi_take_result()`.

If no result is waiting, the app requests the firmware Wi-Fi selector with `wifi_request(WIFI_COOKIE)` and returns. The fixed cookie is `0x5749464900000001ULL`.

When a result is available, the app renders a two-row list:
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

`wifi_request` transfers control of network selection to firmware. `wifi_take_result` supplies three outputs used by the app: connected state, cancellation state, and a returned cookie. The current source does not branch on the returned cookie after retrieval.

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
- failure to obtain a pending selector result is treated as the initial/continuation handoff case: the app requests the selector and returns;
- the UI contains exactly two rows;
- return values from `wifi_request` are intentionally ignored by the current source;
- no retry loop, timeout, or error-specific status text is implemented here.

## Source files

- `Apps/wifi_settings.c`
- `Apps/wifi_settings.json`

Behavior outside these source and interface boundaries is not established by this document.
