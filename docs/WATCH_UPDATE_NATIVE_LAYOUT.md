# Shared Watch update views at native height

The opt-in PORTABLE_WATCH_NATIVE_HEIGHT path uses the existing width/240 transform for both axes. WatchViewLayout.h anchors the existing two-row view footer to native height and maps touches through the same transform. wifi_nova.inc retains its default square behavior and keyboard layout. update_portable.inc uses matching footer coordinates, and adapter.c transforms the shared header Back hit region while preserving physical app input coordinates.

The real portable update controller/renderer fixture ran 312 UBSan scenarios across both application kinds, default 240x240, native 240x240, native 410x502 and native 480x600. Tests include confirmation/cancel/restart behavior, cleanup and radio failures, sleep/alarm paths, native footer actions, header Back and exhaustive row-hit boundaries. The prior native-height header Back test failed before the adapter correction. Rendered 410x502 confirmation output was visually inspected.

build_watch_shared_updates.py inherits the complete Watch update feature definitions, including Contexts, broadcasts, Quick Controls, local navigation/sleep, low battery and motion wake; it selects checked custody and the qualified Watch alarm header. Both target ELFs pass structural/import/export checks and lossless loader metadata compaction. No product feed URL is compiled into these shared apps. Target builds and host fixtures do not qualify physical display, touch, sleep, or network behavior.

These applications are component candidates, not an installed product composition. Their exact boot authority and complete cohort still need admission after capacity is resolved. No old application ELF was replaced.
