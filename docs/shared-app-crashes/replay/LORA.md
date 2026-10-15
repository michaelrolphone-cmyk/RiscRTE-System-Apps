# LoRa completion of the repaired application inventory

The repaired LoRa output is shared by T-Watch internal delivery and Wave's
existing external-media policy, `Apps/lora_messages/lora_messages.elf` on
volume 12. The original manifest and boot grants remain unchanged. Wave's
`radio.lora` provider is optional; absence remains an ordinary unavailable-radio
UI state. No new capability literal, built-in import or exported symbol appears
in the repaired output relative to either official predecessor.

## Selected output

- SHA-256: `81877ba548ae0c45f4ebbbe71c53753159a4b3a28282ac9ce3c5ef63ad90c0f5`
- Size: 136960 bytes, 4252 bytes larger than either 132708-byte predecessor.
- Production adapter: System `b8086d45ffa8259f982acac5974e7d6e4a6aa88b`.
- LoRa controller/model and Utility headers: Utilities
  `40cb8ee6cebe86469044745651a47b7201016136`.
- Watch navigation/sleep bridge: shared Watch
  `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`.
- Runtime SDK: `d493ca3ede561d410a858440ee2db9073c963426`.
- Compiler: Xtensa ESP32-S3 GCC 8.4.0, esp-2021r2-patch5.
- Metadata compaction: inherited loader-metadata-v4, with exact public helper
  provenance and hash in `inputs.json`.

The original native-custody and checked-storage selectors already existed in
LoRa's qualified 0.1.15 recipe. This extension adds neither selector nor atomic
or automatic preference features. Its app catalog, navigation-retention and
native-height options reproduce the Wave 0.1.16 predecessor's allocated code.
The current production adapter supplies the optional shared-data refusal and
post-retention callback fences. The native retained-invocation ABI is supplied
by the separately qualified Runtime repair.

## Exact predecessor evidence

The public LoRa 0.1.15 receipt is in the T-Watch repository at
`6e90c62635f4d299db38ee2640dc6aa25365c6d2`,
`docs/lora-input-policy/qualified/lora_messages-build-record.json`.
Its full command, generated inputs and source mappings reproduce
`c223ae04595f8b288d7084bb3804eb30252d6b9ecc0eb0cd4095c006ebc8b14b`,
128820 bytes. Substituting Utilities 40cb leaves that result unchanged because
all selected controller/model inputs are identical.

Preserving that command and selecting System `21f640e`, the above shared Watch
bridge and Runtime SDK, plus `PORTABLE_NAVIGATION_RETENTION`,
`PORTABLE_WATCH_NATIVE_HEIGHT` and `PORTABLE_RUNTIME_APP_CATALOG`, reproduces
all Wave 0.1.16 allocated sections. Its inherited v4 compaction reproduces the
complete official external ELF byte-for-byte:
`0a0c9ed6d8c701af9af336fcbb067b6ece8fc304f4acf68722b627c04d96d785`.
The checked public release asset is `Waveshare-0.1.22-SD-card.zip` under release
`test-shared-suite-0.1.22`, SHA-256
`12ba60f0b25bd9f1b794eea98b0109f691d23e422029478e9f77f8952db38e8c`.
This reconstruction proves the selected source closure; it does not claim the
original historical builder used that exact equivalent commit.

T-Watch 1.1.35 contains LoRa 0.1.16 with SHA-256
`fa87d851c8d1e3f83ad85bf5a08b2ecd2431397c60e261b0b18b9afc5e89d6df`.
Its code differs from the Wave predecessor (.text is eight bytes larger), and
its exact historical source closure remains unresolved. T-Watch LoRa is
therefore explicitly qualified as a bounded migration to the same repaired
shared controller/adapter, preserving its capability and symbol contracts.

## Validation scope

Fresh relocated replay checks every pinned public input, complete command,
selected compactor, final byte count/digest and dynamic symbol inventory. The
ELF also passes the System structural validator. Product admission, storage
capacity and device regression checks remain separate qualification gates.
The release must deliver the external Wave ELF alongside its existing media
manifest and regenerate the card content digest; the internal 17-app Wave
product remains unchanged.
