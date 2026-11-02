# Shared Files 1.6.1 delivery

The application is a single shared Files controller with scene/intent presentation.
The canonical compact ELF is 250,020 bytes, SHA-256
`8d19aa1d7f5115c8a21cc293d14178d9e0b2c3edb2b42f9f85ed1f8022f16d94`.
The owning workflow rechecks that identity, the full original ELF sidecar, repeated
link/compaction and ELF admission.

This successor preserves file browsing, storage operations, text input, handler/source
handoffs, peer files, sharing/setup, Context audio and RF, automatic preset/rule
effects, background telemetry and checked lifecycle cleanup. Optional capabilities
remain provider-owned; absent features are reported rather than simulated.

## Published components
- [Files background](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/117)
- [Home settings effects](https://github.com/michaelrolphone-cmyk/RiscRTE-Watch/pull/21)
- System settings-effects header: `lib/PortableApps/include/PortableResidentSettingsEffects.h`

The Home build uses retained System `bb0cadf3` plus that exact header overlay,
not an unchanged retained checkout. The existing source-pin verifier records this
distinction and checks every consumed source and compiler digest.

## Qualification
`.github/workflows/files-scene.yml` runs focused, complete-root, storage, peer,
sharing, background, current/retained scene Runtime, Home effects and product
admission suites. Normal and ASan/UBSan results are separate; leak detection is
explicitly disabled. Hardware and endpoint doubles are identified in each receipt.

The product lane verifies six original input hashes before deriving Files grants
from actual decoded stores. It preserves every existing grant and unrelated byte,
adding only the shared scene declaration already specified by the original
projection recipe. All three products receive identical application ELF bytes.
Home and RTC replacements are restricted to the qualified compatible packages.

Earlier `files-*-checkpoint.json`, `files-expanded-qualification.json` and
`files-scene-cloud-status.md` are historical intermediate records, not final
qualification of this head. Current hosted receipts and logs are retained in the
workflow artifact. Final hosted delivery is pending until that workflow passes.

No physical-device test or complete native firmware rebuild is claimed.

## Loader-metadata packaging correction

The initial 391,232-byte Files 1.6.0 ELF exceeded the retained Watch store capacity. Files 1.6.1 uses the existing `loader-metadata-v3` compactor, copied byte-for-byte from System `5cdc71054f51489a2e6c654e25b1b0eb21953172:docs/shared-app-crashes/replay/compact_current_elf.py` (also used by Home). The full original ELF remains a sidecar and its known hash is checked. Allocated sections, program segments, entry point, dynamic symbols, retained static symbols and relocation targets must remain identical. Repeated compaction, deliberate corruption rejection, Runtime validation and all three product preservation/admission are required by CI. No app feature, authority, data file or partition geometry is removed to fit. Final compacted identity is recorded by the owning run; this note does not assert that the run has passed.
