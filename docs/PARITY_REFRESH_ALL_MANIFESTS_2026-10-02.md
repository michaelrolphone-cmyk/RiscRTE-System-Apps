# Remaining Reader app manifests — 2026-10-02

## Source-aware scope

- Reader master snapshot: `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.
- System-Apps main before this batch: `74568c40520c21576628fb6adc46e7b66c92152f` (PR #14 and the three-app 1.0.2 update PR #16 were already merged).
- The remaining twelve `upstream-only` drift rows all belonged to the same accepted Reader master snapshot. For each, the app C source and every manifest field except `version` already matched the target repository. Current Reader release API and the release workflow artifact confirm every current manifest version is already published.
- Syncing them as one branch preserves external-specific build and SDK changes. No app source, API baseline, package-format code, release workflow, or external-only behavior changed.
- Reader's separate open File Browser PR #357 proposes 1.3.3 but is not on master and is not copied. This batch uses merged-master and published version 1.3.2. Reader PR #347 and its publication boundary remain untouched.

## Published identities

The packages below were extracted from Reader's release workflow artifact `rte-apps` ID `11209466823`, run `36965130240`, produced on source commit `f7f006f78bf1f83c28f3ce05728b8973e895956b`. Each ZIP's size and SHA-256 matches the corresponding GitHub release asset metadata. Each embedded ELF matches the independent pinned-toolchain System-Apps build byte-for-byte.

| App | Version | Published ZIP bytes / SHA-256 | Embedded ELF bytes / SHA-256 |
|---|---:|---|---|
| Settings | 1.0.1 | 3,675 / `4759acc4cee47963f7f46bdf323eae8eaf91a8cbc4761bb7191d7ee9e258fa1c` | 2,652 / `57353530d9d8233fce0686a8fb94e3407e3a36292b5a2a5778b1df769bf99ff2` |
| Springboard | 1.3.1 | 13,068 / `d2c16c687ec3f8f3040323feb71b2287ad38fb1c552e705c815228c0de926780` | 12,020 / `6863a4a8f08b5cc5c518a0dec212e9817fce13c5dcd318ca26bc0eeb9a69a150` |
| File Browser | 1.3.2 | 22,249 / `77c11282d1f2caab348b1b204465e5195828c23e16d4e359a3cb088441b5dcef` | 21,128 / `5eb80bc076739938f8b99d1f9292c6a7f471b2783c4ebebcfdf2fa1481abff2d` |
| Time Zone | 1.0.1 | 4,435 / `6d2ce40503961940f6afeb1fd262aac2a18ceda315d30c751b46e0ed72ae8249` | 3,404 / `c6092d550e9f50a6454ce448e390f6aef735eb7831357ae31ea98b13497e313f` |
| Wi-Fi Settings | 1.0.3 | 4,164 / `d382883cf73e7d1969d38b6470001e36f81677f629216e21d190d8826f7ec8d6` | 3,084 / `e50adecc5308600b5637db6806aa933d24e70686ad86c77e34b755afd9998f32` |
| SD Firmware Update | 1.0.2 | 5,494 / `fe630b7736149bb7466c6efc72a90d41b2cb25c1838d6862477bf448d0d1d6b0` | 4,340 / `3a5c85210a75fc9051ff3eeff9ea6d8661d1dfdafdc62125f0d50fde065ce528` |
| Language | 1.0.1 | 6,750 / `c0bd878fe672ee50aaa09dda56816612a4dd46542857c2be18d969f982bf33ee` | 5,640 / `4aaf38dd7035d4beb292334a46e32e370df26aea34e9d63399d8afd4081de574` |
| Status Bar Settings | 1.0.3 | 4,058 / `ff035cd74f8810ea30dacb364ea37df2d0863628c9db39558b2a08405e76c69b` | 2,920 / `ef294e50c5007b301245b0a5aaf3e2abcfb19328a91ce24ab718af7325ad4e0b` |
| Font Manager | 1.0.2 | 5,531 / `496d51eda44d354ab8a509a4189d4e849628e87bb116b57b8a05409fcaba4c22` | 4,464 / `cd211b29c01436a7ed7e27a5959622d29a5b2a102e683f0790eb30bcdaef2706` |
| Font Selection | 1.0.1 | 3,980 / `f71b8d4cc77bf20b78438adb951274a84f76382e1f72923b94b846b12de010e1` | 2,896 / `b007c9e7bac09be8595eb1d39f83e049c96a3765c9f73855e7b409783cdcbee1` |
| Image Viewer | 1.1.1 | 4,223 / `08a218444ffe165f612f6ed00f0128155282e7a35be636979f367ade1fb9330d` | 3,108 / `98e833a178c778ae12a4a4e142a026ad25c8642b3bed0f0657b748c52c2de8c5` |
| File Transfer | 1.0.1 | 3,416 / `b0210d097c07189ff1e6392bd542fd74972e565b87c7d9a92d7082c6e175b117` | 2,340 / `7163d299b2b4115e719f52d5288925634e5613d6f38ddd984c80189bb4d5cdcc` |

Together with the fifteen previously synchronized app manifest rows, this completes manifest parity for all 18 approved System Apps. Across 38 tracked source/manifest/helper inputs, 17 remain byte-identical to the captured base and 21 converge to the accepted Reader snapshot; no upstream-only, external-only, or conflict rows remain.

## Validation and readiness

- Twelve affected app ELFs built independently with the pinned Xtensa toolchain, passed structural validation, and matched the extracted published ELFs byte-for-byte.
- All 18 host fixtures and all 10 independent pipeline unit tests pass on this final manifest state.
- Exact-byte release parity is 18/18; the original 11 required parity checks were not weakened.
- Release URLs, archive metadata, embedded ELF identity, and source commit are pinned in `sdk/release-baseline.json` and `docs/release-parity.json`.
- `parity_ready_count` remains **0**. Byte parity does not establish external package publication, catalog integration, U1 runtime installation, device behavior, physical qualification, or provider cutover.

No Reader/U1 change, release or live-catalog action, runtime ownership change, hardware/X4 work, deployment, security change, or flash was performed.
