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

The saved fit changes only through validated bounded refinement during ordinary noise. Movement uses recent90-second windows,
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


## Continuous refinement

Every30 seconds the solver checks for genuinely new sessions across at least half
the fitted links (at least3). It retains two recent sessions per direction as
validation data and fits the remaining rolling history jointly in the existing
dimension. A candidate must remain observable, align to the current frame and
preserve conditioning. Accepted bounded steps must improve both robust loss and
RMS against the SAME current edge statistics and must pass held-out validation.
A step is at most0.1m per anchor and uses at most25% of the candidate correction.
No new data, insufficient coverage, offline nodes, suspected movement or rejected
validation leave coordinates unchanged. Full rings continue to recognize new
sessions by arrival watermark; a fixed sample count never disables refinement.

Refinement increments baseline.revision, not the movement epoch or frame ID.
The current residual may rise when incoming evidence changes: it is not made
artificially monotonic by comparing errors from different measurement windows.
Each accepted update reports same-window before/after RMS, validation metrics,
the maximum coordinate step, accepted count and timestamps in baseline.refinement.
These diagnostics survive restarts and are carried through the existing socket.

Movement compares recent ranges to an independent reference retained from the
last physical calibration. Refinement never updates that reference, preventing
gradual physical movement from being hidden by successive small coordinate changes.

Recovery accepts the largest observable anchor subset while a moved/unreliable node requalifies, rather than requiring every previously fitted anchor to recover at once. Unfitted nodes stay explicit and are added automatically when observable.

Once noncoplanar3D geometry is established, expansion cannot silently downgrade the network to a plane to include an inconsistent anchor. Legacy downgrades are revalidated using retained3D evidence. Intrinsic-plane rover hypotheses are computed in that plane's basis and transformed back, so a tilted plane is not mistaken for horizontal z=0.
