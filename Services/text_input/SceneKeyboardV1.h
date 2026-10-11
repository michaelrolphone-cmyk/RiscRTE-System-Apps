#pragma once
/* Private System provider-to-provider ui.scene extension. This is not an
 * application text-input contract and carries no keyboard layout policy.
 * value selects the plain-text page (0..3); text is printable ASCII; label is
 * the prompt; action receives VALUE_EVENT key codes below. minimum=0,
 * maximum=3, step=1. target optionally carries the character limit (0 means unknown). DISABLED means a hardware keyboard is attached.
 * The text-input owner updates the scene after every key, even an ignored key.
 */
enum { RISC_SCENE_KEYBOARD_NODE = 7 };
enum {
    RISC_SCENE_KEY_LAYER = 128,
    RISC_SCENE_KEY_BACKSPACE = 129,
    RISC_SCENE_KEY_SPACE = 130,
    RISC_SCENE_KEY_DONE = 131,
    RISC_SCENE_KEY_CANCEL = 132,
    RISC_SCENE_KEY_CLEAR = 133
};

/* Text-only document revisions preserve the displayed topology and queued
 * keys. The host rebases each delivered key to the latest acknowledged text
 * revision. Every other change invalidates the input epoch and queued keys. */
