# Shared Files 1.6.0 delivery

The application is a single shared Files controller with scene/intent presentation.
The qualified canonical ELF is 391,232 bytes, SHA-256
`8f9860dc07c65345a9bb20ac7686e4d4eca39694f00345b6f87e44e2a89692d9`.
The owning workflow rechecks that identity, repeated link and ELF admission.

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
