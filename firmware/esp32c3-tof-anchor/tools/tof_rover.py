"""Persistent, motion-segmented rover ranging statistics; no measurement journal.

Accumulate independent FTM sessions, not repeated polls or fitted coordinates.
Motion evidence invalidates the stationary segment. Precision is not RF accuracy.
"""
import json
import math
import os
import statistics
import time
from pathlib import Path

import numpy as np
from scipy.optimize import least_squares


class RoverRefinement:
    def __init__(self, path=None, clock=time.time):
        self.path = Path(path) if path else None
        self.clock = clock
        self.rows = {}
        self.seen = {}
        self.frame = None
        self.boot = None
        self.segment = 1
        self.total_sessions = 0
        self.last_motion = None
        self.reason = "startup"
        self.accel = None
        self.imu_identity = None
        self.last_imu = None
        self.visual = None
        self.last_visual = None
        self.dirty = False
        self.saved_at = 0.
        if self.path and self.path.exists():
            try:
                d = json.loads(self.path.read_text())
                if d.get("schema") == "rover.tof.refinement-state/1":
                    self.rows = d["rows"]
                    self.seen = d["seen"]
                    self.frame = d["frame"]
                    self.boot = d["boot"]
                    self.segment = d["segment"]
                    self.total_sessions = d["total_sessions"]
                    self.last_motion = d.get("last_motion")
                    self.reason = "restored"
            except (OSError, ValueError, KeyError, TypeError):
                self.reason = "state_unavailable"

    def invalidate(self, reason, now=None):
        now = self.clock() if now is None else now
        if self.rows:
            self.segment += 1
        self.rows = {}
        self.reason = reason
        self.last_motion = now
        self.dirty = True

    def observe_boot(self, boot):
        if boot is not None and self.boot is not None and boot != self.boot:
            self.invalidate("camera_reboot")
            self.seen = {}
            self.accel = None
            self.imu_identity = None
        if boot is not None:
            self.boot = boot

    def observe_motion(self, ahrs, now=None):
        now = self.clock() if now is None else now
        imu = ahrs.get("imu", {}) if isinstance(ahrs, dict) else {}
        if not imu.get("valid") or not isinstance(imu.get("age_ms"), (int, float)) or not 0 <= imu["age_ms"] <= 500:
            return False
        identity = imu.get("sample_count", imu.get("observed_uptime_ms"))
        if identity is None or identity == self.imu_identity:
            return False
        self.imu_identity = identity
        accel = imu.get("accel_m_s2")
        gyro = imu.get("gyro_rad_s")
        if not all(isinstance(v, list) and len(v) == 3 and all(isinstance(x, (int, float)) and math.isfinite(x) for x in v) for v in [accel, gyro]):
            return False
        self.last_imu = now - imu["age_ms"] / 1000.
        a = np.array(accel)
        rotating = np.linalg.norm(gyro) > .06
        accelerating = self.accel is not None and np.linalg.norm(a - self.accel) > .45
        moving = bool(rotating or accelerating)
        # Follow actual current gravity orientation even after a pickup/turn.
        self.accel = a if moving or self.accel is None else .9 * self.accel + .1 * a
        if moving:
            self.invalidate("ahrs_motion", now)
        return moving

    def observe_visual(self, evidence, now=None):
        """Consume fresh, deduplicated camera pose displacement from the mapper."""
        now = self.clock() if now is None else now
        if not isinstance(evidence, dict) or evidence.get("tracking") is not True:
            return False
        at = evidence.get("observed_at")
        p = evidence.get("position")
        frame = evidence.get("map_id")
        if not isinstance(at, (int, float)) or not 0 <= now-at <= 3 or not isinstance(p, list) or len(p) != 3 or not all(isinstance(v, (int, float)) and math.isfinite(v) for v in p):
            return False
        prev = self.visual
        if prev and at <= prev["observed_at"]:
            return False
        self.last_visual = at
        self.visual = dict(evidence)
        if not prev or prev.get("map_id") != frame or not 0 < at-prev["observed_at"] <= 3:
            return False
        # Visual-unit motion still invalidates a stationary assumption; it does
        # not assert a metric displacement when metric scale is unavailable.
        scale = .03 if evidence.get("metric") else .02
        moving = math.dist(p, prev["position"]) > scale
        if moving:
            self.invalidate("visual_motion", now)
        return moving

    def observe(self, target, distance, observed, now=None):
        now = self.clock() if now is None else now
        if not isinstance(distance, (int, float)) or not math.isfinite(distance) or distance <= 0 or not isinstance(observed, (int, float)) or not 0 <= now-observed <= 30:
            return False
        # The firmware reports age_ms, not a session sequence. Re-polling the
        # same completion reproduces its observation time within clock jitter.
        if target in self.seen and observed <= self.seen[target] + .75:
            return False
        if target not in self.rows and len(self.rows) >= 12:
            return False
        self.seen[target] = observed
        self.total_sessions += 1
        self.dirty = True
        if self.last_motion is not None and now-self.last_motion < 2:
            return True
        r = self.rows.setdefault(target, {"n": 0, "mean": 0., "m2": 0., "first": observed, "last": observed, "rejected": 0, "recent": []})
        r["last"] = observed
        r["recent"] = (r["recent"] + [[observed, float(distance)]])[-9:]
        sigma = math.sqrt(max(0., r["m2"] / max(1, r["n"]-1)))
        if r["n"] >= 8 and abs(distance-r["mean"]) > max(1.5, 4*sigma):
            r["rejected"] += 1
        elif r["n"] < 5:
            # Robust bootstrap: one initial bad RTT must not anchor the mean.
            xs = [v for _, v in r["recent"]]
            center = statistics.median(xs)
            xs = [v for v in xs if abs(v-center) <= max(1.5, 3*statistics.median(abs(v-center) for v in xs))]
            r["n"] = len(xs)
            r["mean"] = sum(xs)/len(xs)
            r["m2"] = sum((v-r["mean"])**2 for v in xs)
        else:
            r["n"] += 1
            delta = distance-r["mean"]
            r["mean"] += delta/r["n"]
            r["m2"] += delta*(distance-r["mean"])
        self._range_change(now)
        return True

    def _range_change(self, now):
        changed = []
        for key, r in self.rows.items():
            recent = [(t, v) for t, v in r["recent"][-5:] if now-t <= 75]
            if r["n"] < 10 or len(recent) < 5:
                continue
            sigma = math.sqrt(max(0., r["m2"]/max(1, r["n"]-1)))
            delta = statistics.median(v for _, v in recent)-r["mean"]
            if abs(delta) > max(.75, 2.5*sigma/math.sqrt(5)):
                changed.append(key)
        if len(changed) >= 2:
            # Multiple independently updated links changed persistently. This
            # could be rover motion or RF-environment change: reacquire either.
            self.invalidate("sustained_range_change", now)

    def snapshot(self, baseline, now=None):
        now = self.clock() if now is None else now
        frame = [baseline.get("frame_id"), baseline.get("epoch")]
        if self.frame is not None and frame != self.frame:
            self.invalidate("anchor_frame_changed", now)
        self.frame = frame
        rows = [{"target_bssid": k, "distance_m": r["mean"], "session_count": r["n"], "std_m": math.sqrt(max(0., r["m2"]/max(1,r["n"]-1))), "age_s": now-r["last"], "observed": r["last"]} for k, r in sorted(self.rows.items()) if r["n"] and 0 <= now-r["last"] <= 30]
        motion = self.last_motion is not None and now-self.last_motion < 2
        quiet_imu = self.last_imu is not None and 0 <= now-self.last_imu <= 3
        quiet_visual = self.last_visual is not None and 0 <= now-self.last_visual <= 3
        out = {"schema": "rover.tof.refinement/1", "state": "moving" if motion else "accumulating", "segment": self.segment, "total_sessions": self.total_sessions, "session_count": sum(r["session_count"] for r in rows), "anchor_count": len(rows), "stationary_evidence": "ahrs_and_ranges" if quiet_imu else "visual_and_ranges" if quiet_visual else "ranges_only", "reason": self.reason, "estimate": None, "ranges": rows, "navigation_authority": False, "uncertainty": "Repeated-session precision excludes systematic FTM bias and anchor-model error"}
        if motion or not baseline.get("valid") or baseline.get("dimension") != 3:
            self.save(now)
            return out
        points = {p["bssid"]: [p["x_m"],p["y_m"],p["z_m"]] for p in baseline.get("positions", [])}
        usable = [r for r in rows if r["target_bssid"] in points]
        if len(usable) >= 4:
            anchors = np.array([points[r["target_bssid"]] for r in usable], dtype=float)
            distances = np.array([r["distance_m"] for r in usable])
            a = 2*(anchors[1:]-anchors[0])
            if np.linalg.matrix_rank(a, tol=1e-5) == 3:
                b = distances[0]**2-distances[1:]**2+(anchors[1:]**2).sum(axis=1)-(anchors[0]**2).sum()
                initial = np.linalg.lstsq(a, b, rcond=None)[0]
                fit = least_squares(lambda p:np.linalg.norm(anchors-p,axis=1)-distances,initial,loss="soft_l1",f_scale=.5,max_nfev=100)
                residual = np.linalg.norm(anchors-fit.x,axis=1)-distances
                rms = float(np.sqrt(np.mean(residual**2)))
                out["estimate"] = {"x_m": float(fit.x[0]), "y_m": float(fit.x[1]), "z_m": float(fit.x[2]), "residual_rms_m": rms, "frame_id": baseline["frame_id"], "baseline_epoch": baseline["epoch"], "baseline_revision": baseline["revision"], "approximate": True}
                mature = min(r["session_count"] for r in usable) >= 5
                out["state"] = "stationary_refining" if mature and (quiet_imu or quiet_visual) else "refining"
                out["minimum_sessions_per_anchor"] = min(r["session_count"] for r in usable)
                out["residual_rms_m"] = rms
        self.save(now)
        return out

    def save(self, now=None, force=False):
        now = self.clock() if now is None else now
        if not self.path or not self.dirty or (not force and now-self.saved_at < 5):
            return
        d = {"schema": "rover.tof.refinement-state/1", "rows": self.rows, "seen": self.seen, "frame": self.frame, "boot": self.boot, "segment": self.segment, "total_sessions": self.total_sessions, "last_motion": self.last_motion}
        self.path.parent.mkdir(parents=True,exist_ok=True)
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(d,allow_nan=False))
        os.replace(tmp,self.path)
        self.saved_at = now
        self.dirty = False
