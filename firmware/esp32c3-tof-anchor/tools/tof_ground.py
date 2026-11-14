"""Surveyed floor geometry and independent, directed radio checks in metres."""
import math
import statistics

import numpy as np
from scipy.optimize import least_squares


def surveyed_floor(document, scale, frame, fixed):
    plane = document.get("ground_plane")
    if plane is None:
        return None
    elevation = plane["elevation"] * scale
    return {"valid": True, "status": "surveyed_floor", "frame_id": frame,
            "normal_anchor": [0., 0., 1.], "offset_m": -elevation,
            "elevation_m": elevation, "axes": "east_north_up",
            "source": "surveyed_floor_datum", "reference_anchors": sorted(fixed),
            # Display convention used by the existing floor view: east, up, south (right-handed).
            "T_floor_anchor": [[1., 0., 0., 0.], [0., 0., 1., -elevation],
                               [0., -1., 0., 0.], [0., 0., 0., 1.]]}


def radio_calibration(history, fixed, now, recent_s=180.):
    """Estimate signed errors using disjoint time windows and independent peers."""
    links = []
    groups = {}
    for key, samples in sorted(history.items()):
        a, b = key.split(">")
        if a not in fixed or b not in fixed or a == b:
            continue
        truth = math.dist(fixed[a], fixed[b])
        recent = [r for r in samples if 0 <= now-r[0] <= recent_s]
        if len(recent) < 3:
            continue
        bias = statistics.median(r[2] for r in recent) - truth
        before = [r for r in samples if recent_s < now-r[0] <= 3*recent_s]
        row = {"from": a, "to": b, "survey_distance_m": truth, "bias_m": bias,
               "recent_sessions": len(recent), "last_observed_at": max(r[0] for r in recent),
               "drift_m": None, "drift_m_per_min": None}
        if len(before) >= 3:
            drift = bias - (statistics.median(r[2] for r in before) - truth)
            delta = statistics.median(r[0] for r in recent) - statistics.median(r[0] for r in before)
            row.update(drift_m=drift, drift_m_per_min=60*drift/delta,
                       reference_sessions=len(before))
        links.append(row)
        groups.setdefault((a, "anchor_to_rover"), []).append(bias)
        groups.setdefault((b, "rover_to_anchor"), []).append(bias)
    corrections = {}
    for (anchor, direction), biases in groups.items():
        if len(biases) < 2:
            continue
        # Transfer only the component that predicts held-out *other peers*.
        # Link-specific multipath and the rover's own radio delay are unobserved.
        errors = [v-statistics.median(biases[:i]+biases[i+1:]) for i, v in enumerate(biases)]
        before = math.sqrt(statistics.mean(v*v for v in biases))
        after = math.sqrt(statistics.mean(e*e for e in errors))
        if after < before and after <= .75:
            corrections.setdefault(anchor, {})[direction] = {
                "subtract_m": statistics.median(biases), "peers": len(biases),
                "holdout_raw_rms_m": before, "holdout_corrected_rms_m": after}
    return {"method": "directed_survey_peer_holdout", "window_s": recent_s,
            "links": links, "corrections": corrections}


def calibrated_distance(anchor, direction, distance, calibration):
    model = calibration.get("corrections", {}).get(anchor, {}).get(direction, {})
    return distance - model.get("subtract_m", 0.)


def ground_observations(rows, calibration, refined):
    result = []
    for row in rows:
        if row.get("consistent") is False:
            continue
        anchor = row["target_bssid"]
        if refined:
            # Independently deduplicated raw measurements from this stationary segment.
            parts = row.get("ground_directions", [])
        else:
            parts = row.get("directional_measurements", [row])
            if row.get("method") != "reciprocal_median" and parts:
                parts = [max(parts, key=lambda p: p["observed"])]
        if not parts:
            continue
        weights = [p.get("session_count", 1) for p in parts]
        values = [calibrated_distance(anchor, p.get("direction"), p["distance_m"], calibration)
                  for p in parts]
        d = float(np.average(values, weights=weights))
        raw = float(np.average([p["distance_m"] for p in parts], weights=weights))
        if d <= 0:
            d = raw
        result.append(dict(row, distance_m=d, raw_distance_m=raw, correction_m=raw-d,
                           session_count=sum(weights)))
    return result


