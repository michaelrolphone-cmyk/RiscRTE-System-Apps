# File Transfer

File Transfer is a foundational file/connectivity handoff app that opens the firmware-owned wireless file-transfer session. Upstream version **1.0.0** requires firmware **1.1.24** and builds as `file_transfer.elf`.

It obtains `T5AppApi`, `T5SystemUiApi` version 1, and `T5UiApi` version 1, then renders one highlighted action: **Start File Transfer**, subtitled **Wi-Fi, Calibre wireless, or reader hotspot**. Back/exit leaves. Confirm activates the action; a tap activates only when shared UI hit testing resolves row 0. Activation calls `file_transfer_request()` and then returns regardless of its boolean result.

The `T5SystemUiApi` contract explicitly leaves ownership of Wi-Fi, Calibre, captive portal, HTTP server, and cleanup resources with the firmware File Transfer session. This ELF does not implement those transports, contain endpoints, directly read/write transferred files, or persist state. Missing required providers/functions causes immediate return.

Audited at upstream `525e32689203502a7b22f6350b7ef04f272271db`: source `1a030a3c9531cef1d627f88b58ae5920550984df`, manifest `b3e315ece23e1e7a2ce0df46ef4e4dd71f8e0334`, and `T5SystemUiApi.h` `41d06ae089ddb7d7e4e7378123ff7c6854e642d2`. No independent destination build/release artifact is established yet.
