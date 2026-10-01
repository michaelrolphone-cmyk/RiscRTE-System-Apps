# External migration readiness — 2026-10-01

Reader remains authoritative. This bounded refresh compares Reader master
`1e0188c1ff0234dd33fe054c9a6fb4fde36596df` and release-index
`572746f4fcf3fde19947a066b7e5c8028cd76d21` with System-Apps base
`b64e1c99371946b7f7ff19a0ec0587859beb584b`.

All 38 app source/manifest/helper inputs match that master after base-aware
synchronization of Button Remap, Clear Cache and OTA Update, each 1.0.0 → 1.0.1.
No external source conflict was found. Independent tooling and SDK are retained.
Ten pipeline tests, 18 host fixtures and 18 ELF/sidecar checks pass locally.
Eight required published-byte comparisons pass; ten unchanged historical build
mismatches remain visible in [release parity](release-parity.json).
See [refresh provenance](PARITY_REFRESH_2026-10-01.md) for exact identities and limits.

The companion Drivers refresh starts at `9039be6c9abb30742b7a77a8ef39d507aaf01cea`
and is limited to USB navigation 0.1.1 → 0.1.2 plus its fixture, build identity,
documentation and source/release inventory. Its exact-head CI and merge evidence
belong to that repository's PR and maintenance claim #5.

Productivity, MCU and Utilities were already synchronized and green per the
maintenance handoff. They were deliberately not re-audited or modified in this
run. The old matrix describing their earlier missing pipelines is superseded;
this document does not manufacture fresh verification for those repositories.

## Current claims and limits

System-Apps maintenance claim #5 owns this refresh through validation/merge and
records release of the claim. Final PR CI must pass before ready/merge, followed
by target-main and postmerge CI verification. A local File Browser sanitizer
regression timed out on this Mac at its existing 10-second runtime limit; the
unchanged Linux CI gate remains required and has not been weakened.

Master source parity and released-byte reproduction do not establish prospective
U1 ZIP compatibility. `parity_ready_count` remains zero: independent publication,
ZIP/catalog/runtime integration and explicit cutover authorization remain separate.
No package-format migration, release automation change, release, live catalog,
Reader removal, hardware qualification or runtime switch is part of this refresh.

The historical no-strip probe remains evidence about older published builds,
not current proof for the refreshed 1.0.1 apps. Image Viewer has no host UI fixture.
Controller replay in Drivers still depends on historical Reader build context;
this bounded update does not resolve that unrelated limitation.
