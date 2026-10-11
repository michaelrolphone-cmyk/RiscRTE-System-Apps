# Using this shared Files test composition

1. Configure the saved local network in Wi-Fi Settings. Open Files, then its actions menu and **Share over Wi-Fi**.
2. Select **Start HTTP**. Keep Files in the foreground. The screen shows the device URL (port8080), username **files**, a fresh temporary password and the remaining five-minute lifetime.
3. On another device on the same network, add that exact URL to a WebDAV client supporting HTTP Digest authentication, using the displayed username/password. X4 exposes the selected SD volume; Watch exposes only the explicitly configured saved-file export (Timecard in these test bundles), not an SD card or all private app data.
4. **Bluetooth setup** is optional: a compatible setup client selects **RiscRTE Files**; compare the number on both devices and choose **Match** only if equal. BLE brokers the temporary connection descriptor; it does not carry file data or create a remote browser.
5. **Stop**, leaving sharing, or the five-minute expiry ends the session. Restarting creates new credentials.

These builds provide a WebDAV server and the existing local Files browser. They do not include an on-device WebDAV client, so X4-to-Watch/Watch-to-X4 remote directory browsing is not implemented. Use a separate compatible WebDAV client for remote browsing. A normal browser is not a full WebDAV file manager. HTTP file contents are unencrypted; use the trusted local network indicated by the UI. BLE pairing does not encrypt HTTP transfers. Client interoperability has not been tested on hardware.

Packaging correction: X4 product0.1.81 and Watch product1.1.2 preserve all functional code and component versions from the qualified unversioned test bundles. Runtime0.2.8 / Files1.5.24 remain unchanged. X4 SD0.2.19 is the published dynamic-discovery/removal baseline, not the older0.2.16. Unfinished installer transaction/journal changes are excluded.
