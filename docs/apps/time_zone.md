# Time Zone

## Purpose and scope

Time Zone is the RiscRTE system-settings application for selecting a firmware-provided time-zone region and city. It is a UI front end over the firmware time-zone provider: the app itself does not calculate UTC offsets, implement daylight-saving rules, maintain a time-zone database, or define a persistence format.

This page is derived from the actual `Apps/time_zone.c` implementation and `Apps/time_zone.json` manifest.

## Package metadata

- App/source ID: `time_zone`
- Display name: `Time Zone`
- Version: `1.0.1`
- Minimum firmware: `1.1.24`
- Runtime artifact: `time_zone.elf`
- Icon: `solid:f0ac`
- Categories: `Settings`, `Time`

The manifest declares no optional capability array. Runtime dependencies are established directly by the API accessors used by the source.

## Source and entry point

The complete implementation is in `Apps/time_zone.c`. The app entry point is `app_main(void)`.

At startup it obtains three firmware-owned interfaces:

- `t5_app_get_api(T5_APP_ABI_VERSION)` -> `t5_app_api_v1`
- `t5_time_zone_get_api(T5_TIME_ZONE_API_VERSION)` -> `t5_time_zone_api_v1`
- `t5_ui_get_api(T5_UI_API_VERSION)` -> `t5_ui_api_v1`

The app returns immediately if any API table is missing or if any required function pointer is unavailable.

## App interface usage

The application uses `app->poll(&input, 50)` for host input. It consumes:

- `exit_requested`
- `buttons`
- `tapped`
- `touch_x`
- `touch_y`

The source uses Back, Up, Down, Left, Right, and Confirm button bits. The poll interval is 50 ms. A failed poll or host exit request terminates the application.

No filesystem, network, package, process-launch, or persistence calls are made through the app API.

## Time-zone interface usage

The app requires these provider operations:

- `region_count()` to enumerate available regions.
- `region_info(index, &info)` to obtain a region name and selected flag.
- `city_count(region)` to enumerate cities in a selected region.
- `city_info(region, index, &info)` to obtain a city name and selected flag.
- `select_city(region, city)` to apply the user's final choice.

The provider remains the source of truth. The app keeps only temporary navigation state and does not duplicate the configured zone into app-owned storage.

## UI interface usage

The app requires:

- `render_list` to render both region and city lists.
- `hit_test` to map touch coordinates to a row index.
- `next_index` and `previous_index` to move through the active list.

It does not use app-specific drawing primitives or maintain its own framebuffer.

## Region workflow

The app starts in region mode. It asks the provider for the region count, caps the visible count at 96 rows, and fetches region metadata into static storage.

The region screen uses:

- Title: `Time Zone`
- Subtitle: `Select a region`
- Back label: `Back`
- Confirm label: `Select`
- Previous label: `Up`
- Next label: `Down`

Each successfully loaded row displays the provider-supplied region name. A region whose metadata has `selected=true` receives the value text `Selected`.

During startup the app scans the available regions, up to the 96-row limit, for the provider-selected region and places selection on that row.

Activating a region only enters city mode. It does not change the configured time zone.

## City workflow

When a region is activated, the app records that region, enters city mode, and scans its cities—again capped at 96—for the currently selected city.

The city screen title is the selected region's name and the subtitle is `Select a city`. Provider-selected city rows show the value `Selected`.

Activating a city calls `select_city(region, selected)`. If the provider returns true, the app exits. If it returns false, the app remains active.

## Navigation and touch behavior

- Back in city mode: return to the region list and restore the region row as the active selection.
- Back in region mode: exit the app.
- Up or Left: move to `previous_index`.
- Down or Right: move to `next_index`.
- Confirm: activate the selected region or city.
- Tap: run `hit_test`; if it resolves to a valid row, select that row and immediately activate it.
- Host exit request: exit immediately.

The list is rerendered after directional selection changes and when switching between region and city modes.

## In-memory state and implementation limits

The implementation is allocation-free and uses static arrays:

- `t5_ui_list_row_t rows[96]`
- `t5_time_zone_region_info_t region_info[96]`
- `t5_time_zone_city_info_t city_info[96]`

Other persistent-in-process variables track row count, active region, selected row, and whether city mode is active.

`MAX_ROWS` is 96. Provider counts above that are truncated for display and navigation. If the active selection becomes invalid after loading rows, it is normalized to row zero. A zero-row list also leaves selection at zero.

Failed `region_info` or `city_info` calls are skipped; the app continues loading subsequent entries.

## Persistence and storage

The app performs no direct file I/O and defines no app-specific state directory or file format. Any persistence caused by `select_city` belongs to the firmware time-zone provider.

## Network and hardware

The implementation contains no network operations and no direct hardware access. It relies entirely on firmware-owned APIs for input, UI rendering, and time-zone configuration.

## Failure behavior

- Missing API table: exit before rendering.
- Missing required API function pointer: exit before rendering.
- Invalid selected row passed to activation: no action.
- Failed provider metadata lookup: skip that row and continue.
- Failed `select_city`: remain in the app.
- Failed input poll: exit.

No separate error screen, retry dialog, or status message is implemented.

## Build and packaging

The manifest names `time_zone.elf` as the application artifact. This repository migration preserves the upstream source and manifest identity. Repository-level build, CI, release, and publication automation are separate from the app implementation.

## Not specified by this app

The source does not establish the underlying zone database, UTC-offset computation, daylight-saving policy, persistence schema, RTC behavior, or network time synchronization. Those concerns are intentionally not documented here as app guarantees.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.1**; the application C source is synchronized without source edits. Source blob `7c5fd087bc12b7114cd9bbf61d1b142dfadf43c8`; manifest blob `350b5af07fee6b533f71addecf838b90e14ac259`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-time_zone-v1.0.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-time_zone-v1.0.1/application-time_zone-1.0.1-xtensa-esp32s3.rte.zip) has a **4435**-byte package with SHA-256 `6d2ce40503961940f6afeb1fd262aac2a18ceda315d30c751b46e0ed72ae8249`. Its embedded `time_zone.elf` is **3404** bytes with SHA-256 `c6092d550e9f50a6454ce448e390f6aef735eb7831357ae31ea98b13497e313f`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
