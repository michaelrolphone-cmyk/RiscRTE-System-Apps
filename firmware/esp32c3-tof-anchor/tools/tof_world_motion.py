"""Continuity of the surveyed rover body position in east/north/up metres."""
import json
import os
from pathlib import Path

import numpy as np


class WorldBodyPose:
    def __init__(self, directory):
        self.path = Path(directory) / 'rover-world-body-pose.json' if directory else None
        self.body = self.world_id = self.lever = None
        self.segment = self.origin = self.local_origin = None
        self.aligned = False
        self.origin_time = 0.
        self.last_write = None
        self.time = 0.
        self.source = 'surveyed body checkpoint'
        if self.path:
            try:
                survey = json.loads((self.path.parent / 'survey-world.json').read_text())
                ref = survey['reference']
                self.lever = np.array(ref['T_enu_camera'])[:3,3] - np.array(ref['T_enu_body'])[:3,3]
                self.world_id = survey['world_id']
                saved = json.loads(self.path.read_text())
                if saved['world_id'] != survey['world_id']:
                    return
                p = np.asarray(saved['position_enu_m'], dtype=float)
                if p.shape != (3,) or not np.isfinite(p).all():
                    return
                self.time = float(saved['observed_at'])
                self.source = saved['source']
                self.body, self.world_id = p, survey['world_id']
                self.last_write = p.copy()
            except (OSError, ValueError, TypeError, KeyError, IndexError):
                return

    def track(self, nav, h):
        if self.body is None or h is None:
            return
        aligned = nav.alignment is not None and nav.world_id == self.world_id
        if self.segment != nav.segment or aligned != self.aligned:
            # No integration across a gap, boot or restart.
            self.segment, self.aligned = nav.segment, aligned
            self.origin_time = h[0]
            self.origin = self.body.copy()
            self.local_origin = h[1].copy()
            self.origin_sigma = h[2]
        if h[0] < self.time:
            return
        if aligned:
            self.body = self.origin + nav.alignment @ (h[1] - self.local_origin)
            self.source = 'AHRS displacement from accepted world pose'
        self.time = h[0]

    def supports(self, nav, camera, h):
        if self.body is None:
            return True
        if h is None or self.segment != nav.segment or self.local_origin is None or h[0] < self.origin_time:
            return False
        travel = h[1] - self.local_origin
        # A rotation alone can move the antenna by at most twice its lever arm.
        tolerance = 2*np.linalg.norm(self.lever) + 3*abs(h[2]-self.origin_sigma)
        delta = camera - (self.origin+self.lever)
        if nav.alignment is not None and nav.world_id == self.world_id:
            return np.linalg.norm(delta-nav.alignment@travel) <= tolerance
        # Gravity gives vertical displacement even without absolute yaw.
        return (abs(delta[2]-travel[2]) <= tolerance and
                np.linalg.norm(delta[:2]) <= np.linalg.norm(travel[:2])+tolerance)

    def correct(self, nav, camera, h):
        if self.body is None or h is None or not self.supports(nav,camera,h):
            return
        if self.aligned:
            return
        travel = h[1]-self.local_origin
        direction = (camera-self.lever-self.origin)[:2]
        length = np.linalg.norm(direction)
        if length == 0. or not np.any(travel):
            return
        # Radio can resolve missing yaw, but cannot enlarge IMU displacement.
        self.body = self.origin.copy()
        self.body[:2] += direction / length * np.linalg.norm(travel[:2])
        self.body[2] += travel[2]
        self.origin, self.local_origin, self.origin_sigma = self.body.copy(), h[1].copy(), h[2]
        self.source = 'AHRS-constrained ranging'
        self.origin_time = h[0]
        self.time = max(self.time, h[0])

    def display(self, nav, h):
        if self.body is None:
            if (h is None or self.world_id is None or nav.world_id != self.world_id
                    or nav.alignment is None or nav.offset is None or not nav.surveyed):
                return None
            self.body = nav.offset + nav.alignment @ h[1] - self.lever
            self.source = 'accepted world-aligned ranging'
        self.track(nav,h)
        result = dict(schema='rover.world-body-pose/1', world_id=self.world_id,
                      position_enu_m=self.body.tolist(), observed_at=self.time,
                      source=self.source, held=bool(h is None or h[3] or (not self.aligned and self.origin_time != h[0])))
        if not np.array_equal(self.body,self.last_write):
            tmp = self.path.with_suffix('.tmp')
            tmp.write_text(json.dumps(result,allow_nan=False))
            os.replace(tmp,self.path)
            self.last_write = self.body.copy()
        return result
