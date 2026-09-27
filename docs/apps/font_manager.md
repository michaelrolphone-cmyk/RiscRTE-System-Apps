# Manage Fonts

Manage Fonts is the foundational appearance/settings app for installing, updating, and removing SD font families through firmware `T5FontApi` version 2. Upstream version **1.0.0** requires firmware **1.1.24** and builds as `font_manager.elf`.

It refreshes the provider-owned catalog and renders at most 64 families. Rows show the provider name/description plus Update available, Installed, or total size rounded up to KiB. Up/Left and Down/Right navigate; Back/exit leaves; tap or Confirm activates. An installed family with no update requires two activations on the same row before `delete_family` is called. Other activation calls `install_family` and redraws provider progress with family, file index/count, and downloaded/total bytes.

Provider errors are surfaced as Done, Network error, Invalid font catalog, Storage error, Checksum error, Invalid font file, Invalid font selection, or Font service unavailable. Network endpoints, downloads, checksums, storage paths, and persistence are provider-owned and not established by this app. Missing required APIs/functions causes immediate return.

Audited at upstream `525e32689203502a7b22f6350b7ef04f272271db`: source `f188b6f5eaf0ee7c2746cee3736ebbe9951ce141`, manifest `2ae0e6992dfca124b682a81dc1b4f9976182bcb7`, `T5FontApi.h` `f019bf858c9ec85d316ac35c79cf12ab8139a7c2`. No independent destination build/release artifact is established yet.
