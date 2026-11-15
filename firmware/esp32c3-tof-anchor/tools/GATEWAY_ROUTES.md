# Gateway identity routes

The FTM collector can share the rover gateway's SQLite device-route registry:

    python tof_coordinator.py --camera http://camera-bootstrap-address --state STATE --device-routes GATEWAY_ROUTES_SQLITE --camera-id STABLE_CAMERA_ID

Every camera status/ranging request resolves its origin again from the stable device identity. A route update takes effect in the running collector, without firmware or process deployment. The bootstrap origin remains available when no identity row has been registered.

Anchor HTTP recovery consults stable ftm:BSSID registry entries and the host neighbor table. ESP station/AP address variants identify discovery candidates only. A candidate must return the expected rover.tof.anchor/1 BSSID and responder role before a measurement is accepted or either registry is updated. Incorrect neighbors cannot rebind an anchor.

Accepted observations overwrite one route row per device. Measurements, maps, baselines and refinement checkpoints stay in their existing stores. Existing callers without --device-routes retain the previous transport behavior. Neighbor discovery requires host reachability; a powered-off or unresponsive module still needs hardware recovery.

Validation covers same-process camera route changes, verified anchor address recovery, wrong-device rejection and both deployed ESP station/AP address forms. The source ToF suite passed85 tests, including4 new route tests. Those4 tests also passed against the installed runtime modules before restarting only the ToF collector.
