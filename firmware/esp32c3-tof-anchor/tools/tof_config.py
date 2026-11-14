"""Runtime measured geometry. No module identities or room coordinates in code."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import tempfile

import numpy as np
from scipy.optimize import least_squares

SCHEMA = "rover.tof.geometry-config/1"

def _finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)

def validate(document):
    if not isinstance(document, dict) or document.get("schema") != SCHEMA:
        raise ValueError("unsupported geometry configuration schema")
    if document.get("axes") != "east_north_up":
        raise ValueError("axes must be east_north_up")
    units = document.get("units")
    if units not in ("in", "m"):
        raise ValueError("units must be in or m")
    if not isinstance(document.get("frame_id"), str) or not document["frame_id"].strip():
        raise ValueError("frame_id required")
    plane = document.get("ground_plane")
    if plane is not None:
        if not isinstance(plane, dict) or not _finite(plane.get("elevation")):
            raise ValueError("ground_plane requires a finite elevation in configuration units")
    contact = document.get("rover_ground_contact")
    if contact is not None:
        if plane is None or not isinstance(contact, dict) or not isinstance(contact.get("grounded"), bool):
            raise ValueError("rover_ground_contact requires a ground plane and boolean grounded")
        if not _finite(contact.get("confirmed_at")) or contact["confirmed_at"] < 0:
            raise ValueError("ground contact requires a Unix confirmation timestamp")
        if "radio_height" in contact and (not _finite(contact["radio_height"]) or contact["radio_height"] < 0):
            raise ValueError("radio_height must be a nonnegative offset above the floor")
    rows = document.get("anchors")
    if not isinstance(rows, list):
        raise ValueError("anchors must be a list")
    seen = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("anchor must be an object")
        address = row.get("bssid", "")
        if not isinstance(address, str) or not re.fullmatch(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", address):
            raise ValueError("anchor requires a BSSID")
        address = address.lower()
        if address in seen:
            raise ValueError("duplicate BSSID")
        seen.add(address)
        if "enabled" in row and not isinstance(row["enabled"], bool):
            raise ValueError("enabled must be boolean")
        p = row.get("position")
        if not isinstance(p, list) or len(p) != 3 or not all(
            isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in p):
            raise ValueError("position requires three finite numbers")
        if "label" in row and not isinstance(row["label"], str):
            raise ValueError("label must be text")
    return document

def strict_load(raw):
    return validate(json.loads(raw, parse_constant=lambda v: (_ for _ in ()).throw(ValueError("nonfinite number"))))

def digest(raw):
    return hashlib.sha256(raw).hexdigest()

def empty_document():
    return dict(schema=SCHEMA, frame_id="configured-enu", axes="east_north_up", units="m", anchors=[])

def read_configuration(path):
    try:
        raw = Path(path).read_bytes()
    except FileNotFoundError:
        return empty_document(), None
    return strict_load(raw), digest(raw)

def replace_configuration(path, document, expected_revision):
    """Compare-and-swap plus atomic replace; a complete document is the configuration."""
    import fcntl
    path = Path(path)
    validate(document)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.with_suffix(path.suffix + ".lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        _, current = read_configuration(path)
        if current != expected_revision:
            raise ValueError("geometry revision conflict; read current configuration")
        raw = (json.dumps(document, allow_nan=False, sort_keys=True, indent=2) + "\n").encode()
        fd, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=str(path.parent))
        try:
            with os.fdopen(fd, "wb") as stream:
                stream.write(raw); stream.flush(); os.fsync(stream.fileno())
            os.replace(temporary, path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
    return digest(raw)

def anchored_fit(fixed, active, edges):
    """Laterate observable unknown nodes; fixed coordinates never enter optimization."""
    points = {k: list(v) for k, v in fixed.items()}
    sigma = {k: 0. for k in fixed}
    conditions = [1.]
    pending = set(active) - set(points)
    while pending:
        added = []
        for node in sorted(pending):
            links = [e for e in edges if node in (e["a"], e["b"]) and
                     (e["b"] if e["a"] == node else e["a"]) in points]
            neighbors = [e["b"] if e["a"] == node else e["a"] for e in links]
            if len(neighbors) < 4:
                continue
            anchors = np.array([points[k] for k in neighbors], dtype=float)
            matrix = 2 * (anchors[1:] - anchors[0])
            s = np.linalg.svd(matrix, compute_uv=False)
            if len(s) < 3 or s[-1] <= max(1e-5, s[0] * 1e-4):
                continue
            ranges = np.array([e["distance_m"] for e in links])
            weights = np.array([e["sigma_m"] for e in links])
            rhs = ranges[0]**2-ranges[1:]**2+(anchors[1:]**2).sum(axis=1)-(anchors[0]**2).sum()
            initial = np.linalg.lstsq(matrix, rhs, rcond=None)[0]
            fit = least_squares(lambda p: (np.linalg.norm(anchors-p, axis=1)-ranges)/weights,
                                initial, loss="soft_l1", max_nfev=400)
            errors = np.linalg.norm(anchors-fit.x, axis=1)-ranges
            limits = np.maximum(.75, np.maximum(.12*ranges, 2.5*weights))
            jac = (fit.x-anchors)/np.maximum(np.linalg.norm(fit.x-anchors, axis=1)[:, None], 1e-12)
            singular = np.linalg.svd(jac, compute_uv=False)
            if not np.isfinite(fit.x).all() or singular[-1] <= max(1e-5, singular[0]*1e-4) or np.any(abs(errors)>limits):
                continue
            points[node] = fit.x.tolist()
            covariance = np.linalg.pinv(jac.T @ np.diag(1/weights**2) @ jac)
            sigma[node] = float(np.sqrt(max(0., np.trace(covariance))))
            conditions.append(float(singular[0]/singular[-1]))
            added.append(node)
        if not added:
            break
        pending -= set(added)
    selected = [dict(e, residual_m=float(np.linalg.norm(np.array(points[e["a"]])-points[e["b"]])-e["distance_m"]))
                for e in edges if e["a"] in points and e["b"] in points]
    return dict(positions=points, dimension=3, rank=3*len(set(points)-set(fixed)),
                condition_number=max(conditions), anchor_model_sigma_m=sigma, edges=selected,
                residual_rms_m=float(np.sqrt(np.mean([e["residual_m"]**2 for e in selected]))) if selected else 0.)

class RuntimeGeometry:
    def __init__(self, path):
        self.path = Path(path) if path else None
        self.revision = None
        self.document = None
        self.error = None
        self.active = False
        self.epoch = 0

    def update(self, baseline, now):
        if self.path is None:
            return None
        try:
            document, revision = read_configuration(self.path)
        except (OSError, ValueError, TypeError) as e:
            self.error = str(e)
            result = baseline.status(now)
            result.update(valid=False, status="geometry_configuration_error", positions=[],
                          geometry_configuration=dict(path=str(self.path), revision=self.revision, error=self.error))
            return result
        self.error = None
        enabled = [a for a in document["anchors"] if a.get("enabled", True)]
        changed = revision != self.revision
        was_configured = bool(baseline.fit and baseline.fit.get("configured_geometry"))
        if not enabled:
            if self.active or was_configured:
                baseline.fit = None
                baseline.recalibrating = False
                baseline.suspicions = {}
                baseline.last_fit_attempt = -1e30
                baseline.epoch += 1
                baseline.dirty = True
            self.active = False
            self.revision = revision
            self.document = document
            return None
        scale = .0254 if document["units"] == "in" else 1.
        fixed = {a["bssid"].lower(): [v*scale for v in a["position"]] for a in enabled}
        if changed or not self.active:
            baseline.epoch += 1
            self.epoch = baseline.epoch
            baseline.suspicions = {}
            baseline.recalibrating = False
            baseline.moved = []
        self.active = True
        self.revision = revision
        previous_fixed = None
        if self.document is not None:
            previous_scale = .0254 if self.document["units"] == "in" else 1.
            previous_fixed = {a["bssid"].lower(): [v*previous_scale for v in a["position"]]
                              for a in self.document["anchors"] if a.get("enabled", True)}
        elif baseline.fit and baseline.fit.get("configured_geometry"):
            previous_fixed = {k: baseline.fit["positions"][k] for k in baseline.fit.get("fixed_anchors", [])}
        if changed and previous_fixed is not None:
            moved = {k for k in set(previous_fixed) | set(fixed) if previous_fixed.get(k) != fixed.get(k)}
            for key in list(baseline.history):
                if set(key.split(">")) & moved:
                    del baseline.history[key]
            if moved:
                baseline.events.append(dict(type="configured_anchor_positions_changed", at=now, anchors=sorted(moved)))
                baseline.events = baseline.events[-16:]
        self.document = document
        edges = baseline._edges(now)
        active = sorted(k for k, seen in baseline.last_seen.items() if now-seen <= 30)
        fit = anchored_fit(fixed, active, edges)
        old = baseline.fit
        same_geometry = (old and old.get("configuration_revision") == revision and
                         old.get("positions") == fit["positions"])
        fit.update(configured_geometry=True, configuration_revision=revision,
                   fixed_anchors=sorted(fixed), frame_id=document["frame_id"] + ":" + revision,
                   epoch=baseline.epoch, created_at=now if changed or not old else old["created_at"],
                   revision=(old["revision"] if same_geometry else 1 if changed or not old else old.get("revision", 0)+1))
        baseline.fit = fit
        baseline.spatial_dimension = 3
        baseline.recalibrating = False
        baseline.suspicions = {}
        baseline.refinement = dict(revision=fit["revision"], accepted=0, last_check_at=now,
                                   last_update_at=now, state="configured_fixed_and_live_unknown")
        baseline.dirty = True
        baseline.save(now)
        result = baseline.status(now)
        rank = np.linalg.matrix_rank(np.array(list(fixed.values()))-np.mean(list(fixed.values()),axis=0), tol=1e-5)
        # Known points remain available even if the fixed geometry is not enough
        # to identify a unique three dimensional rover position.
        result.update(world_alignment="configured_east_north_up", fixed_anchors=sorted(fixed),
                      geometry_configuration=dict(path=str(self.path), revision=revision, error=None,
                        frame_id=document["frame_id"], axes=document["axes"], units=document["units"],
                        anchors=enabled, affine_rank=int(rank)),
                      positions=[dict(p, position_source="measured_fixed" if p["bssid"] in fixed else "range_estimated",
                                      label=next((a.get("label", "") for a in enabled if a["bssid"].lower()==p["bssid"]), ""))
                                 for p in result["positions"]])
        from tof_ground import radio_calibration, surveyed_floor
        plane = surveyed_floor(document, scale, fit["frame_id"], fixed)
        if plane is not None:
            result["ground_plane"] = plane
            result["radio_calibration"] = radio_calibration(baseline.history, fixed, now)
            contact = dict(document.get("rover_ground_contact") or {})
            if "radio_height" in contact:
                contact["radio_height_m"] = contact.pop("radio_height") * scale
            result["rover_ground_contact"] = contact
        return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--get", action="store_true")
    mode.add_argument("--replace", metavar="JSON_FILE")
    parser.add_argument("--expected-revision")
    args = parser.parse_args()
    path = Path(args.state)/"geometry-config.json"
    if args.get:
        document, revision = read_configuration(path)
    else:
        document = strict_load(Path(args.replace).read_bytes())
        revision = replace_configuration(path, document, args.expected_revision)
    print(json.dumps(dict(revision=revision, configuration=document), allow_nan=False))

if __name__ == "__main__":
    main()
