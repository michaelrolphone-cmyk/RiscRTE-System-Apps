# Clear Reading Cache

## Purpose and classification

Clear Reading Cache is the RiscRTE maintenance application that asks the firmware cache service to remove generated reading-cache data. It is classified as a foundational System App because it provides a core reader/system maintenance workflow.

Manifest metadata:
- version **1.0.0**
- minimum firmware **1.1.21**
- artifact `clear_cache.elf`
- icon `solid:f2ed`
- categories `System`, `Maintenance`

## Host interfaces

### `T5AppApi`

The app obtains ABI version 1 and requires `poll`. It polls at 50 ms intervals.

### `T5CacheApi`

The app obtains cache API version 1 and requires `clear_reading_cache(t5_cache_clear_result_t *)`. The current API header states that this provider operation deletes firmware-owned reading-cache directories only—`epub_*` and `xtc_*` beneath `/.crosspoint`—and returns aggregate `removed_count`, `failed_count`, and `directory_available` fields.

The app does not enumerate or delete cache paths itself.

### `T5UiApi`

The app obtains UI API version 1 and requires `render_list` and `hit_test`.

## User-visible workflow

The warning screen renders one highlighted row, **Clear cached reading data?**, and states that cached EPUB/XTC render data will be regenerated while book files and settings are not deleted.

Back cancels. Confirm, or a touch that hit-tests to a rendered row, calls the cache provider.

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

## Source and interface identities

At upstream commit `525e32689203502a7b22f6350b7ef04f272271db`:
- `Apps/clear_cache.c`: `1c51c862f8b9626c1182302f1c2390f4cafb87e2`
- `Apps/clear_cache.json`: `86fc4a3edf36c7fd7ec7025d8539313d1e6e3a7a`
- `T5CacheApi.h`: `f3bd3a4a18dd80f7238b4d28533fac012041afcc`
- current `T5AppApi.h`: `fda810300de5cadff16e81efd42ba7efff8fe33b`
- `T5UiApi.h`: `ef09b405fc2518ee7ecf039f8b251939d80b14a6`
