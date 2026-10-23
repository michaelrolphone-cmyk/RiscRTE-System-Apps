# T061 action-declaration guard and enabled qualification

This is a minimal stacked repair over PR153 head
`2566fb0186301b370fa490b0fb285acc85b6cb35`. The original draft and its
historical evidence remain unchanged. It is not firmware or product delivery.

## Reproduced CI failure

- [Native App Store failing job](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/39621346682/job/119304866011)
- [Native Firmware Update failing job](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/39621346700/job/119305914547)
- [Unchanged-base passing job](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/39356056775/job/118411829989)

Both failing jobs checked out the exact PR head, not a synthetic merge. They
failed the unchanged full retained recipe selector-off comparison on app_store,
before attempting its ota_update comparison. Their 12 smaller selector-off
profiles passed. The base passed both full recipes with the same pinned external
sources and compiler SHA-256. This is not unrelated repository parity drift.

The original failure artifacts contain a 233,224-byte baseline app_store ELF
(`bd5a1761cd2faec67f691a9ac52db392c2d09b1ad491b42ef8e4c48cfb7555d1`)
and same-size candidate
(`e187a766dbcf7f45e0c6814b7956f0a2c20e65f41dcdf3bc303f4b322c7dafe7`).
All 5,588 differing bytes are in the non-allocated `.symtab` and `.strtab`.
Every allocated section and every byte outside those tables are identical.
Thirteen local symbol names have compiler IDs shifted by seven, with otherwise
identical normalized symbol metadata. This establishes byte-only incompatibility
in this artifact, not an observed change in executable behavior.

## Minimal source repair

The seven remote-action enum constants were unconditionally declared even when
remote battery was disabled. Guarding only those seven declarations removes
their compiler-identity effect from ordinary builds. Their enabled values remain
bits 14 through 20; all ordinary values remain unchanged. All in-tree production
users and remote test consumers are already feature-gated. Independent no-write
preprocessing showed enabled output unchanged and disabled output matching the
original feature-disabled header. External clients using these opt-in names must
enable the feature; no such disabled in-tree consumer exists.

No baseline, expected ELF hash, byte comparison, compiler option, product pin,
version, radio policy, refresh behavior, selection behavior or firmware changed.
The original full-recipe gates remain required, including their symbol tables.
Target byte preservation must be established by the existing CI on the new head;
local host tests alone cannot establish it.

## Enabled coverage

The 53 original workflow files were verified against their exact-head Git blob
IDs. None invokes the remote-battery runners or selects the remote feature. Green
ordinary Quick/Clock and target-build jobs therefore do not qualify this feature.
The additive `Resident Quick remote battery qualification` lane checks out the
PR head and the exact previously qualified source cohort:

- Runtime `13eca114cb58469cf6004fafc3da84fbd9d29beb`.
- Drivers `1c9663658d61bc51557576aef40f82c26f36232c`, identical source tree
  `e9efca843b41733d90bfa4e8c48fe1135216d470` to local tested
  `ba28f9bdfbe84d23de36350189cc2ec89fea1677`.
- Utilities `6b2047500905f8f45087cb49989103a926fc5c63`.
- X4 source contracts `6b7d1abe9b2dcc236f6086d07c0f619244c907cb`.

It runs action-value/disabled-name checks, 20 build-selection checks, normal and
ASan/UBSan session/controller tests, the real adapter with ordinary/atomic queue
paths and seven compared rendered states, actual patched producer/parser/consumer
round trips, and real Runtime optional admission with synthetic providers.
The action-contract test also requires all seven names to be absent when disabled.
The lane has read-only permissions and no push, dispatch, release or radio step.

`local-evidence.tar.gz` preserves local repaired-source logs, source hashes,
adapter and roundtrip receipts, and the original CI failure/base receipts plus
ELF section/symbol analysis. The inventory records all included file hashes.
Compiler-produced executables and original CI ZIPs are excluded; their hashes
and original artifact/job identities are retained. Host qualification is not
target or device qualification. LeakSanitizer is disabled; ASan/UBSan are enabled.

## Still open

The remote-battery enabled Xtensa resident-host ELF, matching rebuilt provider,
imports/memory/custody, exact product/version selection and explicit optional
sensor owner grant remain unqualified. So do physical Watch-to-X4 reception,
last-reading/current indication, explicit refresh and close/restoration behavior.
No automatic peer selection, persistent identity, background scan, changed
pairing policy, merge, release, catalog update or flash is authorized here.
