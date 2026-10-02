# Font Family

Font Family is a foundational appearance/settings app for choosing the reader font through firmware `T5FontApi` version 2. Upstream version **1.0.1** requires firmware **1.1.24** and builds as `font_selection.elf`.

The app reads at most 64 provider choices, renders each name, marks the provider-selected choice with `Selected`, and initializes selection to that row. Back/exit leaves unchanged. Up/Left and Down/Right use shared UI navigation. Confirm calls `select_choice`; a tap selects the hit row and calls the same operation. The app exits only when the provider returns `T5_FONT_OK`.

It uses `choice_count`, `choice_info`, and `select_choice`. `t5_font_choice_info_t` supplies a 64-byte name plus builtin/selected flags. Actual font activation and persistence are provider-owned. No app-owned storage path, file format, network endpoint, or direct hardware access appears in source. Missing required APIs/functions causes immediate return.

Audited at upstream `525e32689203502a7b22f6350b7ef04f272271db`: source `e7ef8428a17ff85b99a365098c251b3c0b99e152`, manifest `0e15372fff714f626d25b2f4dd73802bc8bd39eb`, `T5FontApi.h` `f019bf858c9ec85d316ac35c79cf12ab8139a7c2`. No independent destination build/release artifact is established yet.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.1**; the application C source is synchronized without source edits. Source blob `e7ef8428a17ff85b99a365098c251b3c0b99e152`; manifest blob `462193437c9e81b2cc251ed7d5b847d077fcdb16`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-font_selection-v1.0.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-font_selection-v1.0.1/application-font_selection-1.0.1-xtensa-esp32s3.rte.zip) has a **3980**-byte package with SHA-256 `f71b8d4cc77bf20b78438adb951274a84f76382e1f72923b94b846b12de010e1`. Its embedded `font_selection.elf` is **2896** bytes with SHA-256 `b007c9e7bac09be8595eb1d39f83e049c96a3765c9f73855e7b409783cdcbee1`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
