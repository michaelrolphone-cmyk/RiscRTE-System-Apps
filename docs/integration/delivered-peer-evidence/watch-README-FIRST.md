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

## Preserving alternative for the actual delivered Watch 1.1.2

The preserving/ folder contains payloads for the existing paired-cohort updater: delivered1.1.2/e73c91e -> bridge1.1.3/89a2a76 -> peer1.1.4/e3c24a2. Apply and confirm the bridge first, then the target. The bridge preserves the one-file map and installs the native-selected V2 authority for the exact eight-file expansion. Payloads exclude NVS and AppData. Hosting URLs are not configured; use the existing paired-cohort transport with the candidate metadata. Never flash these payloads at0x0.

Full image SHA256: b19673297eca3f9323f97004203aaedbe9eb3c146d4b738ef1a2c6096b380ede
