# Catalog failure-screen diagnostics

Waveshare 0.1.39 hardware feedback: opening Springboard succeeds but reports Apps unavailable. This increment makes the existing failure observable on the display; it is not evidence that the physical cause is fixed.

The catalog client retains its latest nonterminal failure stage, result and index. A private app-local Springboard hook reads those fields without a Runtime/provider callback. Both the original generic grid and RGB Springboard show the details below the existing unavailable message. Identical failed retries avoid extra redraw; changed failure details redraw, and successful publication clears them. Interface-rejection details survive grant cleanup. Terminal custody fences suppress the getter as well as further Runtime operations.

No firmware ABI or public provider contract changes. Normal shared Springboard pixels remain byte-identical. Display ownership, one-second recovery, stale-launch fencing and every existing feature remain in place.

Qualification: 184 normal/ASan/UBSan catalog cases at 240x240 and 410x502; full-feature Springboard normal/sanitized geometry and lifecycle matrix; exact .39 target reproduction; current default and atomic target reproduction. An independent integration fixture links this actual PortableAppCatalog client against native Runtime c258c4ba's real app.catalog implementation with native-settings lifecycle enabled, normal and ASan/UBSan. These checks pass without identifying the physical first-failure trigger. No hardware qualification is claimed.
