# Production raster evidence

These PNGs are actual production C app/adapter raster output from the callback
fixtures in `test_usb_transfer.py` and `test_quick_usb_transfer.py`, rotated from
native 800x480 scan orientation into logical 480x800 touch coordinates. The
six-state overview is assembled from those screenshots. Provider faults are
simulated; this is not a hardware run.

The Quick Actions fixture selects the new entry and preserves existing
brightness/on-off, notifications, silent, DND, airplane, Wi-Fi, Bluetooth,
torch, close and battery controls. Radio availability reflects fixture grants.
The retained/suspended fixtures show an injected provider error string.
