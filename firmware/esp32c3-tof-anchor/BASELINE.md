# Persistent relative anchor baseline

The coordinator loads tools/tof_baseline.py and saves baseline.json atomically beside latest.json.
It uses the existing robot.tof.status:1 / robot.tof.localize:1 source and robot.vision.stream:1 metadata.tof.
World alignment is deliberately unregistered. No navigation authority is granted.

Distinct successful sessions are keyed by source boot, target and range_seq. Packet repeats,
failed sessions and measurements older than30 seconds never add calibration evidence.
Each direction retains at most120 sessions within30 minutes; no raw journal grows indefinitely.
Default acquisition requires5 sessions per included direction spanning30 seconds.
Directional medians and MAD spreads feed a joint robust least-squares distance fit.
Missing edges are initialization guesses only, never measurement constraints.
Rank, conditioning, connectivity and redundant rigidity checks reject unobservable/ambiguous fits.
For larger sparse3D graphs no global-rigidity certificate is implemented, so the solver is conservative.
A partial baseline identifies unfitted anchors explicitly and upgrades when sufficient evidence arrives.
A planar baseline describes an intrinsic anchor plane; it is not a floor assumption.
A planar rover fix retains its two-sided ambiguity.

The saved fit stays fixed during ordinary noise. Movement uses recent90-second windows,
at least4 distinct post-fit observations spanning30 seconds, coherent changes on multiple links,
a threshold of at least1 metre (also scaled by range and robust statistical spread),
and a further30-second confirmation hold. A single bad link or lost node is not a move.
A unique minimal changed-link cover with unaffected control links identifies candidate moved anchors.
Broad/ambiguous changes are labelled movement_or_persistent_RF_change, not a physical-motion certainty.
On confirmation, old measurements touching affected anchors are discarded and rover fixes become invalid.
The solver automatically acquires a replacement fit. Stable common anchors align the new fit to the old
relative frame; without at least4 noncoplanar stable controls for3D (3 noncollinear for2D) a new frame_id is emitted explicitly.
Epochs, recent events, bounded samples and a small previous-fit history survive restarts.
Offline anchors alone do not invalidate their stored coordinates or fabricate fresh measurements.

All coordinates, residuals, model uncertainties, movement state and epoch are exposed in latest.json.
Model covariance/residuals are not guarantees of RF accuracy; persistent multipath can remain biased.
The socket compactor may replace baseline.positions with positions_reference=anchor_geometry and
omit duplicate per-edge diagnostics; full capability responses retain the complete solver result.

Dependencies: requirements-tof.txt. Installed Mac NumPy/SciPy were used without installing packages.
Run: python -m unittest discover -s tools -p 'test_tof*.py' -v
