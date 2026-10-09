# Native alarm guard compatibility source

Public code `ccd57a97d27316a15deb2ddd601f565c24bcdf96` has tree
`3e2300d416c0ca54b4f22ea1dd03bec126c980c8` and parent
`16058588cd8e64021b8c89c4fbad3f2ecc736a5a`. It is an independently qualified
12-file guard-only port, not a replacement for a later X4 adapter.

The original tested X4 source remains
`b1d539df71c7e2c32bf3be6c4ed4228d13f45e0b`, tree
`ed5afdc7fc1e069d8d1bc85e0884ad2174451661`. It is referenced for comparison only;
its unpublished ancestry and source bundle are not uploaded here. Existing
artifact recipes that require that original identity must keep it.

`input-comparison.json` records SHA-256 for every changed input. Both guarded
client headers, both test fixtures and both runners are byte-identical to the
original implementation. The five adapter files are narrowly adapted to the
already-public base, and the documentation explains the difference. In
particular, this base's ordinary retention helper is already quiet, so the
silent helper delegates to it. No whole-tree equivalence is claimed.

Run `python scripts/verify_x4_alarm_source.py` from a full public clone to verify
the exact public parent/tree, 12-file scope and every published input hash. It
does not require or import the original source history.

Host and target tests use public Runtime
`3fcbba6a04626f8484a523e7da59fb44c82e6e49` and Utilities
`25c386d8ca9cb41ad47b141c57c4fb8f904e547d`:

```sh
python scripts/test_portable_alarm_terminal.py --runtime RUNTIME --utilities UTILITIES
python scripts/test_alarm_terminal_target.py --baseline PUBLIC_16058588 --runtime RUNTIME --utilities UTILITIES --xtensa-cc CC
```

All 40 actual API1/API2 adapter cases plus descriptor/client matrices pass in
plain and ASan/UBSan builds. Four API1/API2 plain/Quick Xtensa flag-off objects
are byte-identical to the public base; four selected profiles compile. The
70 existing repository unit tests pass. Dedicated CI repeats those focused
checks. Select `PORTABLE_ALARM_TERMINAL_RETENTION` together with the existing
native custody flag. No product version or hardware qualification is assigned.
