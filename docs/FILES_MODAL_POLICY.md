# Files shared-editor lifecycle checkpoints

This opt-in correction preserves the selected Watch and X4 Files 1.5.34
Find/Rename/New-folder behavior while servicing lifecycle policy inside a shared
plaintext editor. Selected corrected components are version 1.5.35. The source
builder exposes `--shared-text-policy`, requiring the existing shared-text and
WebDAV-sharing selections. Disabled builds retain their previous behavior.

## Ownership boundaries

The shared text host remains the sole keyboard/touch/navigation consumer.
`fb_text_policy_boundary` is true only synchronously during a settled policy
prepare or a successfully paused policy apply. Ordinary `portable_file_browser_safe`
continues to reject an active editor, retained text/native custody, retained files
and every owned file handle. No general browser operation becomes editor-safe.

During paused apply only, `portable_file_browser_close` preserves the paused text
host and draft while performing its original checked sharing, file-source,
handler, volume and owned-handle cleanup. Normal exit/Back/Home/fini still closes
the host. A later acceptance reacquires storage through the existing controller.
Remote expiry, disconnect or mount-route changes still cancel the old draft
before it can mutate the replacement volume. Pending close and retained failures
follow the existing non-retrying custody rules.

The bridge's Files-only guards retain active-sharing idle inhibition, native
network-owned resident POLL exclusion and checked service progression. Stopped
X4 policy releases storage before resident dispatch. Watch low-battery sampling
continues while sharing is active; a low crossing keeps its existing deferred
sample through checked sharing shutdown. Eligible Contexts/broadcast service
steps continue only when the existing sharing gates permit them. This does not
claim sustained advertising where the predecessor stopped it after app polling.

Routine no-sleep checkpoints preserve passive input subscriptions. Any possible
sleep or Contexts message detaches input before dispatch. Pending pause keeps its
chosen mode; paused Runtime exit closes cleanly. Non-OK resume retains the session
instead of dispatching policy twice. Missing checkpoint support closes cleanly and
shows the existing retry message. No universal lossless physical sampled-tap or
hardware validation claim is made.

## Reproduction

Run `python3 scripts/test_file_browser_policy.py --utilities ../utilities
--output ../files-policy-evidence --legacy`, with the Runtime SDK checkout at
`../runtime`. The CI workflow pins Runtime 64c8e52e and Utilities 4cfe2124.
The suite executes actual Files, portable adapter, plaintext host and real peer /
WebDAV / sharing backend code. Scene checkpoint, native input/display/battery,
resident dispatcher and bounded background-service endpoints are fixtures.

The 44 normal/ASan/UBSan policy workflows cover stopped idle, active borrowed and
native-owned sharing, pending frames, owned file handles, paused Home, retained
resume, unavailable/missing checkpoint suffix, Contexts message dismissal,
low-battery shutdown and remote expiry. The optional legacy matrix retains 144
normal/sanitized controller workflows across RGB, paper and paper-scroll,
including actual app_main entry, accept/cancel/Home, 127-byte names, non-ASCII
names, Find, Rename, New folder, remote copy/move/delete and retained cleanup.
LeakSanitizer is disabled in this executor because of ptrace restrictions.

Device recipe evidence separately proves exact selected flags, authority and font
closure, Xtensa target/stack inspection, complete normal/sanitized cohort
admission and lossless metadata-only packing. Watch flag-off replay is raw-byte
identical. X4 flag-off replay has only compiler local numeric symbol suffix
changes in non-allocated .strtab; all other sections match, and independently
metadata-compacted ELFs are byte-identical. This distinction is recorded rather
than claiming raw equality. Watch's two-free-block floor is preserved.

The previously documented resident-plus-Contexts-workflow-message limitation is
not enabled by these selected profiles: Watch has local policy; X4 Files has no
Contexts client. A future combined profile requires a separate compatibility fix.
