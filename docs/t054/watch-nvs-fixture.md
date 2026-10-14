# Synchronous Watch UI regression qualification

The legacy Settings peripheral fixture only changed snapshot state and returned an empty event stream. The deployed ordered-touch client correctly consumes events after its initial snapshot, so that fixture silently lost all later gestures. The fixture now emits one sequenced DOWN/MOVE/UP event for each modeled physical report, preserves current snapshot state, and drains the pending event once. Production input code is unchanged.

The native-height suite passes all 190 normal/ASan/UBSan cases at 240x240 and 410x502, both rotations, including original 240-pixel golden frames, Quick saves, modal ownership and asynchronous presentation. The synchronous Quick core/session and 108 adapter/rotation cases also pass. These cover the non-atomic profiles selected by Watch's existing NVS backend.
