# Watch19 System source custody

`System.bundle` preserves the exact reconstructed commit required by the Watch19
artifact recipe. It is the unchanged 25,741-byte bundle from the delivered
source-and-proof archive, with SHA-256
`5d59a53b22e5f6f92f46d6c58e3d3138ee910d8e8cd9d36b22bbc7d456075f4f`.

| Identity | Commit or tree |
| --- | --- |
| Public prerequisite | `851f389d72672f1ff33825940306b1a4e9931062` |
| Exact reconstruction commit in bundle | `1b1c2009935a5489c897b12c12bd4a37e684779f` |
| Source tree | `8a3337a96c6b78b1e39421424a61ea4bdda82091` |
| Public commit with that exact source tree | `dbbd479eb329bb54a0cb21d1e4d16bd930e4dc4d` |
| Public commit parent (PR81 source routes) | `c12ee9cfac111cde2160132faa110c67e57f2fc7` |

The original commit `3c9e6dbe54d4d744ef31511e7994890c900b808b` was not recovered.
Its verified tree was reconstructed as `1b1c200`; this bundle preserves that
exact reconstruction commit and its ancestry. Use `1b1c200`, not the equivalent
public commit or the later custody commit, when the artifact recipe requires
that identity. The custody commit adds only documentation, the bundle, and its
verification; it does not alter production code or delivered artifacts.

## Verify from public GitHub alone

Clone the published branch with full history, then run the verifier:

```sh
git clone --single-branch --branch checkpoint/watch19-system-integration https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps.git System-custody
cd System-custody
python3 scripts/verify_watch19_system_custody.py
```

The verifier checks the fixed manifest, byte count, SHA-256 and exact bundle
header. It creates a new temporary bare repository, fetches only the public
prerequisite and its ancestors, proves the reconstruction commit is initially
absent, imports the bundle, and checks the exact commit, tree, prerequisite
ancestry and strict Git object integrity. It also checks that the public code
commit has the recorded tree and parent. It does not change the working checkout
or fetch from the network itself. Use a full, unfiltered clone: a shallow checkout
must first fetch full history, and a partial clone must materialize all objects
in the prerequisite's ancestry. Alternatively, pass `--repository /path/to/full/clone`.

## Materialize the exact source for the artifact recipe

After verification, these commands import the preserved ref into the clone and
create a separate source worktree:

```sh
git fetch --no-tags ./docs/source-checkpoints/watch19-system/System.bundle HEAD:refs/heads/recovered-watch19-system
git worktree add --detach ../watch19-system-source 1b1c2009935a5489c897b12c12bd4a37e684779f
git -C ../watch19-system-source rev-parse HEAD HEAD^{tree}
```

The output must be the exact reconstruction commit and source tree in the table.
The bundle has one prerequisite and one advertised ref (`HEAD`); no private
checkout, local reflog, release upload or unpublished Git ref is needed.

CI runs the same fresh-repository import and negative tests for altered bytes,
truncation and changed prerequisite metadata. This establishes source custody,
not device qualification or a product release.
