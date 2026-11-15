"""Owner-confirmed survey pose and gravity alignment shared by world-map consumers."""
import json
import math
import os
from collections import deque
from datetime import datetime
from pathlib import Path
import socketserver
import tempfile
import threading
import time
import uuid

import numpy as np
from scipy.spatial.transform import Rotation

from tof_lidar_floor import fit_floor


def atomic_json(path, value):
    with tempfile.NamedTemporaryFile(mode='w', dir=path.parent, delete=False) as f:
        try:
            json.dump(value, f, allow_nan=False)
            f.flush()
            os.fsync(f.fileno())
        except Exception:
            os.unlink(f.name)
            raise
    os.replace(f.name, path)


def quaternion(value):
    q = np.asarray(value, dtype=float)
    if q.shape != (4,) or not np.isfinite(q).all() or not .9 < np.linalg.norm(q) < 1.1:
        return None
    return Rotation.from_quat(q[[1,2,3,0]]).as_matrix()


def proper(value):
    m = np.asarray(value, dtype=float)
    return m if (m.shape == (3,3) and np.allclose(m.T@m,np.eye(3),atol=.003)
                 and abs(np.linalg.det(m)-1) < .003) else None


def tilt(n):
    return np.array([math.asin(np.clip(-n[0], -1, 1)), math.atan2(n[1], n[2])])


