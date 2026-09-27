# RiscRTE System Apps

Independent source repository for foundational RiscRTE system applications migrated from `michaelrolphone-cmyk/T5S3-Reader`.

## Scope and classification

This repository contains applications required for basic operation and first-use system workflows: launching installed apps, installing/updating/managing software and packages, configuring the runtime, foundational file browsing/dispatch, runtime driver-package management, and device firmware maintenance. Optional hardware utilities, productivity tools, MCU-development/debug tools, games, ROM/catalog apps, LLM-specific apps, KOReader-specific tools, and other domain-specific applications belong elsewhere.

During migration, `T5S3-Reader` remains a strictly read-only upstream source of truth.

## Application documentation

- [Springboard / Apps](docs/apps/springboard.md) — installed-app discovery, launcher grid, launch handoff, page/edit controls, and Home-screen pin workflow.
- [App Store](docs/apps/app_store.md) — release catalog, application install/update, SD package Inbox, uninstall workflow, and compact install-state icons.
- [Settings](docs/apps/settings.md) — front end for the firmware-owned settings model and complex setting-action handoff.
- [Remap Front Buttons](docs/apps/button_remap.md) — core front-button role mapping, duplicate-assignment prevention, reset-to-defaults, and provider-owned persistence.
- [Clear Reading Cache](docs/apps/clear_cache.md) — confirmation and reporting workflow for firmware-owned EPUB/XTC reading-cache cleanup.
- [File Browser](docs/apps/file_browser.md) — SD/removable-storage browsing, file-handler dispatch, native ELF launch, rename/move/delete actions, and SD↔USB file copy.
- [Time Zone](docs/apps/time_zone.md) — firmware-owned region/city selection, list navigation, touch handling, and time-zone provider interaction.
- [Wi-Fi Networks](docs/apps/wifi_settings.md) — firmware-owned wireless selection handoff, connection-status display, and core network configuration workflow.
- [Package Manager](docs/apps/package_manager.md) — installed-package inventory, SD Inbox, verified install, replacement/downgrade, and uninstall.
- [Driver Manager](docs/apps/driver_manager.md) — online/SD driver packages, install progress, compact installed-state icons, and retained-stage recovery.
- [SD Firmware Update](docs/apps/sd_firmware_update.md) — selected-image validation, confirmation, progress reporting, firmware installation, and restart handoff.
- [Firmware Update](docs/apps/ota_update.md) — firmware-owned OTA check/install/progress/restart workflow.
- [Language](docs/apps/language_settings.md) — firmware interface-language selection through the language provider.
- [Customize Status Bar](docs/apps/status_bar_settings.md) — provider-defined status-bar item activation and appearance settings.
- [Manage Fonts](docs/apps/font_manager.md) — downloadable SD-font catalog, install/update progress, and removal.
- [Font Family](docs/apps/font_selection.md) — reader font-family selection through the firmware font provider.
- [Image Viewer](docs/apps/image_viewer.md) — image file dispatch, probing, firmware decoding/rendering, and the current PNG safety guard.
- [File Transfer](docs/apps/file_transfer.md) — handoff to the firmware-owned wireless/Calibre/hotspot file-transfer session.

## Repository tree

```text
Apps/
  app_store.c
  app_store.json
  button_remap.c
  button_remap.json
  clear_cache.c
  clear_cache.json
  driver_manager.c
  driver_manager.json
  file_transfer.c
  file_transfer.json
  font_manager.c
  font_manager.json
  font_selection.c
  font_selection.json
  image_viewer.c
  image_viewer.json
  language_settings.c
  language_settings.json
  ota_update.c
  ota_update.json
  status_bar_settings.c
  status_bar_settings.json
  file_browser.c
  file_browser.json
  package_manager.c
  package_manager.json
  sd_firmware_update.c
  sd_firmware_update.json
  settings.c
  settings.json
  springboard.c
  springboard.json
  time_zone.c
  time_zone.json
  wifi_settings.c
  wifi_settings.json

docs/
  apps/
    app_store.md
    button_remap.md
    clear_cache.md
    driver_manager.md
    file_transfer.md
    font_manager.md
    font_selection.md
    image_viewer.md
    language_settings.md
    ota_update.md
    status_bar_settings.md
    file_browser.md
    package_manager.md
    sd_firmware_update.md
    settings.md
    springboard.md
    time_zone.md
    wifi_settings.md

system-apps-manifest.json
```

## Migration and parity policy

- App source and manifests remain byte-identical to the approved upstream blobs while `T5S3-Reader` is authoritative.
- `system-apps-manifest.json` records the approved System App set, upstream identities, versions, and migration state.
- Every approved app has one implementation-derived documentation page linked above.
- Documentation is derived from current source, manifests, and ABI/interface headers; it does not define provider-owned behavior that the app source does not establish.
- A source/docs migration is not the same as full parity readiness. Build, test, packaging, release automation, and published-version parity must also be established before an app is reported parity-ready.

## Current parity limitations

This bootstrap repository does not yet contain a complete independent compatibility-header/toolchain/build/release pipeline for all listed apps. The tracked app sources, manifests, README links, and dedicated documentation are synchronized to the audited upstream identities recorded in `system-apps-manifest.json`, but build/test/package/release parity is still pending. The manifest distinguishes source/document migration state from full parity readiness. GitHub remains the source of truth for integrated work; staged Drive handoffs are recovery/integration aids only.

Until the runtime is officially switched to this repository, relevant upstream application changes are synchronized here without modifying `T5S3-Reader`.
