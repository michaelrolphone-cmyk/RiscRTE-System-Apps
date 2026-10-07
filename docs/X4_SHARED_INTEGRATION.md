# Shared X4 client integration

This branch combines explicit Clock light sleep, the app-owned launch guard,
fresh contact/committed QuickActions ownership, Nova7 Wi-Fi and update app
presentations. Original unit commits remain parents. The power status accessor
and its tests are copied byte-for-byte from accepted Watch System Apps
f146d82d4c2bc4d4b6ac97f7be83b04035ad7d60; the accessor reads the adapter-owned
gauge binding without another capability acquire/release or a native ABI change.

Shared rebuilt package versions are recorded in their manifests and inventory;
the sleep Clock is0.2.2. The updater presentation is ready for product testing,
but Services/update/Catalog.h still selects the Watch release feed. An X4 product
must not advertise a working updater until its own feed/service configuration is
qualified. No device operation, credentials or network install is performed here.

The Wi-Fi renderer tests gzip their exact raw pixel captures after each scenario
and compare decompressed portrait/native bytes. This keeps the same assertion
while avoiding a roughly600MiB transient raster footprint.
