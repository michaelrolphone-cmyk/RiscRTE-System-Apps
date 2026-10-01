# Clear Reading Cache

## Purpose and classification

Clear Reading Cache is the RiscRTE maintenance application that asks the firmware cache service to remove generated reading-cache data. It is classified as a foundational System App because it provides a core reader/system maintenance workflow.

Manifest metadata:
- version **1.0.1**
- minimum firmware **1.1.21**
- artifact `clear_cache.elf`
- icon `solid:f2ed`
- categories `System`, `Maintenance`

## Host interfaces

### `T5AppApi`

The app obtains ABI version 1 and requires `poll`. The required raw poll interface remains checked; interaction uses UI events at 50 ms intervals.

### `T5CacheApi`

The app obtains cache API version 1 and requires `clear_reading_cache(t5_cache_clear_result_t *)`. The current API header states that this provider operation deletes firmware-owned reading-cache directories only—`epub_*` and `xtc_*` beneath `/.crosspoint`—and returns aggregate `removed_count`, `failed_count`, and `directory_available` fields.

The app does not enumerate or delete cache paths itself.

### `T5UiApi`

The app obtains UI API version 1 and requires `render_list` and `poll_event`.

## User-visible workflow

The warning screen renders one highlighted row, **Clear cached reading data?**, and states that cached EPUB/XTC render data will be regenerated while book files and settings are not deleted.

Back cancels. Only a semantic Confirm event calls the cache provider. Taps on explanatory list rows do not authorize deletion; mapped action controls are resolved by the shared UI event API.

The result screen maps provider results as follows:
- provider returned false → **Failed**, status **Cache service unavailable**
- cache directory unavailable → **Nothing to clear**, status **Reading cache directory was not found**
- `failed_count > 0` → **Completed with errors**, with removed and failed counts
- otherwise → **Cache cleared**, with removed-item count

Back, Confirm, or any tap exits the result screen.

## Failure handling, storage, and persistence

The app does not retry a failed cache operation. It reports the aggregate provider result only.

It performs no direct file-system mutation, no network I/O, and defines no persistent app-specific state or file format. Cache path ownership and deletion semantics remain provider-owned.

## Implementation limits

- status buffer: 128 bytes
- warning view: exactly one list row
- result view: exactly one list row

## Source and build provenance

Source and manifest synchronized from Reader `1e0188c1ff0234dd33fe054c9a6fb4fde36596df`:
- `Apps/clear_cache.c`: `84c7daedf20b81cac64bb260a7a2fe3db19552be`
- `Apps/clear_cache.json`: `beeb66a9ce48e4ef64987af46303dc4e2f4c396d`

The SDK/ABI baseline remains independently pinned in `sdk/baseline.json`.
The matching app host fixture is pinned there to the inspected Reader master.
Published version 1.0.1 is copied unchanged, with canonical ELF identity from
release-index `572746f4fcf3fde19947a066b7e5c8028cd76d21` recorded in `sdk/release-baseline.json`.
Host fixtures exercise app/provider behavior; firmware touch/orientation dispatch
and hardware operation are not qualified by this external repository run.
