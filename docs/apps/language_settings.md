# Language

Language is a foundational Settings app for selecting the firmware interface language. Upstream version 1.0.1 requires firmware 1.1.24 and builds as `language_settings.elf`.

## Confirmed workflow

The app obtains `T5LanguageApi` v1 and `T5UiApi` v1. It reads the firmware-owned language list, caps it at 64 entries, selects the provider record marked active, and renders a shared list. Back/Exit leaves without changing language. Previous/Next use the UI provider navigation helpers. Confirm calls `select(language_id)` for the current row; tapping a valid row selects it immediately. The app exits when provider selection succeeds.

`T5LanguageApi` exposes `count`, `read`, and `select`. Each `t5_language_info_t` carries an 8-bit language ID, selected flag, and a name up to 96 bytes. The provider contract states that `select` immediately applies and persists the firmware setting; this app does not access firmware I18N or settings storage directly.

The UI functions used are `render_list`, `poll_event`, `hit_test`, `next_index`, and `previous_index`. Missing required providers/functions, a zero language count, or any failed language read causes an immediate return. No network I/O, direct hardware access, storage path, or app-owned persistence format is present.

## Source identity

Audited against upstream `T5S3-Reader` commit `525e32689203502a7b22f6350b7ef04f272271db`: source `c9e81623fcd306793ce81c75d7d4f82ae46634ce`, manifest `549e821c8c7a924f81a25082ad1604791fc823a2`, `T5LanguageApi.h` `d5126e0eb05b66836491af67ebdfa2cfb20c1235`, and `T5UiApi.h` `ef09b405fc2518ee7ecf039f8b251939d80b14a6`.

No independent destination build or release artifact has been established yet.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.1**; the application C source is synchronized without source edits. Source blob `c9e81623fcd306793ce81c75d7d4f82ae46634ce`; manifest blob `2ee7a2db76826820cb5bd9882f85e21f161ef74f`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-language_settings-v1.0.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-language_settings-v1.0.1/application-language_settings-1.0.1-xtensa-esp32s3.rte.zip) has a **6750**-byte package with SHA-256 `c0bd878fe672ee50aaa09dda56816612a4dd46542857c2be18d969f982bf33ee`. Its embedded `language_settings.elf` is **5640** bytes with SHA-256 `4aaf38dd7035d4beb292334a46e32e370df26aea34e9d63399d8afd4081de574`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
