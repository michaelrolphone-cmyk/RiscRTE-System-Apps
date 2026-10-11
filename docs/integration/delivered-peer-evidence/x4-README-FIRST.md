# Peer Files test installation

WARNING: The FULL-INITIAL-ERASES-DATA.bin file is a complete 16 MiB image for address 0x0. Installing it replaces internal firmware, both application banks, NVS/settings (including saved Wi-Fi), and internal AppData/user records. Back up wanted internal files/settings first. It is not a preserving update. No device was flashed during this work.

## Fresh installation

1. Use only the archive for your device. Keep a copy of your existing internal data before proceeding.
2. For X4, merge the supplied SD/Apps, SD/fonts and SD/Drivers directories into the SD card root. Keep unrelated user files. Insert the card before starting the new image. Watch needs no SD files in this composition.
3. Put the device in its usual USB bootloader mode. With the ESP32-S3 esptool v4 toolchain, select your own serial port and run the following command (replace PORT and IMAGE with your actual values):

   python -m esptool --chip esp32s3 --port PORT write_flash 0x0 IMAGE

   IMAGE is the FULL-INITIAL-ERASES-DATA.bin file at the archive root. Do not flash a preserving cohort payload at 0x0. This document does not execute any command.
4. Restart, configure your Wi-Fi network again, and check Files and the existing apps. These builds passed software checks; device radio interoperability and live heap margins remain untested.

## Connect Watch and X4

1. Save Wi-Fi settings on both devices for the same trusted local network. Enable the applicable radio policy. End USB SD export before browsing or receiving files on X4.
2. On the device offering files, open Files, select the intended local volume, open Share files, and select Start HTTP. Wait until its address and temporary password are displayed.
3. Select Bluetooth setup on that sharing screen. On the other device open Files > Nearby files and select the device advertising its Files session.
4. Compare the numbers displayed on both devices. Select Match only when they are identical; otherwise Reject. BLE brokers the temporary session; file payloads travel by WebDAV over Wi-Fi.
5. Browse the peer and choose a file operation. To upload, select a local file before entering Nearby files, then choose Upload selected local file. Downloads can be opened through the normal declared file-type handler after transfer and connection cleanup.
6. Use Stop or Back / Stop when finished. Sessions expire after five minutes; a new session uses fresh credentials. HTTP authentication does not encrypt file contents. Use a trusted network.

## Exact Watch user-file exports

| Owner | AppData namespace | Exported path |
|---|---:|---|
| timecard | 1 | /timecard/timecard.json |
| audio_spectrum | 2 | /audio-spectrum/spectrum-events-a.sqt |
| audio_spectrum | 2 | /audio-spectrum/spectrum-events-b.sqt |
| waterfall | 3 | /waterfall/rf-events-a.rft |
| waterfall | 3 | /waterfall/rf-events-b.rft |
| lists | 62 | /lists/lists.bin |
| model_viewer | 64 | /models/model.obj |
| model_viewer | 64 | /models/model.stl |

These are the existing owners' named files, with read/create/revision-checked replacement. This AppData API does not expose arbitrary rename/delete/mkdir. Credentials, BLE bonds and internal metadata are not exported. X4 SD retains generic file management. Listings over64 entries or32 KiB XML reject explicitly. OTS remains backlog.

## Preserving alternative for the actual delivered X4 0.1.81

The preserving/ folder contains a different format for the existing paired-cohort updater. Starting baseline is the delivered0.1.81/aac8650 bundle, boot SHA92bc02d99223d0f261863e83a944ca559c8ea4440f646574255569947e8945bb. Apply and confirm its included0.1.82 bridge (source 747b7beaa4d6a296c29cf534682f454d89da8cb8), then apply and confirm0.1.83. Merge the SD companion tree before activating0.1.83. These payloads contain native firmware plus boot store only, never NVS or AppData. Hosting URLs are not configured; use the existing paired-cohort update transport with the included candidate metadata. Do not write these files using esptool.

Do not substitute the earlier staging-only0.1.82/1108e7b bridge. This archive includes only the bridge for the delivered .81 baseline. The .81 export map and Files KV6 grant already exist, so normal preservation rules apply; the V2 authority record remains inert for this distinct source.

Full image SHA256: 0902c78c9408a7138134e9e1ca8925000270270b86f3accf01fa6a8ab041ceef
