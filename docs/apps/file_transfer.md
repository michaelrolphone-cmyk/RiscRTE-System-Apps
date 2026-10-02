# File Transfer

File Transfer is a foundational file/connectivity handoff app that opens the firmware-owned wireless file-transfer session. Upstream version **1.0.1** requires firmware **1.1.24** and builds as `file_transfer.elf`.

It obtains `T5AppApi`, `T5SystemUiApi` version 1, and `T5UiApi` version 1, then renders one highlighted action: **Start File Transfer**, subtitled **Wi-Fi, Calibre wireless, or reader hotspot**. Back/exit leaves. Confirm activates the action; a tap activates only when shared UI hit testing resolves row 0. Activation calls `file_transfer_request()` and then returns regardless of its boolean result.

The `T5SystemUiApi` contract explicitly leaves ownership of Wi-Fi, Calibre, captive portal, HTTP server, and cleanup resources with the firmware File Transfer session. This ELF does not implement those transports, contain endpoints, directly read/write transferred files, or persist state. Missing required providers/functions causes immediate return.

Audited at upstream `525e32689203502a7b22f6350b7ef04f272271db`: source `1a030a3c9531cef1d627f88b58ae5920550984df`, manifest `b3e315ece23e1e7a2ce0df46ef4e4dd71f8e0334`, and `T5SystemUiApi.h` `41d06ae089ddb7d7e4e7378123ff7c6854e642d2`. No independent destination build/release artifact is established yet.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.1**; the application C source is synchronized without source edits. Source blob `1a030a3c9531cef1d627f88b58ae5920550984df`; manifest blob `c521692a6bb3d6d878d8229454b9907b3721bf00`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-file_transfer-v1.0.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-file_transfer-v1.0.1/application-file_transfer-1.0.1-xtensa-esp32s3.rte.zip) has a **3416**-byte package with SHA-256 `b0210d097c07189ff1e6392bd542fd74972e565b87c7d9a92d7082c6e175b117`. Its embedded `file_transfer.elf` is **2340** bytes with SHA-256 `7163d299b2b4115e719f52d5288925634e5613d6f38ddd984c80189bb4d5cdcc`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
