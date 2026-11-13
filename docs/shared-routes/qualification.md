# Shared content-addressed route repair

Source patch base: RiscRTE-System-Apps `83076470d832403556e987fcc4dfc148b112ea89`.
The modified assembler and plan-generator blobs are identical to the initial
qualification base `77c427a75e5651f03119c059fcdfd36d23df8708`.
Actual product baseline: published Firmware Update `83076470d832403556e987fcc4dfc148b112ea89`.

The general Home overlay now binds replacement application/provider destinations
to actual ELF hashes, requires explicit destination review, preserves fallback
identity, rejects collisions/shared-file overwrites, and compares authority with
JSON type fidelity. Stable paths remain stable. Retargeting an application needs
its existing true internal fallback. Existing content-addressed objects are never
overwritten or deleted.

`repair_shared_external_routes.py` separately repairs existing cumulative products
without changing any executable, manifest, grant, namespace, firmware or store
geometry. Correctly addressed optional SD images that are absent remain absent.
The plan is bound to the complete original internal and SD inventories. All lane
plans were validated before writing the final candidates.

## Verified results

- 58 Python regression tests pass on Linux and the corresponding updated Mac
  source is retained. Tests cover explicit destinations, stale inputs, alias and
  content collisions, fallback identity, JSON types, immutable inputs, exact
  Runtime path/identity bounds, no-op/multi-instance repair and CLI failure.
- All downloaded baseline asset sizes and SHA256s matched GitHub release
  metadata. X4 contained exactly three bad provider digest names: power,
  scene-host and civil-clock. Their preferred SD bytes matched internal fallbacks.
- X4 changes only three `boot.json` paths and adds their three correctly named
  SD objects. Every original SD object and all other internal bytes are retained.
- X4 remains 5,308,416 bytes. The pinned SPIFFS writer and independent reader
  roundtrip both products. Repeated store and SD ZIP builds are byte-identical.
- Repaired X4 store SHA256:
  `606819c54c5fe88dc13ae9ee3f391af6cde3e6f103f25c92446d86870bbaa430`.
- Repaired X4 SD ZIP SHA256:
  `ec692938d730485626285c9530cf8bf98235598f7ab9d73978fa6992564d0bfa`.
- Watch/Waveshare store and SD ZIP bytes remain identical to their baseline.
- Actual unchanged Runtime `0052d1fc42e9cfc82d626c5a247d460919c87935`
  preserving policy passes 22 normal/UBSan positive/negative executions using
  real decoded X4 inventories. The old filename mismatch is reproduced and
  repaired routes pass. Mutated authority, manifests, hashes and names reject.
- Actual X4 Runtime graph preparation and structural validation pass normal and
  UBSan: 50 ELFs, retained SD graph bound, no hardware/storage calls.

The policy harness must be built with the owning X4 definitions, especially
`RISC_PAIRED_APP_DATA=1`, providers/platform capacities 40, and app policy and
requirement capacities 32. Initial compilation omitted the app-data flag and
therefore selected ABI1; that test configuration failure was diagnosed and
corrected. The qualifying source is unchanged production Runtime. Clang's
duplicate forward-declaration warning is retained but not promoted to an error
with `-Wno-error=redeclared-class-member` for the existing admission harness;
no Runtime source was changed.

## Reproduction and limits

Run the four source regression suites individually:

```sh
python3 scripts/test_shared_app_cohort.py
python3 scripts/test_shared_home_cohort.py
python3 scripts/test_external_image_routes.py
python3 scripts/test_route_only_repair.py
```

The dedicated CI workflow checks these suites at the exact PR head.
For a new unpacked product, generate a reviewed plan with the repair script's
`plan` command, then apply that exact plan to a fresh output directory. Use the
owning pinned bootfs packer at the original size. The retained Mac source/proof
bundle contains full pinned Runtime/System archives, baseline assets, repaired
assets, plans, compile commands, logs, and policy/native admission receipts.

This is software qualification only. No device was flashed, bank activated,
or storage erased. This source change does not publish or activate firmware. Physical readback/activation
and full all-application convergence are not established by this repair. Other
applications' source, product integration and publication blockers remain separate.
