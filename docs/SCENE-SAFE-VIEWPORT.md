# Shared scene safe content

The provider reuses PortableSafeViewport to honor display-advertised rectangular safe insets. Insets follow the final effective display rotation; input first maps to the full logical display, then rejects/subtracts the safe viewport. Raster coordinates add the same origin. Reserved borders are cleared in existing bounded raster bands, not a blocking whole-frame pass. Zero-inset profiles retain the existing pixel path and batch count.

Tests cover full and actual color-dynamic profiles, normal and ASan/UBSan, asymmetric insets/all rotations,466 square with69-pixel declared round-panel safe insets, malformed sums and unusably narrow content. They check border pixels, visible ink and transformed touch admission/refusal. This uses the rectangular safe-content contract; it is not a circular-mask renderer or physical display qualification. Original source fails the border assertion. Existing zero-inset provider regression matrix remains applicable.
