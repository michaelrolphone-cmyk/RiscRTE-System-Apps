# Clear Reading Cache

## Purpose and classification

Clear Reading Cache is the RiscRTE maintenance application that asks the firmware cache service to remove generated reading-cache data. It is classified as a foundational System App because it provides a core reader/system maintenance workflow.

Manifest metadata:
- version **1.0.2**
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

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` supplies the matching application source and the manifest is synchronized at version **1.0.2**. Source blob `84c7daedf20b81cac64bb260a7a2fe3db19552be`; manifest blob `a2c6be0f8f7430edb56caab03eef7db4a8aec4b0`. The SDK/ABI baseline remains independently pinned in `sdk/baseline.json`.

Reader published release [`app-clear_cache-v1.0.2`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-clear_cache-v1.0.2/application-clear_cache-1.0.2-xtensa-esp32s3.rte.zip) contains a `4624`-byte package with SHA-256 `7456674bf42fcb73709a65cd648ec741f4db64181920f72d82533089adf872b1`. The downloaded package contains `clear_cache.elf` (3560 bytes, SHA-256 `8d2eaa8ea68bae515fd2b51ce4553ba723d58c8fd838a5ded6c125dee1d77b30`). Release metadata and the downloaded workflow artifact agree; the ELF identity matches the earlier 1.0.1 release, so no additional bump was needed. The release was produced from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; these app inputs are unchanged at current Reader master.

Host fixtures exercise app/provider behavior; device operation and U1 runtime readiness are not established by this evidence.
