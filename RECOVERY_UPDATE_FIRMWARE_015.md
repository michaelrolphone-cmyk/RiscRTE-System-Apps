# X4 firmware updater 0.1.5 source recovery

This working source combines the restored .53 X4 updater policy with the public
source-route delta from RiscRTE-System-Apps commit
c12ee9cfac111cde2160132faa110c67e57f2fc7. The historical private X4 commit
c5cda9f9a303fffdd0dd48d779e272fed9f6d246 is unavailable; original private-commit
byte parity is not claimed.

All four new route files are exact public blobs. The service merge retains both
the existing X4 product-check helper and the public route helpers at their one
shared insertion point. Existing disabled-feed and installed-product checks
remain. The builder retains X4 flags and adds the public explicit route selector.

After all product source recovery is frozen, build from a clean checkout with:

```
NATIVE_APP_CC=/path/to/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc \
  python3 scripts/build_portable_updates.py --services-only --product x4 \
  --source-routes --output-dir /fresh/output/outside/source/tree
```

Use the pinned GCC 8.4 toolchain. This emits firmware provider 0.1.5 and App Store
provider 0.1.4; the X4 update feed remains disabled. Output must be outside this
source checkout so the build record truthfully records a clean source tree.
The composition must bind this new source commit, not the unavailable old pin.
The generated ELF still requires current native/import/store admission.

Fresh source-only qualification: 216 actual-provider route executions pass for
Watch, configured X4 and unconfigured X4 in normal and ASan/UBSan modes. Four
flag-off preprocessor comparisons match restored source after removing only
whitespace-only lines. Builder-selection checks preserve flag-off versions and
only enable routes for firmware. Another 212 existing provider regression
executions pass normal and ASan/UBSan. Exact dependency hashes and provenance are in
receipts/update-firmware-015-recovery.json. No product ELF was compiled here.

Existing target binaries are stale comparison evidence and are not build inputs.
No physical network, flash, hardware, live update or data migration was exercised.
