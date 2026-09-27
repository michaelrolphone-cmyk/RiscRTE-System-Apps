# Font Family

Font Family is a foundational appearance/settings app for choosing the reader font through firmware `T5FontApi` version 2. Upstream version **1.0.0** requires firmware **1.1.24** and builds as `font_selection.elf`.

The app reads at most 64 provider choices, renders each name, marks the provider-selected choice with `Selected`, and initializes selection to that row. Back/exit leaves unchanged. Up/Left and Down/Right use shared UI navigation. Confirm calls `select_choice`; a tap selects the hit row and calls the same operation. The app exits only when the provider returns `T5_FONT_OK`.

It uses `choice_count`, `choice_info`, and `select_choice`. `t5_font_choice_info_t` supplies a 64-byte name plus builtin/selected flags. Actual font activation and persistence are provider-owned. No app-owned storage path, file format, network endpoint, or direct hardware access appears in source. Missing required APIs/functions causes immediate return.

Audited at upstream `525e32689203502a7b22f6350b7ef04f272271db`: source `e7ef8428a17ff85b99a365098c251b3c0b99e152`, manifest `0e15372fff714f626d25b2f4dd73802bc8bd39eb`, `T5FontApi.h` `f019bf858c9ec85d316ac35c79cf12ab8139a7c2`. No independent destination build/release artifact is established yet.
