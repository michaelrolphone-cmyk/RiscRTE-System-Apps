# Capability/API requirement identity

Provider 0.1.2 treats a requirement as the pair `(capability, API)`, matching
Runtime admission. An application may need `storage.key-value@2` for its own
records and `storage.key-value@1` for shared preferences. Both entries remain
required and neither is silently collapsed. Duplicate identical pairs still
fail, and authority comparison still requires every original pair.

The previous name-only check rejected the installed Spectrum 0.4.0 manifest
as well as its proposed updates. The regression fixture is copied from the
software-verified Watch 299bc1ba full image's `audio_spectrum.json`; it has
twelve requirements, including both KV APIs. Tests cover parsing the existing
manifest, a version-only update, duplicate-pair refusal and changed/missing API
authority, plus the complete service classification/download/cleanup path.

This parser repair does not make the old installed provider self-updatable.
Firmware OTA clones its old boot store and app OTA replaces only an authorized
app ELF/manifest. An existing 0.1.1 provider therefore needs a separately
verified bootstrap path preserving NVS/app-data before it can process this
Spectrum update. No requirements, native privileges or data layout change.
