# Selected Watch alarm source

This 11,274-byte bounded bundle preserves exact tested commit
`e2b2b8b7d903acb6aec6b07ebee7d037de2babb1`, tree
`4d69b22ddfc2c8582e14808bb0ec88e6ef1fea6e`, over recovered Watch base
`1b1c2009935a5489c897b12c12bd4a37e684779f`. Its SHA-256 is
`df39f03d134178b57baf4cafbb8f471cd1025f4d7e7c3ab852f33f4f78290bcc`.
The prerequisite is preserved by the adjacent Watch19 bundle.

Public code commit `93fa422b82c174c2c56ad5875f63a6bcbee97b63` applies the exact
12-file tested delta on `fbdfad1f8db4e332528387713b85cbcc15232be1` (PR84).
Its production source is identical to the tested source; the only tree
difference is PR84's six existing custody files. Subsequent custody files do
not change production code. No newer native X4 adapter is replaced.

From a full public clone of branch `publish/watch-alarm-terminal-retention`:

```sh
python scripts/verify_watch_alarm_source.py
git fetch --no-tags docs/source-checkpoints/watch19-system/System.bundle HEAD:refs/heads/recovered-watch-base
git fetch --no-tags docs/source-checkpoints/watch-alarm-terminal/System.bundle HEAD:refs/heads/recovered-watch-alarm
git worktree add --detach ../watch-alarm-exact e2b2b8b7d903acb6aec6b07ebee7d037de2babb1
python scripts/test_portable_alarm_retention.py
```

The verifier checks fixed bytes/hash/header, imports both bundles into a fresh
prerequisite-only repository, checks exact source/parent/tree, proves public
production equality and runs strict Git integrity checks. CI repeats that
verification, four mutation checks, the 68 actual client/adapter cases under
plain/ASan/UBSan, four macro-off Xtensa byte comparisons and four selected
target compilations. Target dependency versions are pinned in the workflow.

Select `PORTABLE_ALARM_TERMINAL_RETENTION` for these guards. Source publication
does not assign a product version or establish device qualification. X4's
separate native implementation has its own focused source publication.
