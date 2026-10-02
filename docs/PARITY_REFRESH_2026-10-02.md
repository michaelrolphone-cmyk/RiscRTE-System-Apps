# Status Bar Settings parity refresh — 2026-10-02

Reader master: `ca66db298c2e735f45e5029083a9bfbd7b6740bd`
Reader release-index: `f9fb899c5e22da26280b97587fff79b42118e56a`
System-Apps base: `dfc226998553888b8a736a985ba248b938e70ddf`

Reader master later advanced to `ca66db29` when the separately owned Hollow
Trail PR #352 merged. Its diff is isolated to Hollow Trail; the Status Bar
Settings source/manifest/test and release-index f9 remain unchanged. The fresh
immutable three-way audit against ca66db298c2e735f45e5029083a9bfbd7b6740bd covered
all 38 tracked app source, manifest and helper inputs. Only `Apps/status_bar_settings.c` and `.json` were upstream-only;
the external copies equaled their recorded Reader `1e0188c1` baseline. No
external-only implementation changes or conflicts were found. The sync copied
the two exact Reader blobs. It also replaces the old held-confirm test fixture
with the current Reader regression fixture; SDK headers and compiler inputs did
not change. The post-sync `source-drift.json` reports all tracked inputs clean.

Reader's published 1.0.2 release supersedes external 1.0.0. The new source
ignores held Confirm repeats until a sampled release; another press activates
the selected item. The existing release identity is 2,920 bytes, SHA-256
`ef294e50c5007b301245b0a5aaf3e2abcfb19328a91ce24ab718af7325ad4e0b`. No further
version is invented.

Local evidence: ten pipeline tests passed; the affected host fixture passed
with `-Wall -Wextra -Werror`; the selective pinned Xtensa build passed import,
integrity and ELF checks and compared byte-for-byte equal to the downloaded
published asset. The incremental full-cohort release report was derived by
reusing unchanged existing build outputs and substituting only the newly built
status-bar ELF/evidence; it records 9/18 exact matches, with all nine required
entries passing. The complete exact-head CI run must still pass before merge.

This establishes current-master source and released ELF parity for this app. It
does not establish U1 ZIP/runtime compatibility, package installation/update,
rollback, release automation, live catalog readiness or cutover. No prospective
OTA/Clock/Hollow source from unmerged Reader branches is included.
