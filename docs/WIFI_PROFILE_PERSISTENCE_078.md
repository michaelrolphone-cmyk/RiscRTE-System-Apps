# X4 Wi-Fi 1.1.26: remember successful connections

Successor to scan checkpoint `f5673befd745a744872e4802332e647d64577a5c`
(1.1.25), on the exact source-selected X4 .78 Wi-Fi base. The native .2.5,
Wi-Fi provider .2.1, renderer, shared masked keyboard and storage ABI are unchanged.

## Selected-path findings

The .78 app's successful async association only displayed “Connected; Save
stores this network”. It never called the profile transaction. The existing
50-profile tests explicitly called the save codec or Save action, so they did
not prove persistence after Connect. The new baseline-compatible
`wifi_connect_save_probe.c` fails against the selected original controller and
passes with this change. This is a behavior correction: earlier source expressly
required separate Save. It is not evidence of a defective NVS flash chip.

Every connected-status poll also replaced the message, hiding Save results and
errors. Failed Forget discarded the draft/selected slot before success was known.
Those two source defects are corrected.

The exact .78 boot grants `storage.key-value@1`, instance/namespace **6** to
`wifi_settings.json` (and namespace 1 for radio preferences). The selected app
builder uses `PORTABLE_WIFI_STORAGE_INSTANCE=6`. Runtime resolves that grant to
`keyValueNamespace`, validates keys and caps v1 blobs at 64 bytes. Native
`NvsKeyValue.h` maps it to **rte00000006**, calls `nvs_set_blob`, `nvs_commit`,
and verifies readback. Both Runtime and NVS accept the existing profile keys.
No selected namespace or grant mismatch was found.

Slot zero retains legacy `wifi.commit`, `wifi.a0/a1`, `wifi.b0/b1`. Other profiles
have independent `wXXXXXXXX.c/a0/a1/b0/b1` keys; `wifi.profiles` reserves the growing
index. The unchanged codec writes inactive chunks before its selector, validates
CRC/generation/readback, preserves existing profiles under interrupted writes,
and reuses empty slots. No separate storage implementation or migration is added.

## Behavior

Only the selected resident X4 builder enables `PORTABLE_WIFI_REMEMBER_CONNECTED`.
An accepted join copies its credentials; the first confirmed CONNECTED result
queues a bounded profile search/save. Failed/cancelled joins do not save. Editing
the draft cannot change the credentials that actually associated. Repeated
connected polling neither repeats writes nor overwrites the save/error message.
The list remains multi-profile; a different SSID uses another slot, while the
same SSID updates its profile. Explicit Save remains available for edits/retry.

Each save/search step pairs the existing service lease across its complete storage
transaction, preventing native worker contention between chunk and selector
writes. BUSY before starting defers without writes. Pending saves participate
in cooperative suspend/close; Back can explicitly cancel. Failed writes stay
unconfirmed and require user retry. Failed Forget retains its draft and selected
slot for retry, while successful Forget tombstones only that profile.

Reentry loads legacy slot zero as before; Saved networks selects other profiles.
This does not introduce auto-connect, change which profile other apps treat as
the default, or move settings to SD. Saved values are app-owned plaintext records;
password masking remains display-only, with no new credential logging.

## Reproduction and verification

```
python3 scripts/test_wifi_persistence.py --runtime Runtime --output evidence/persistence
python3 scripts/test_wifi_resident_checkpoint.py --runtime Runtime --watch Watch --output evidence/resident
python3 scripts/test_wifi_workflow.py --output evidence/workflows
```

The new runner executes normal and UBSan tests through the actual controller,
profile codec and **production native NVS adapter**. Its SDK boundary persists
synthetic NVS blobs in a file; separate processes model cold restart without
retaining app globals, grants, handles or provider state. Outer grants, radio,
keyboard and peripherals are synthetic. This proves source-path behavior, not
physical flash durability or a hardware reboot. Tested: failed association with
zero writes, two distinct networks without replacement, app reentry, cold-process
reload, saved selection, masked edit, failed/successful delete, empty-slot reuse,
BUSY with zero writes, failed save and explicit retry, pending close, copied join
credentials, and open networks. Existing transaction faults and scan regressions
remain required. No real credentials or hardware are used.

Build Wi-Fi **1.1.26** with the same .78 recipe and historical resident SDK as
1.1.25. Replace only its internal ELF/manifest in the next qualified assembly.
Reader .1.8, Daily Paper .1.0, panel .1.12, native, provider, SD split and other
apps remain pinned. See the companion X4 checkpoint for exact source/build hashes.
