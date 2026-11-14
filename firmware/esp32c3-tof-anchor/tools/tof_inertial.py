"""Capture-clock inertial odometry and pre-average radio innovation checks."""
from collections import deque
from datetime import datetime
import http.client
import json
import math
from pathlib import Path
import socket
import struct
import threading
import time

import numpy as np
from scipy.spatial.transform import Rotation


def vector(value, size):
    try:
        a = np.asarray(value, dtype=float)
        return a if a.shape == (size,) and np.isfinite(a).all() else None
    except (TypeError, ValueError):
        return None


def rotation(q):
    q = vector(q, 4)
    if q is None or not .9 < np.linalg.norm(q) < 1.1:
        return None
    return Rotation.from_quat(q[[1, 2, 3, 0]]).as_matrix()


def point(p):
    return None if not isinstance(p, dict) else vector([p.get(k) for k in ('x_m','y_m','z_m')], 3)


class InertialNavigator:
    """Integrate IMU specific force. Never integrate across an unobserved interval."""
    def __init__(self, vision_dir=None):
        self.directory = Path(vision_dir) if vision_dir else None
        self.lock = threading.RLock()
        self.identity = self.frame = None
        self.alignment = None
        self.world_id = None
        self.reference_source = self.reference_id = self.heading_quality = None
        self.history = deque()
        self.links = {}
        self.pending = {}
        self.relative_fix = None
        self.offset = None
        self.absolute_sigma = None
        self.absolute_source = None
        self.last_fix_sessions = {}
        self.seen = {}
        self.rejected_ranges = self.rejected_fixes = self.accepted_fixes = 0
        self.last_decision = None
        self.last_rejection = None
        self.samples = self.gaps = 0
        self.feed_error = None
        self.segment = 0
        self.ref_check = float('-inf')
        self.reset_motion()

    def reset_motion(self):
        self.segment += 1
        self.p = np.zeros(3)
        self.v = np.zeros(3)
        self.a = None
        self.previous_us = None
        self.gravity = None
        self.stationary = False
        self.rest_observed = False
        self.quiet_start = None
        self.rest_gate = dict(imu_quiet=False, quiet_duration_s=0., velocity_consistent=False, evidence='imu_quiet_assumed_zero_velocity', constant_velocity_observable=False)
        self.quiet_velocity_consistent = False
        self.bias = np.zeros(3)
        self.trav_sigma = self.vel_sigma = 0.
        self.history.clear()
        self.links.clear()
        self.pending.clear()
        self.relative_fix = None
        self.offset = self.absolute_sigma = self.absolute_source = None
        self.last_fix_sessions.clear()

    def configure(self, baseline):
        key = (baseline.get('frame_id'), baseline.get('epoch'))
        if self.frame != key:
            self.offset = self.absolute_sigma = self.absolute_source = None
            self.links.clear()
            self.relative_fix = None
            self.pending.clear()
            self.seen.clear()
            self.last_fix_sessions.clear()
            self.frame = key
        self.surveyed = baseline.get('world_alignment') == 'configured_east_north_up'

    def reference(self, boot, receiver, generation, yaw_origin):
        if not self.directory or not isinstance(boot, str) or not boot.isalnum() or not isinstance(generation, int):
            return
        names = [f'attitude-world-reference-{boot}-g{generation}.json','attitude-world-reference.json']
        for name in names:
            try:
                r = json.loads((self.directory/name).read_text())
                att = r['attitude']
                if (r['boot_id'], r['receiver_id'], att['generation'], att.get('yaw_origin_uptime_ms')) != (boot,receiver,generation,yaw_origin):
                    continue
                survey = json.loads((self.directory/'survey-world.json').read_text())
                if r['world_id'] != survey['world_id']:
                    continue
                imu = np.asarray(r['reference']['T_enu_imu'], dtype=float)[:3,:3]
                orientation = rotation(att['quaternion_wxyz'])
                if orientation is None or not np.allclose(imu.T @ imu, np.eye(3), atol=.001):
                    continue
                self.world_id = r['world_id']
                self.alignment = imu @ orientation.T
                self.reference_source = 'boot_matched_reference_file'
                self.reference_id = name
                return
            except (OSError, KeyError, IndexError, TypeError, ValueError):
                continue

    def persistent_reference(self, attitude):
        """Recover the current integration-frame alignment from the persisted firmware reference.

        A reboot changes the gyro integration origin, not the saved world identity.
        Use rotations associated with the same capture; never zero the live pose.
        """
        if not self.directory or not (attitude.get('valid') is True
                and attitude.get('world_alignment_valid') is True
                and attitude.get('world_reference_persistent') is True
                and isinstance(attitude.get('world_reference_id'), str)
                and attitude['world_reference_id']):
            return False
        try:
            survey = json.loads((self.directory/'survey-world.json').read_text())
            if attitude.get('world_id') != survey['world_id']:
                return False
            world = np.asarray(attitude['world_rotation_enu_imu'], dtype=float)
            current = rotation(attitude.get('quaternion_wxyz'))
            if (world.shape != (3,3) or not np.isfinite(world).all() or current is None
                    or not np.allclose(world.T @ world, np.eye(3), atol=.001)
                    or not np.isclose(np.linalg.det(world), 1., atol=.001)):
                return False
            self.alignment = world @ current.T
            self.world_id = survey['world_id']
            self.reference_source = 'persistent_firmware_world_rotation'
            self.reference_id = attitude['world_reference_id']
            self.heading_quality = attitude.get('heading_quality')
            return True
        except (OSError, KeyError, TypeError, ValueError):
            return False

    def ingest(self, metadata, now):
        push = metadata.get('sensor_delivery') == 'websocket_push'
        if push:
            self.last_push_receive = now
        elif now-getattr(self, 'last_push_receive', float('-inf')) < 2:
            return
        envelope = metadata.get('ahrs') or {}
        if not envelope.get('timing_qualified'):
            return
        raw = envelope.get('ahrs') or {}
        attitude = raw.get('attitude') or {}
        boot, receiver = envelope.get('boot_id'), envelope.get('receiver_id')
        generation, origin = attitude.get('generation'), attitude.get('yaw_origin_uptime_ms')
        capture = envelope.get('capture_monotonic_us')
        elapsed = (envelope.get('snapshot') or {}).get('elapsed_since_receive_s')
        timestamp = metadata.get('receive_decode_timestamp')
        if not (boot and receiver and isinstance(capture, int) and isinstance(elapsed, (int,float)) and timestamp):
            return
        # elapsed_since_receive_s bounds transport age at serialization; use the
        # decode time to avoid stamping a delayed packet as a new observation.
        received = datetime.fromisoformat(timestamp.replace('Z','+00:00')).timestamp()
        if not 0 <= now-received < 2 or not 0 <= elapsed < 1:
            return
        epoch = received - elapsed
        identity = boot, receiver, generation, origin
        with self.lock:
            if self.identity != identity:
                if self.identity and received <= getattr(self, 'last_received', 0):
                    return
                self.reset_motion()
                self.alignment = self.world_id = None
                self.reference_source = self.reference_id = self.heading_quality = None
                self.ref_check = float('-inf')
                self.identity = identity
            self.last_received = received
            if self.alignment is None:
                self.persistent_reference(attitude)
            if self.alignment is None and now-self.ref_check > 2:
                self.ref_check = now
                self.reference(*identity)
            for s in raw.get('imu_samples', []):
                us = s.get('monotonic_us')
                if not isinstance(us, int) or not 0 <= capture-us < max(1500000, int(raw.get('imu_history_window_ms',1500))*1000) or s.get('generation') != generation:
                    continue
                age = (capture-us)/1e6
                self.sample(s, epoch-age)

    def sample(self, s, wall, sonar=None):
        us = s['monotonic_us']
        if self.previous_us is not None and us <= self.previous_us:
            return
        q = rotation(s.get('quaternion_wxyz'))
        accel, gyro = vector(s.get('accel_m_s2'), 3), vector(s.get('gyro_rad_s'), 3)
        if not (s.get('valid') and s.get('attitude_valid')) or q is None or accel is None or gyro is None:
            return
        dt = 0. if self.previous_us is None else (us-self.previous_us)/1e6
        if dt > .15:
            self.gaps += 1
            self.reset_motion()
            dt = 0.
        self.previous_us = us
        self.samples += 1
        force = q @ accel
        quiet = np.linalg.norm(gyro) < .07 and abs(np.linalg.norm(force)-9.80665) < .45
        if self.gravity is None:
            if not quiet:
                return
            self.gravity = np.array([0.,0.,9.80665])
        linear = force-self.gravity-self.bias
        quiet = quiet and np.linalg.norm(linear) < .35
        if not quiet:
            self.quiet_start = None
            self.quiet_velocity_consistent = False
        elif self.quiet_start is None:
            self.quiet_start = us
            # Quiet IMU cannot observe constant velocity. Do not erase an
            # already integrated velocity merely because acceleration ceases.
            self.quiet_velocity_consistent = (not self.rest_observed or
                np.linalg.norm(self.v) <= max(.025, self.vel_sigma))
        self.rest_gate = dict(imu_quiet=bool(quiet),
            quiet_duration_s=0. if self.quiet_start is None else (us-self.quiet_start)/1e6,
            velocity_consistent=bool(self.quiet_velocity_consistent),
            evidence='imu_quiet_assumed_zero_velocity', constant_velocity_observable=False)
        self.stationary = bool(quiet and self.quiet_start is not None and
            us-self.quiet_start >= 800000 and self.quiet_velocity_consistent)
        if self.stationary:
            self.v[:] = 0.
            self.a = np.zeros(3)
            self.vel_sigma = .025
            self.bias += (1-math.exp(-dt/2.))*linear
        else:
            mean = linear if self.a is None else (self.a+linear)*.5
            self.p += self.v*dt + .5*mean*dt*dt
            self.v += mean*dt
            self.a = linear
            self.trav_sigma += self.vel_sigma*dt + .5*.35*dt*dt
            self.vel_sigma += .35*dt
        # Initialize relative velocity from the IMU quiet-window assumption.
        # It is not an independent observation of zero speed; expose that
        # limitation, and never require sonar to initialize navigation.
        if self.stationary:
            self.rest_observed = True
        if self.rest_observed:
            if self.history:
                wall = max(wall, self.history[-1][0]+1e-6)
            self.history.append((wall,self.p.copy(),self.trav_sigma,self.stationary))
            while self.history and wall-self.history[0][0] > 35:
                self.history.popleft()

    def at(self, wall):
        if not self.history or wall < self.history[0][0]-.12 or wall > self.history[-1][0]+.45:
            return None
        return min(reversed(self.history), key=lambda x:abs(x[0]-wall))

    def display(self, now):
        h = self.at(now)
        out = dict(schema='rover.inertial-navigation/1', state='awaiting_stationary_reference' if not self.history else 'stale',
                   frame_id=self.frame[0] if self.frame else None, baseline_epoch=self.frame[1] if self.frame else None,
                   world_id=self.world_id, imu_samples=self.samples, segment=self.segment, gaps=self.gaps,
                   accepted_fixes=self.accepted_fixes,rejected_fixes=self.rejected_fixes,rejected_ranges=self.rejected_ranges,
                   last_decision=self.last_decision,last_rejection=self.last_rejection,position=None,
                   feed_error=self.feed_error, reference_source=self.reference_source,
                   reference_id=self.reference_id, heading_quality=self.heading_quality,
                   reference_status='aligned' if self.alignment is not None else 'missing_world_alignment',
                   stationary_reference=self.rest_gate.copy())
        if h is None:
            return out
        out.update(state='stationary' if h[3] else 'propagating', observed_at=h[0],velocity_m_s=self.v.tolist(),
                   vector_frame='ahrs_gravity_aligned',displacement_m=h[1].tolist())
        if self.offset is not None and self.alignment is not None:
            p = self.offset+self.alignment@h[1]
            out.update(position=dict(zip(('x_m','y_m','z_m'),p.tolist())), position_sigma_m=self.absolute_sigma+h[2],
                       position_source=self.absolute_source, velocity_enu_m_s=(self.alignment@self.v).tolist())
        return out

    def gate_range(self, key, d, observed, h):
        refs = self.links.get(key, ())
        if len(refs) < 3 or h is None:
            return True
        old_d, old_h = sorted(refs,key=lambda item:item[0])[len(refs)//2]
        # Triangle inequality applies to each directed link even before yaw or
        # the absolute rover position is known. A constant FTM bias cancels.
        travel = float(np.linalg.norm(h[1]-old_h[1]))
        limit = travel + 3*abs(h[2]-old_h[2]) + 1.35
        if abs(d-old_d) <= limit:
            return True
        self.rejected_ranges += 1
        self.last_rejection = dict(kind='range',anchor=key[0],direction=key[1],observed_at=observed,
                                   change_m=abs(d-old_d),limit_m=limit)
        return False

    def position_gate(self, p, h, rms):
        if h is None:
            return True
        if self.offset is not None and self.alignment is not None:
            delta = float(np.linalg.norm(p-(self.offset+self.alignment@h[1])))
            limit = 3*math.hypot(self.absolute_sigma+h[2],max(.25,rms))+.10
        elif self.relative_fix:
            last_p, last_h, sigma = self.relative_fix
            delta = float(np.linalg.norm(p-last_p))
            limit = float(np.linalg.norm(h[1]-last_h[1])) + 3*(abs(h[2]-last_h[2])+math.hypot(sigma,max(.25,rms)))+.10
        else:
            return True
        if delta <= limit:
            return True
        self.last_rejection = dict(kind='position',innovation_m=delta,limit_m=limit,observed_at=h[0])
        return False

    def correct(self, p, h, rms):
        sigma = max(.50,rms)
        self.relative_fix = (p.copy(),h,sigma)
        self.accepted_fixes += 1
        if self.alignment is None or not self.surveyed:
            return
        if self.offset is None:
            self.offset = p-self.alignment@h[1]
            self.absolute_sigma = sigma
        else:
            pred_sigma = self.absolute_sigma+h[2]
            gain = pred_sigma**2/(pred_sigma**2+sigma**2)
            self.offset += gain*(p-(self.offset+self.alignment@h[1]))
            self.absolute_sigma = max(.25,math.sqrt(1-gain)*pred_sigma)
        self.absolute_source = 'accepted_independent_ftm'


class VisionFeed(threading.Thread):
    """Reuse the local push stream including the firmware IMU history ring."""
    def __init__(self, nav, path):
        super().__init__(daemon=True, name='tof-imu-stream')
        self.nav, self.path = nav, str(path)

    def run(self):
        while True:
            try:
                with socket.socket(socket.AF_UNIX) as sock:
                    sock.settimeout(5)
                    sock.connect(self.path)
                    conn = http.client.HTTPConnection('localhost')
                    conn.sock = sock
                    conn.request('GET','/v1/live')
                    with conn.getresponse() as r:
                        if r.status != 200:
                            raise OSError(f'vision HTTP {r.status}')
                        while True:
                            total, = struct.unpack('!I',r.read(4))
                            size, = struct.unpack('!I',r.read(4))
                            if size > total-4:
                                raise ValueError('invalid live frame')
                            meta = json.loads(r.read(size))
                            r.read(total-4-size)
                            self.nav.ingest(meta,time.time())
                            self.nav.feed_error = None
            except (OSError, ValueError, TypeError, KeyError, struct.error, http.client.HTTPException) as exc:
                self.nav.feed_error = type(exc).__name__
                time.sleep(.3)


class InertialRefinement:
    """Stage independent radio sessions until their joint fix has been checked."""
    def __init__(self, nav):
        self.nav = nav
        self.baseline = None
        self.previous_segment = None

    def configure(self, baseline):
        self.baseline = baseline
        with self.nav.lock:
            self.nav.configure(baseline)

    def observe(self, refinement, anchor, distance, observed, now, direction, session_id):
        from tof_baseline import localize_relative
        from tof_ground import fit_ground, ground_observations, validate_transfers
        nav, b = self.nav, self.baseline
        with nav.lock:
            h = nav.at(observed)
            key = (anchor, direction)
            if now-observed > 30 or observed > now:
                return
            prev = nav.seen.get(key)
            if prev and (observed < prev[1]-.1 or (prev[0] == session_id if session_id is not None else observed-prev[1] < .75)):
                return
            nav.seen[key] = session_id, observed
            if not b or not b.get('valid') or nav.at(now) is None or h is None:
                # Without contemporaneous evidence an inertial veto is unjustified.
                refinement.observe(anchor,distance,observed,now=now,direction=direction,session_id=session_id)
                return
            if refinement.segment != self.previous_segment:
                nav.pending.clear()
                self.previous_segment = refinement.segment
            if not nav.gate_range(key,distance,observed,h):
                nav.last_decision = 'range_rejected_before_average'
                return
            surveyed = {p['bssid'] for p in b['positions']}
            if anchor not in surveyed:
                # An unfitted anchor cannot vote in the position solution.
                refinement.observe(anchor,distance,observed,now=now,direction=direction,session_id=session_id)
                return
            nav.pending[key] = dict(target_bssid=anchor,distance_m=distance,observed=observed,
                                    direction=direction,session_id=session_id)
            batch = [r for r in nav.pending.values() if now-r['observed'] <= 30 and nav.at(r['observed']) is not None]
            per_anchor = {}
            for r in sorted(batch,key=lambda x:x['observed']):
                per_anchor[r['target_bssid']] = r
            contact = b.get('rover_ground_contact') or {}
            pickup = refinement.last_pickup
            grounded = bool(b.get('ground_plane') and contact.get('grounded') and
                            (pickup is None or contact.get('confirmed_at',0) > pickup))
            needed = 3 if grounded or b['dimension'] == 2 else 4
            if len(per_anchor) < needed:
                nav.last_decision = 'awaiting_independent_ranges'
                return
            chosen = list(per_anchor.values())
            # During motion, compensate each sphere to the common fix time when
            # ENU position is known. Otherwise retain only near-simultaneous data.
            h = nav.at(max(r['observed'] for r in chosen))
            anchors = {r['bssid']:point(r) for r in b['positions']}
            modeled = []
            for r in chosen:
                t = nav.at(r['observed'])
                shift = h[1]-t[1]
                d = r['distance_m']
                if nav.offset is not None and nav.alignment is not None:
                    a = anchors[r['target_bssid']]
                    d += np.linalg.norm(nav.offset+nav.alignment@h[1]-a)-np.linalg.norm(nav.offset+nav.alignment@t[1]-a)
                elif np.linalg.norm(shift) > .20:
                    nav.last_decision = 'awaiting_time_aligned_ranges'
                    return
                modeled.append(dict(r,distance_m=d))
            if grounded:
                rows = ground_observations(modeled,b['radio_calibration'],False)
                height = contact.get('radio_height_m',.0635)
                fit = fit_ground(b,validate_transfers(b,rows,height),height)
            else:
                fit = localize_relative(b,modeled)
            nav.pending.clear()
            p = point(fit.get('position'))
            if p is not None and fit.get('valid'):
                rms = fit['position'].get('residual_rms_m',.5)
                if not nav.position_gate(p,h,rms):
                    nav.rejected_fixes += 1
                    nav.last_decision = 'position_rejected_before_average'
                    return
                nav.correct(p,h,rms)
                nav.last_decision = 'position_accepted'
            elif nav.relative_fix is not None or nav.offset is not None:
                nav.last_decision = 'unqualified_batch_excluded'
                return
            else:
                # Range evidence can still refine a first fix with unresolved
                # geometry. Never promote an ambiguous candidate to a pose.
                nav.last_decision = 'range_only_bootstrap'
            for r in chosen:
                target, direction = r['target_bssid'], r['direction']
                t = nav.at(r['observed'])
                refs = nav.links.setdefault((target,direction),deque(maxlen=7))
                refs.append((r['distance_m'],t))
                refinement.observe(target,r['distance_m'],r['observed'],now=now,
                                   direction=direction,session_id=r['session_id'])

    def publish(self, result, now):
        nav = self.nav
        with nav.lock:
            h = nav.at(now)
            rejected = False
            refinement = result.get('rover_refinement') or {}
            for candidate in (result.get('position'),refinement.get('estimate')):
                p = point(candidate)
                if p is not None and not nav.position_gate(p,h,candidate.get('residual_rms_m',.5)):
                    rejected = True
            if rejected:
                result.update(position=None,valid=False,status='inertial_inconsistent_position')
                result.pop('ground_position',None)
                refinement['estimate'] = None
            info = nav.display(now)
            info['suppress_range_fallback'] = bool(h and (rejected or nav.offset is not None or nav.relative_fix is not None
                or nav.last_decision in ('range_rejected_before_average','position_rejected_before_average','unqualified_batch_excluded')))
            result['inertial_navigation'] = info
