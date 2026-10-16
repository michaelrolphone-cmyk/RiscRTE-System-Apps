# Retained X4 Settings 1.3.29 reconstruction

The retained Settings binary is 153,132 bytes with SHA256 `1c4914ed12749cb076019dd255c40939d73a6be10f1eda069c4e5d68b48148b7`. This checkpoint rebuilds a 153,132-byte candidate with matching data sections. Exact binary identity and first-Save RTC qualification remain pending.

## Inputs

See `receipt.json` for exact source, SDK, compiler, patch and ELF identities. The base source is System `a3504d4f`; `historical-adapter.patch` restores observed older raster, foreground hydration, clock and resident policy paths in the diagnostic checkout. These changes represent historical reconstruction.

`reproduce.py` sets the retained app version to 1.3.29 with the original catalog translation-unit order. `build-one.sh` records the exact native profile flags used. The script paths describe this checkpoint workspace and must be set to the matching local source/toolchain checkouts.

The candidate passes the native ELF structural validator and import/export checks. All ELF section sizes match. Read-only data, dynamic metadata, initialized data and stripped symbol tables are byte-identical. Helper ordering and final binary hash still differ.

Watch PR116's software qualification remains separate from this unfinished X4 source reconstruction.
