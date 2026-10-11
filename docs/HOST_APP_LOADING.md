# Shared host app-loading scene

The opt-in resident Home host presents the selected application's existing
catalog icon and name, `LOADING`, and indeterminate dots before the Runtime
loads or initializes the next foreground application. No percentage or time
estimate is invented. The static scene is rendered once and physically settled
with `LOW_LATENCY`; the retained panel image remains visible through loading
and until the app completes its first presentation.

Runtime's optional, size-versioned `loading(context, relative_path)` host callback
covers direct launches, foreground chains, resumed continuations, file-open
returns and clean legacy handoffs. The host callback does not take input focus
or reopen touch while painting. It cancels a pending host crossfade so the
loading identity is fully legible before loading starts. An uncertain display
lease or failed presentation retains the invocation and makes no later provider
calls. An ordinary load refusal returns to the existing Home recovery path;
copied load/init failure evidence uses the existing host failure screen before
Home. Terminal retention and destructive handoffs never restore Home early.

Build Home with `--resident-shell-host --resident-loading-catalog CATALOG.json`
and the canonical Runtime loading-callback SDK. Use the same catalog passed to
Springboard. The builder validates its existing bounded names and licensed icon
subset and adds the launcher identity from `Apps/springboard.json` when needed.
Unknown targets show their filename and the existing Apps glyph. The selected
development version is Home `0.3.21`; feature-off Home stays `0.3.20`.
The cohort builder also accepts `features.host_app_loading: true` in its explicit
specification. No application embeds a second loading renderer.

Run the focused presentation fixture with:

```sh
python scripts/test_resident_loading.py \
  --runtime-sdk /path/to/runtime/sdk/app \
  --display-sdk /path/to/display/sdk \
  --output-dir /path/to/loading-proof
```

This runs the actual shared resident host controller, adapter, glyphs and fonts
against deterministic providers and a synthetic foreground loading boundary.
Its normal and ASan/UBSan cases cover delayed startup and first-frame completion,
async/synchronous presentation, repeated/chained/resumed loads, long/unknown
names, refusal/BUSY, writable frames, load failure with recovery, legacy unwind,
no-pending startup, and retained/acquire/submit/status/timeout boundaries.
It emits PNG frames and exact source/SDK hashes. Runtime's separate real
Runtime/Graph suite qualifies callback admission and lifecycle ownership.

The selected target build validates the Xtensa ELF and imports/exports. These
are deterministic host and compiler checks, not physical panel, hardware launch,
or release-publication qualification. Desk clock waveform choices, Home Points
readiness, Springboard layout, and stored time settings are unchanged.
