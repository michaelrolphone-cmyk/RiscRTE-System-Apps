# USB reconstruction working checkpoint

Partial working snapshot after the execution workspace was replaced.
The five USB production/build/test files are reconstructed on the exact .53
System source archive (original source commit09922405, verified tree
c55acc64933210cb9c8af74725c4df1f23834b40). The complete recovered baseline
has not yet been uploaded to this branch, so this intermediate commit is
not a coherent target-build or integration candidate. The remaining shared
source blobs will follow without modifying main or another working branch.

USB SD Transfer0.1.5 latches return only after provider-confirmed EJECTED
and successful session end plus grant release. The outer loop restores
a resident Home host by normal child return. Suspend/reset/deconfiguration
remain custody-preserving and do not prove eject.

25 production app/adapter cases passed normally and under ASan/UBSan in
the full local .53-derived tree. Target builds, real Runtime and TinyUSB
requalification remain pending after reconstruction. Physical Windows
eject detection and remaining directory latency remain unresolved.

The earlier df7a412 source and diagnostics .106/.1.5/.1.6 source were lost
locally; reconstructed commits have new identities and require new proof.
No hardware, flash, release, merge or updater action is part of this checkpoint.

Durable baseline archive: Library libfile_54bf28aad1848191b2de20baf9287582.
SHA256:4ed5116c5cefd376b52769039eb0e05dea171bfbe260746f3c3ccef4dc076558.
