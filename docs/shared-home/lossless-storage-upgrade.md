# Shared Home lossless storage and upgrade contract

The public Home ELF is the complete canonical application, identical on all three products. A stored RTEIMG1 envelope is a lossless representation, not a second application, reduced feature set, or alternative executable. The loader recovers the complete canonical ELF before the existing ELF admission/loader path. Canonical and physical hashes are separate.

## Eligibility and preservation

Only explicitly installed internal ordinary applications may be encoded. Any driver alias, image_source/mirror alias, or catalog-authority alias excludes that path. Provider images, external paths, manifests, authority rows, native policy, app data, and stored settings are not transformed. The owning fixed-size SPIFFS packer chooses the shortest descending-positive-savings prefix that actually fits. It retains the original partition geometry and minimum free-block rule; failure never permits resizing or dropping files.

Each 32-byte little-endian header contains magic RTEIMG1 NUL, header size, codec1 raw LZ4 block, canonical length, payload length, canonical CRC32, reserved zero. Exact physical length and caller bounds are checked before one canonical allocation. CRC32 detects corruption; it does not replace the existing artifact/cohort identity checks. Runtime uses bounded input scratch, checkpoints and exact ownership/close semantics. Uncertain custody is terminal, not a retry/fallback.

## Required Watch upgrade order

Do not send an encoded store directly to an older Runtime.

1. Use the existing paired **firmware-only** update operation to install the qualified decoder Runtime0.4.20 firmware. This operation clones the **currently installed raw store byte-for-byte** into the paired target, with matching project and ABI3 and a strictly newer compiled Runtime version. Do not substitute the final encoded store in this step.
2. Reboot into that firmware and complete the existing health/boot confirmation. While the first update remains pending, a second cohort update must be refused.
3. Only the confirmed decoder Runtime may admit and install the final qualified encoded Home cohort. Reboot and confirm through the existing paired update protocol.

No factory erase, partition change, arbitrary rewriting of the current raw cohort metadata, or app-data migration is required. Retain rollback/recovery access to the previous firmware/store pair. Do not present a final cohort package as a direct old-Runtime update.

The exact selected baseline Watch artifact13066458995 (ZIP SHA256 3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2) was built with Runtime0052d1fc42e9cfc82d626c5a247d460919c87935/0.4.18. Its cohort.json runtime_version0.1.10 is stale metadata, not the compiled Runtime version. The firmware-only NativeBankStore/PairedBank implementation is byte-identical to the a27 source used by the old-Runtime rejection controls.

Wave and X4 retain raw application storage when their original stores fit. X4 retains its qualified0052 native Runtime; storage encoding is not enabled for that lane.

## Qualification boundary

Owning tests cover actual NativeBank marker/restart behavior, unchanged raw metadata, refusal before confirmation, decoder admission after confirmation, and NVS/app-data/old-pair preservation. Separate actual paired-transaction tests cover raw cloning, opposite-bank final cohort cuts and rollback with modeled health confirmation. These are compositional hosted proofs, not a single continuous cross-version native reboot trace or a physical-device test.

Final delivery must additionally bind exact native binaries, the original-geometry packed stores and independent round trips, canonical recovered bytes, actual normal/sanitized Runtime graph/ELF admission reading the physical stored trees, and full source/license/provenance receipts. Measurement-only fit, prepare-only compilation, or a decoder-only test is not final product qualification.
