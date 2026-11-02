# Biome Monitor idle logo

Borderless Biome emblem above the exact label `Biome Monitor`, typeset in Archivo Bold (weight 700, width 100). Archivo 700 is specified by the RiscRTE website's `public/riscrte-lockup-descriptor.svg`.

The emblem comes from the supplied original logo, cropped inside its icon frame, thresholded to the panel's two native tones and centered at 304 pixels high. The title is 64 pixels; the complete panel is 800×480 black on white.

Font source: https://github.com/google/fonts/blob/main/ofl/archivo/Archivo%5Bwdth%2Cwght%5D.ttf (font bytes SHA-256 `0e094a7d3c7c4c25cf1310c4b30014f1dae9332220b1c2c88f4fa996f0b05053`, SIL Open Font License).

`idle-logo.png` is the canonical native preview. Regenerate packed firmware bytes with `python tools/pack.py assets/idle-logo.png main/assets/idle_logo.bin`. Exactly 48,000 bytes, black=1, MSB first, 100-byte rows.
