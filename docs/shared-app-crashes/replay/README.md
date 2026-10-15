# Pinned repaired application replay

Use `scripts/replay_shared_crash_apps.py` with Python3 plus pyelftools and the
official Xtensa ESP32-S3 GCC8.4.0 toolchain. Specify `--product wave` or
`--product twatch`, or `--product wave-media`, `--compiler /path/to/xtensa-esp32s3-elf-gcc`, and a fresh
`--output /path/to/output`. No local source layout is required.

The driver fetches immutable public source commits, verifies every qualified
transitive input, materializes generated headers/linker maps, rebuilds Files'
eleven prerequisite sharing objects, links each app with its recorded flags,
normalizes Xtensa relocation padding, and applies the selected checked ELF
compactor. Final byte count, SHA-256 and import/export inventory must match.
The output receipt binds driver, every spec/helper file and compiler hashes.
`--source-cache` is an optional map from repository IDs to local Git object
stores; it extracts the exact pinned commit, never the local working tree.
`--apps` selects a diagnostic subset; an unrestricted product run requires all
17 internal Wave recipes, 18 T-Watch recipes, or the single external-media
LoRa recipe, respectively. Qualification covers source reproducibility, target compilation and structural
ELF validation.

All repaired application adapters are pinned to b8086d45ffa8259f982acac5974e7d6e4a6aa88b,
which contains the three-file crash fix over the deployed Wave source. Files' prerequisite sharing library retains its separately pinned source closure. Frozen
native Springboard uses the separate f31f896 source and must remain unchanged.
The T-Watch profiles preserve its existing feature flags rather than adding
Wave's atomic preferences. Its eight legacy profiles additionally select the
checked-storage and native-custody wrappers required by the current shared
source. That migration requires the retained-invocation Runtime ABI already
present in 4107eae; it does not add boot grants. The product's complete admission,
manifest-policy and SPIFFS reserve/readback gates remain mandatory.

Normalization and compaction helpers are copied unchanged from pinned public sources;
repository, commit, path and SHA-256 are recorded in inputs.json. v3 is selected
for Wave and T-Watch basic apps. The inherited v4 metadata-only compaction is
selected for the T-Watch updater and Spectrum/Waterfall profiles and both LoRa delivery profiles.

The original Wave SDK aliases are recreated as symlinks to their canonical
source files. This preserves header identity and include precedence, including
pragma-once behavior; copied SDK headers retain their qualified copy layout.

The `wave-media` product supplies LoRa at its existing explicit SD-card path; it
does not add LoRa to the internal Wave cohort or alter boot policy. See
[LORA.md](LORA.md) for exact predecessor proof and source migration qualification.

The source-owned `checked_xtensa_link.py` helper allows at most six attempts of
an identical application compiler command, only when the failed output consists
of the exact collect2 signal-6 abort, `compilation terminated.`, and
`double free or corruption (out)` diagnostics. Partial signatures, additional
errors and ordinary failures stop immediately. This handles the observed
intermittent pinned-toolchain failure without identifying its underlying cause.
Every attempt preserves its combined stdout/stderr and SHA-256 in a distinct
`APP-link-attempts` directory; existing attempt directories are never overwritten.
Stale or failed ELF outputs are removed before reuse. Exhaustion fails the replay.
The existing final digest, size and symbol gates remain mandatory after success.
Run the focused host checks with:

```sh
python3 docs/shared-app-crashes/replay/test_checked_xtensa_link.py
```
