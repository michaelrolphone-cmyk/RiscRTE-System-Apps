# Calculator native presentation ownership

PORTABLE_CALCULATOR_NATIVE_UI selects private owner8 and the Calculator-only
presentation/input reducer. No Runtime or T5 ABI changes. It is enabled only by
an app's explicit native begin. Existing Scanner and Touchpad owners remain
independent. A screen-map identity change invalidates gesture ownership; only
a completed matching frame can make its controls visible. First-visible provider
sequence fencing and neutral recovery reject inherited held input. Repaint of an
unchanged map retains fresh input, while stale completion cannot authorize a new
map. Snapshot captures retain their original owner and generation.

The Utilities Calculator uses native raster primitives and its own geometry.
Paper and legacy builds retain their existing selector-off path. This source
change does not select a product firmware or update released recipes.

Validation: scripts/test_calculator_native_input.py runs the production reducer
with normal and ASan/UBSan controls. scripts/test_calculator_native_optout.py
compares all preprocessed tokens against exact pre-change main across36 legacy,
Nova, native, paper, Scanner, Buttons and Touchpad compositions. Utilities runs
actual app/adapter direct and snapshot raster, arithmetic and input scenarios.
Physical performance and product adoption remain separate.
