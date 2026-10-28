# Badge source snapshot CI follow-up

Repairs are against snapshot d0aaeb79f150a57e2ed91a46d746cb0cb23770b6. No production adapter, input host, glyph or firmware bytes change.

The text checkpoint fixture previously treated a full bounded input batch as provider loss. Production preserves valid events across batches. The 34-key case now checks both batch boundaries, ordered consumption and the requested text capacity; the overflow case separately injects the provider's negative next-event result. The complete checkpoint host/client run passed 140 normal and ASan/UBSan scenarios locally. Local leak detection was disabled because the execution host uses ptrace; CI retains its default leak checks.

Telemetry's qualified and generated files have distinct provenance comments. Verification now generates into a temporary directory, requires both exact full-file hashes, compares every glyph declaration byte and all metadata, and verifies the shipped file is unchanged. Pillow 12.3.0 / FreeType 2.14.3 passed. Five existing workflows use that same check.

The Watch color profile source-contract check now recognizes the existing opt-in native-height branch while preserving legacy 240-square and RGB565 requirements (four checks passed). Brightness testing reads the tagged alarm descriptor directly on the current color path; the explicitly selected historical baseline still exercises its original helper. Both original sources are unchanged. Current first-frame brightness: 126 cases passed across normal and ASan/UBSan, paper, RGB240/RGB410, sync/async and no-Quick profiles. Historical ffe0295eff869f0d33d56f98548f9b435975d584 reproduced its dark first frame in eight cases.

All workflow YAML parsed and git diff whitespace checks passed. These are host/software results, not physical qualification. The larger snapshot PR still requires reconciliation with main before readiness; this repair does not authorize deletions, merging or firmware publication.
