#pragma once
/* Private System provider-to-provider ui.scene extension. This is not an
 * application text-input contract and carries no keyboard layout policy.
 * value selects the plain-text page (0..3); text is printable ASCII; label is
 * the prompt; action receives VALUE_EVENT key codes below. minimum=0,
 * maximum=3, step=1. target optionally carries the character limit (0 means unknown). DISABLED means a hardware keyboard is attached.
 * On keyboard nodes only, the otherwise-unused PRIMARY/DESTRUCTIVE bits
 * describe a password field and an explicit user-requested reveal. Existing
 * presenters ignore those bits and retain the masked default; no ABI grows.
 * The text-input owner updates the scene after every key, even an ignored key.
 */
enum { RISC_SCENE_KEYBOARD_NODE = 7 };
enum {
    RISC_SCENE_KEY_LAYER = 128,
    RISC_SCENE_KEY_BACKSPACE = 129,
    RISC_SCENE_KEY_SPACE = 130,
    RISC_SCENE_KEY_DONE = 131,
    RISC_SCENE_KEY_CANCEL = 132,
    RISC_SCENE_KEY_CLEAR = 133,
    RISC_SCENE_KEY_VISIBILITY = 134
};

enum {
    RISC_SCENE_KEYBOARD_PASSWORD = 2u,
    RISC_SCENE_KEYBOARD_REVEALED = 4u
};

/* Text-only document revisions preserve the displayed topology and queued
 * keys. The host rebases each delivered key to the latest acknowledged text
 * revision. Every other change invalidates the input epoch and queued keys. */

/* Opt-in bounded long draft viewport. text contains the last 71 characters;
 * target packs the complete length in the upper 16 bits and limit below it.
 * Older presenters reject this flag before acquiring a session. This changes
 * neither the scene document ABI nor ordinary keyboard semantics. */
#define RISC_SCENE_KEYBOARD_EXTENDED 16u
#define RISC_SCENE_KEYBOARD_EXTENDED_LIMIT 127u
/* The same bounded scene viewport can carry the longer checked text-input
 * suffix. Older presenters reject targets above their supported limit. */
#define RISC_SCENE_KEYBOARD_LONG_LIMIT 255u