def fit_ground(baseline, observations, radio_height):
    floor = baseline["ground_plane"]
    points = {p["bssid"]: [p["x_m"], p["y_m"], p["z_m"]] for p in baseline["positions"]}
    rows = [r for r in observations if r["target_bssid"] in points]
    out = {"valid": False, "position": None, "status": "awaiting_ground_ranges",
           "ground_fit": {"anchor_count": len(rows), "antenna_height_above_floor_m": radio_height}}
    if len(rows) < 3:
        return out
    anchors = np.array([points[r["target_bssid"]] for r in rows])
    ranges = np.array([r["distance_m"] for r in rows])
    z = floor["elevation_m"] + radio_height
    xy = anchors[:, :2]
    dz = anchors[:, 2] - z
    design = 2*(xy[1:] - xy[0])
    if np.linalg.matrix_rank(design, tol=1e-5) < 2:
        out["status"] = "unobservable_ground_geometry"
        return out
    horizontal_sq = ranges**2 - dz**2
    squared = (xy**2).sum(axis=1)
    initial = np.linalg.lstsq(design, horizontal_sq[0]-horizontal_sq[1:]+squared[1:]-squared[0], rcond=None)[0]
    def residual(p):
        return np.sqrt(((xy-p)**2).sum(axis=1)+dz**2)-ranges
    # Check both sides of a long narrow anchor layout for a competing solution.
    center = xy.mean(axis=0)
    across = np.linalg.svd(xy-center, full_matrices=False)[2][-1]
    width = max(1., float(np.median(ranges)))
    fits = sorted((least_squares(residual, start, loss="soft_l1", f_scale=.5)
                   for start in (initial, center+width*across, center-width*across)), key=lambda f:f.cost)
    best = fits[0]
    errors = residual(best.x)
    rms = float(np.sqrt(np.mean(errors**2)))
    threshold = max(.75, .15*float(np.median(ranges)))
    out["ground_fit"].update(residual_rms_m=rms, residual_acceptance_m=threshold,
                             ranges=[{"anchor": r["target_bssid"], "raw_m": r["raw_distance_m"],
                                      "correction_m": r["correction_m"], "used_m": r["distance_m"],
                                      "residual_m": float(e), "transfer": r.get("transfer_check", "none"),
                                      "proposed_correction_m": r.get("proposed_correction_m", r["correction_m"])} for r,e in zip(rows, errors)])
    if not best.success or rms > threshold:
        out["status"] = "inconsistent_ground_ranges"
        return out
    if any(np.linalg.norm(f.x-best.x) > 1. and
           np.sqrt(np.mean(residual(f.x)**2)) <= rms+.10 for f in fits[1:]):
        out["status"] = "ambiguous_ground_ranges"
        return out
    out.update(valid=True, status="grounded_position", position={
        "x_m": float(best.x[0]), "y_m": float(best.x[1]), "z_m": z,
        "ground_z_m": floor["elevation_m"], "frame_id": baseline["frame_id"],
        "baseline_epoch": baseline["epoch"], "baseline_revision": baseline["revision"],
        "residual_rms_m": rms, "residuals_m": errors.tolist()})
    return out

def validate_transfers(baseline, rows, radio_height):
    """A different radio/path can have different delays. Predict it from other anchors."""
    checked = []
    for row in rows:
        if not row["correction_m"]:
            checked.append(row)
            continue
        peers = [dict(r, distance_m=r["raw_distance_m"], correction_m=0.)
                 for r in rows if r["target_bssid"] != row["target_bssid"]]
        check = fit_ground(baseline, peers, radio_height) if len(peers) >= 3 else {}
        anchor = next((p for p in baseline["positions"] if p["bssid"] == row["target_bssid"]), None)
        if check.get("valid") and anchor:
            pos = check["position"]
            predicted = math.dist([pos[k] for k in ("x_m","y_m","z_m")],
                                  [anchor[k] for k in ("x_m","y_m","z_m")])
            if abs(row["distance_m"]-predicted) < abs(row["raw_distance_m"]-predicted):
                checked.append(dict(row, transfer_check="supported", held_out_prediction_m=predicted))
                continue
        checked.append(dict(row, distance_m=row["raw_distance_m"], correction_m=0.,
                            transfer_check="unconfirmed", proposed_correction_m=row["correction_m"]))
    return checked