class SurveyPose:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.path = self.directory/'survey-pose-zero.json'
        self.reference = self.sample = self.rotation = None
        self.floor = {}
        self.scan_keys = {}
        self.tilt_bias = np.zeros(2)
        self.floor_updates = 0
        self.floor_sources = []
        self.last_floor = None
        self.attitudes = deque(maxlen=180)
        survey = json.loads((self.directory/'survey-world.json').read_text())
        self.world_id = survey['world_id']
        ref = survey['reference']
        self.body = np.asarray(ref['T_enu_body'], dtype=float)
        self.imu = np.asarray(ref['T_enu_imu'], dtype=float)
        self.camera_to_body = np.linalg.inv(self.body) @ np.asarray(ref['T_enu_camera'])
        self.imu_to_body = self.imu[:3,:3].T @ self.body[:3,:3]
        if self.path.exists():
            r = json.loads(self.path.read_text())
            if r['world_id'] == self.world_id:
                self.reference = r

    def corrected_base(self, sample):
        if not self.reference:
            return None
        ref = self.reference
        if sample['identity'] == ref['identity']:
            return self.imu[:3,:3] @ np.asarray(ref['gyro']).T @ sample['gyro'] @ self.imu_to_body
        if (sample['persistent'] is not None and ref.get('persistent') is not None and
                sample['profile'] and sample['profile'] == ref.get('profile')):
            return self.imu[:3,:3] @ np.asarray(ref['persistent']).T @ sample['persistent'] @ self.imu_to_body
        return None

    def observe(self, meta, now, nav):
        try:
            received = datetime.fromisoformat(meta['receive_decode_timestamp'].replace('Z','+00:00')).timestamp()
        except (KeyError, ValueError, TypeError):
            return
        if not 0 <= now-received < 2 or meta.get('camera_id') == 'rear':
            return
        env = meta.get('ahrs') or {}
        a = (env.get('ahrs') or {}).get('attitude') or {}
        gyro = quaternion(a.get('quaternion_wxyz'))
        if env.get('timing_qualified') and a.get('valid') and gyro is not None and 0 <= a.get('age_ms',9999) <= 500:
            at = received - max(0,(env.get('snapshot') or {}).get('elapsed_since_receive_s',0)) - a['age_ms']/1000
            if self.sample is None or at > self.sample['at']:
                sample = dict(at=at, gyro=gyro, identity=[env.get('receiver_id'),env.get('boot_id'),
                              a.get('generation'),a.get('yaw_origin_uptime_ms')],
                              persistent=proper(a.get('world_rotation_enu_imu')) if a.get('world_alignment_valid')
                              and a.get('world_id') == self.world_id else None, profile=a.get('world_reference_id'))
                self.sample = sample
                base = self.corrected_base(sample)
                if base is not None:
                    self.attitudes.append((at,base))
        self.observe_scans(meta, received, now)
        self.update_rotation(nav, now)

    def observe_scans(self, meta, received, now):
        fusion = meta.get('lidar_fusion') or meta.get('lidar') or {}
        scan = fusion.get('latest') or fusion
        rows = scan.get('sensors') or []
        if not isinstance(rows,list):
            return
        stamp = scan.get('received_monotonic')
        if not isinstance(stamp,(int,float)):
            return
        receipt_age = time.monotonic()-stamp
        if not 0 <= receipt_age <= 2:
            return
        updated = False
        for row in rows:
            sid = row.get('sensor_id') or row.get('id')
            if sid not in ('front','rear'):
                continue
            raw = row.get('raw') or row
            key = (scan.get('receiver_id'),scan.get('boot_id'),raw.get('observed_monotonic_us'),raw.get('sequence'))
            if self.scan_keys.get(sid) == key:
                continue
            self.scan_keys[sid] = key
            at = now-receipt_age-raw.get('age_ms',0)/1000
            mount = row.get('mount') or (fusion.get('configuration') or {}).get('lidar',{}).get(sid)
            self.floor.pop(sid,None)
            if not mount or not 0 <= now-at <= 2:
                continue
            fit = fit_floor(row,mount,self.camera_to_body)
            if fit is not None:
                self.floor[sid] = (at,fit)
                updated = True
        if not updated or not self.attitudes:
            return
        # Each LiDAR votes against the gyro at its own measurement time.
        innovations = []
        for sid,(at,fit) in self.floor.items():
            if now-at > 2:
                continue
            gyro_time, rotation = min(self.attitudes, key=lambda a: abs(a[0]-at))
            if abs(gyro_time-at) > .25:
                continue
            plane_tilt = tilt(fit['normal'])
            mounting = self.reference.get('floor_tilt_at_zero',{}).get(sid,[0,0])
            innovations.append((sid,plane_tilt-mounting-tilt(rotation[2])))
        if not innovations:
            return
        if len(innovations)>1 and np.linalg.norm(innovations[0][1]-innovations[1][1]) > math.radians(12):
            return
        dt = .1 if self.last_floor is None else max(0,min(1.,now-self.last_floor))
        gain = 1-math.exp(-dt/1.2)
        self.tilt_bias += gain*(np.mean([x[1] for x in innovations],axis=0)-self.tilt_bias)
        self.floor_sources = [x[0] for x in innovations]
        self.last_floor = now
        self.floor_updates += 1

    def update_rotation(self, nav, now):
        if not self.sample or now-self.sample['at'] > 2:
            return
        base = self.corrected_base(self.sample)
        if base is None:
            return
        yaw = math.atan2(base[1,0], base[0,0])
        pitch, roll = tilt(base[2]) + self.tilt_bias
        self.rotation = Rotation.from_euler('ZYX',[yaw,pitch,roll]).as_matrix()
        nav.alignment = self.rotation @ self.imu_to_body.T @ self.sample['gyro'].T
        nav.world_id = self.world_id

    def zero(self, nav, world_id, request_id, now):
        if world_id != self.world_id:
            raise ValueError('Survey world has changed')
        uuid.UUID(request_id)
        if self.reference and self.reference['id'] == request_id:
            return self.receipt()
        if self.reference and request_id in self.reference.get('earlier_ids',[]):
            raise ValueError('This zero request was superseded')
        sample = self.sample
        if not sample or not 0 <= now-sample['at'] <= 1:
            raise ValueError('Waiting for a fresh front IMU attitude')
        earlier = ([self.reference['id']]+self.reference.get('earlier_ids',[])) if self.reference else []
        record = dict(id=request_id, earlier_ids=earlier[:32], world_id=world_id, at=now,
                      position_enu_m=self.body[:3,3].tolist(), identity=sample['identity'], gyro=sample['gyro'].tolist(),
                      persistent=None if sample['persistent'] is None else sample['persistent'].tolist(),
                      profile=sample['profile'],floor_tilt_at_zero={sid:tilt(fit['normal']).tolist()
                      for sid,(at,fit) in self.floor.items() if 0 <= now-at <= 1})
        atomic_json(self.path,record)
        self.reference=record
        self.apply_zero(nav)
        self.update_rotation(nav,now)
        return self.receipt()

    def apply_zero(self, nav):
        # An interrupted zero is recoverable from this one durable reference.
        nav.reset_motion()
        world = nav.world_body
        world.segment = None
        world.body = self.body[:3,3].copy()
        world.world_id = self.world_id
        world.floor_z = world.supported_z = float(world.body[2])
        world.on_floor = True
        world.time = self.reference['at']
        world.source = 'owner-confirmed surveyed body checkpoint'
        world.last_write = None
        world.display(nav,None)
        self.attitudes.clear()
        self.tilt_bias[:] = 0
        self.last_floor = None
        self.floor_updates = 0
        self.floor_sources = []
        self.rotation = self.body[:3,:3].copy()

    def receipt(self):
        return dict(ok=True, world_id=self.world_id, reset_id=self.reference['id'], observed_at=self.reference['at'],
                    position_enu_m=self.reference['position_enu_m'], rotation_enu_body=self.body[:3,:3].tolist())

    def fields(self, now):
        if self.rotation is None or self.reference is None:
            return {}
        return dict(rotation_enu_body=self.rotation.tolist(), survey_zero_id=self.reference['id'],
                    orientation_source='survey-zeroed AHRS', attitude_observed_at=self.sample['at'] if self.sample else self.reference['at'],
                    lidar_floor=dict(sources=self.floor_sources, updates=self.floor_updates,
                                     observed_at=self.last_floor, fresh=self.last_floor is not None and now-self.last_floor<=2,
                                     fits={k:dict(inliers=v['inliers'],residual_m=v['residual_m'],age_s=max(0,now-at))
                                           for k,(at,v) in self.floor.items() if now-at<=2}))


def start_survey_control(nav):
    if not nav.directory or not (nav.directory/'survey-world.json').is_file():
        return None
    pose = SurveyPose(nav.directory)
    if pose.reference and (nav.world_body.body is None or nav.world_body.time < pose.reference['at']):
        pose.apply_zero(nav)
    class Reset(socketserver.StreamRequestHandler):
        def handle(self):
            try:
                self.request.settimeout(2)
                data=json.loads(self.rfile.readline(2048))
                with nav.lock:
                    reply=pose.zero(nav,data['world_id'],data['request_id'],time.time())
            except (ValueError,KeyError,TypeError,OSError) as exc:
                reply=dict(ok=False,error=str(exc))
            self.wfile.write(json.dumps(reply,allow_nan=False).encode()+b'\n')
    path=nav.directory/'survey-pose.sock'
    path.unlink(missing_ok=True)
    server=socketserver.UnixStreamServer(str(path),Reset)
    os.chmod(path,0o600)
    threading.Thread(target=server.serve_forever,name='survey-pose-control',daemon=True).start()
    return pose
