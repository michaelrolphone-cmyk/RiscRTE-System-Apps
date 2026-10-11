# Reusable document pages

`RiscScenePageV1` extends `ui.scene@1` without changing the existing scene,
lifecycle or component prefixes. It is useful to readers, document viewers and
other apps that provide a monochrome content bitmap while using the common
GUI's title, footer, navigation controls and shared shell behavior.

The provider reports viewport geometry from the selected presentation profile.
It copies both the document description and MSB-first one-bit pixels before
returning. Caller buffers may immediately be changed or freed. Frame snapshots
are frozen independently while a new document is submitted; panel callbacks do
not retain application memory. White bits are 1 in this contract, independent
of the physical display's bit polarity.

Page revisions share the normal scene revision sequence. Navigation returns
ordinary action intents. Left/right buttons and page swipe navigation use the
submitted previous/next actions. Menus remain normal declarative components;
returning from page mode restores that same component renderer.

The provider lazily owns two viewport buffers, releasing them only after the
scene's display/input close succeeds. Existing clients do not allocate them.
No EPUB/font parsing or reader preferences are built into the scene provider.

`scripts/test_scene_components.py` verifies existing Watch and paper components
plus copied pixels, invalid input/revisions, navigation and returning to menus.
