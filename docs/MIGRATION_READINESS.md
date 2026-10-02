# External migration readiness — 2026-10-02

Reader remains the read-only source of truth. This focused update audits System-Apps
main `dfc226998553888b8a736a985ba248b938e70ddf` against Reader master
`ca66db298c2e735f45e5029083a9bfbd7b6740bd` and release-index
`f9fb899c5e22da26280b97587fff79b42118e56a`.

## Current source and release parity

Reader master advanced to `ca66db29` when the separately owned Hollow Trail PR #352 merged during this check; its changes are confined to Hollow Trail and are excluded from this System-Apps batch. A fresh immutable comparison confirmed the Status Bar Settings sources and fixture are unchanged by that merge. The base-aware audit covered all 38 tracked source/manifest/helper inputs. Before
this update, exactly two files were upstream-only: Status Bar Settings source and
manifest. The external files exactly matched their recorded Reader `1e0188c1`
baseline; no external-only app changes or conflicts were found. After this
increment, all 38 tracked inputs match current master. The other 17 System-Apps
release rows already matched the current Reader release index. Status Bar Settings
moves from 1.0.0 to the already-published 1.0.2 identity.

| App | Source and manifest | Version | Published ELF | Focused validation | U1/cutover |
| --- | --- | --- | --- | --- | --- |
| [Status Bar Settings](apps/status_bar_settings.md) | Exact to Reader `ca66db29` | 1.0.0 → 1.0.2 | Exact, 2,920 bytes, SHA-256 `ef294e50c5007b301245b0a5aaf3e2abcfb19328a91ce24ab718af7325ad4e0b` | Held-confirm edge fixture; independent pinned Xtensa build | Not qualified |
| Other 17 tracked apps | Unchanged; audit clean at `ca66db29` | Existing identities | Existing release records retained | Existing evidence retained; not rebuilt locally for this increment | Not qualified |

The synchronized cohort now has nine required published-byte matches out of 18.
The remaining nine historical build mismatches remain visible in [release parity](release-parity.json)
and [historical ELF lineage](HISTORICAL_ELF_LINEAGE.md). Their historical
classification is not changed by this app update. See [refresh provenance](PARITY_REFRESH_2026-10-02.md)
for the exact base-aware blobs, test result and CI/merge checkpoints.

## Other external repositories

The fresh repo-state check found no open work in MCU-Dev-Tools, Utilities,
Productivity or System-Apps before this claim. Drivers has the separate X4 Pro
PR #12; it is owned by Grok and was left untouched. Reader changes since the
previous checkpoint contain no source files for those other target repos. Their
latest verified external main heads remain recorded in the maintenance handoff;
this focused increment does not claim new per-repo builds or U1 readiness for them.

## Current claims and limits

System-Apps claim #11 owns this single-app refresh through postmerge CI. Its exact
head workflow must pass before merge; merge is followed by target-main CI. The
remaining nine historical artifact mismatches and local File Browser sanitizer
timeout are preserved from the prior checkpoint; neither is hidden or weakened.
See [2026-10-01 readiness evidence](PARITY_REFRESH_2026-10-01.md) for that
previously completed batch.

`parity_ready_count` remains zero. Current-master source and published-ELF parity
do not establish prospective U1 ZIP compatibility, independent publication,
runtime install/update/rollback behavior or external-provider cutover. No package
format migration, release automation change, release, live catalog, Reader
removal, hardware qualification or runtime ownership switch is part of this work.
