# Shared semantic keypad

RiscSceneKeypadV1 adds the RISC_COMPONENT_KEY semantic node kind without changing existing component table sizes or ABI versions. Adjacent visible key runs are laid out by the presentation provider using display capability geometry and safe insets. Nodes carry labels and actions, never application coordinates or product identity. Existing providers reject unsupported documents; applications must disclose that capability gap rather than silently substitute application-owned device UI.

The provider keeps keypad navigation in its header, chooses native font sizing, and uses the existing touch/navigation, presentation, dirty-region and close-custody machinery. Six capability fixtures exercise all19 actions on240square,466square,480x800paper,320x240rectangle,466round-safe and asymmetric-safe displays. Each also verifies a generic route back action does not overlap the final key row. Normal and ASan/UBSan tests retain raster snapshots. These are real provider tests with fake hardware; they do not constitute physical hardware qualification or execution of a target ELF.

This integration preserves the qualified canvas provider branch and paper contract test branch, plus the safe-viewport fix. Calculator arithmetic/state lives exclusively in its shared application/core, not here.
