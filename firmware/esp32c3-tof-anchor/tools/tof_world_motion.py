"""Continuity of the surveyed rover body position in east/north/up metres."""
from datetime import datetime
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
        self.floor_z = None
        self.on_floor = False
        self.supported_z = None
        self.visual_frame = None
        self.origin_time = 0.
        self.last_write = None
        self.time = 0.
        self.source = 'surveyed body checkpoint'
        if self.path:
            try:
                survey = json.loads((self.path.parent / 'survey-world.json').read_text())
                ref = survey['reference']
                self.lever = np.array(ref['T_enu_camera'])[:3,3] - np.array(ref['T_enu_body'])[:3,3]
                self.camera_basis = np.asarray(ref['T_enu_camera'],dtype=float)[:3,:3]
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
                if saved.get('height_source') in ('surveyed_floor_contact', 'last_supported_world_height'):
                    self.supported_z = float(p[2])
                self.last_write = p.copy()
            except (OSError, ValueError, TypeError, KeyError, IndexError):
                return

    def observe_floor(self, nav, result):
        """Use the same survey datum and pickup-aware contact as the ranging solver."""
        baseline = result.get('baseline') or {}
        plane = result.get('ground_plane') or {}
        contact = result.get('ground_contact') or {}
        if (self.body is None or not baseline.get('frame_id') or
                baseline.get('world_alignment') != 'configured_east_north_up' or
                plane.get('frame_id') != baseline['frame_id'] or not plane.get('valid') or
                plane.get('axes') != 'east_north_up' or
                not isinstance(contact.get('active'), bool) or
                nav.world_id not in (None, self.world_id)):
            return
        elevation = plane.get('elevation_m')
        if not isinstance(elevation, (int, float)) or not np.isfinite(elevation):
            return
        self.floor_z = float(elevation)
        self.on_floor = contact['active']
        if self.on_floor:
            self.supported_z = self.floor_z
        elif self.supported_z is None:
            self.supported_z = self.body[2]
        self.body[2] = self.supported_z

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
            if self.supported_z is not None:
                self.body[2] = self.supported_z
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
            predicted = nav.alignment @ travel
            if self.on_floor:
                predicted[2] = self.floor_z - self.origin[2]
            return np.linalg.norm(delta-predicted) <= tolerance
        # Gravity gives vertical displacement even without absolute yaw.
        return (abs(delta[2]-(self.floor_z-self.origin[2] if self.on_floor else travel[2])) <= tolerance and
                np.linalg.norm(delta[:2]) <= np.linalg.norm(travel[:2])+tolerance)

    def correct(self, nav, camera, h, rms=.5, visual_body=None):
        if self.body is None or h is None or not self.supports(nav,camera,h):
            return
        travel = h[1]-self.local_origin
        if not np.any(travel):
            return
        measurement = camera-self.lever if visual_body is None else visual_body
        if self.aligned:
            predicted = self.origin + nav.alignment @ travel
            sigma = abs(h[2]-self.origin_sigma)
            gain = sigma**2/(sigma**2+max(.01,rms)**2)
            correction = (measurement-predicted)*gain
            length = np.linalg.norm(correction)
            if length > 3*sigma:
                correction *= (3*sigma)/length
            self.body = predicted + correction
        else:
            direction = (measurement-self.origin)[:2]
            length = np.linalg.norm(direction)
            if length == 0. and np.linalg.norm(travel[:2]) > 0.:
                return
            # Without yaw, fixes supply direction only within measured travel.
            self.body = self.origin.copy()
            if length:
                self.body[:2] += direction / length * np.linalg.norm(travel[:2])
            self.body[2] += travel[2]
        if self.supported_z is not None:
            if not self.on_floor:
                # An unbounded IMU integral can agree with a fix only because its
                # uncertainty grew. Commit at most the height change both support.
                lower, upper = sorted((self.supported_z, measurement[2]))
                self.supported_z = float(np.clip(self.body[2], lower, upper))
            self.body[2] = self.supported_z
        self.origin, self.local_origin, self.origin_sigma = self.body.copy(), h[1].copy(), h[2]
        self.source = 'AHRS-constrained ranging' if visual_body is None else 'AHRS-constrained visual tracking'
        self.origin_time = h[0]
        self.time = max(self.time, h[0])

    def visual(self, nav, meta, now):
        tof = meta.get('tof') or {}
        base, reg = tof.get('baseline') or {}, tof.get('registration') or {}
        camera, world = reg.get('current_camera') or {}, reg.get('surveyed_world') or {}
        frame = camera.get('frame_id')
        identity = (reg.get('map_id'), base.get('epoch'), frame)
        if (self.body is None or not frame or identity == self.visual_frame or not reg.get('valid')
                or not base.get('valid') or world.get('axes') != 'east_north_up'
                or world.get('world_id',self.world_id) != self.world_id or not getattr(nav,'surveyed',False)
                or reg.get('map_id') != meta.get('map_id') or not reg.get('map_id')
                or world.get('frame_id') != base.get('frame_id')
                or (base.get('frame_id'),base.get('epoch')) != nav.frame
                or (reg.get('frame_id'),reg.get('baseline_epoch'),reg.get('baseline_revision'))
                != (base.get('frame_id'),base.get('epoch'),base.get('revision'))):
            return
        try:
            age, collector = float(camera['age_s']), float(tof['collector_age_s'])
            if not 0 <= age <= 3 or not 0 <= collector <= 5:
                return
            received = datetime.fromisoformat(meta['receive_decode_timestamp'].replace('Z','+00:00')).timestamp()
            if not 0 <= now-received <= 3:
                return
            h = nav.at(received-age)
            point = np.asarray(camera['position_anchor_m'],dtype=float)
            rotation = np.asarray(camera['R_anchor_camera'],dtype=float)
            if (point.shape != (3,) or not np.isfinite(point).all() or rotation.shape != (3,3)
                    or not np.allclose(rotation.T @ rotation,np.eye(3),atol=.001)
                    or not np.isclose(np.linalg.det(rotation),1.,atol=.001)):
                return
            body = point - rotation @ self.camera_basis.T @ self.lever
        except (KeyError, TypeError, ValueError, OverflowError):
            return
        if h is not None:
            self.visual_frame = identity
            self.correct(nav,point,h,visual_body=body)

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
        if self.supported_z is not None:
            result['height_source'] = ('surveyed_floor_contact' if self.on_floor else
                                       'last_supported_world_height')
        if not np.array_equal(self.body,self.last_write):
            tmp = self.path.with_suffix('.tmp')
            tmp.write_text(json.dumps(result,allow_nan=False))
            os.replace(tmp,self.path)
            self.last_write = self.body.copy()
        return result
