"""Capture a local magnetic-north reference at an explicitly surveyed stationary zero."""
import base64
import http.client
import ipaddress
import json
import sqlite3
import threading
import time
from collections import deque
from urllib.parse import urlsplit, unquote

import numpy as np


def profile_request(directory, receiver, payload=None):
    from tof_host_io import client
    if client() is not None:
        return client().device_request(receiver,"/api/ahrs/calibration",payload)
    cfg=json.loads((directory/'config.json').read_text())['camera']
    if cfg['device_id']!=receiver:raise ValueError('Wrong calibration receiver')
    with sqlite3.connect('file:'+str(directory/'device-routes.sqlite3')+'?mode=ro',uri=True) as db:
        row=db.execute('SELECT host FROM routes WHERE device_id=?',(receiver,)).fetchone()
    if not row:raise ValueError('Unknown magnetometer route')
    connection=http.client.HTTPConnection(str(ipaddress.ip_address(row[0])),timeout=3)
    url=urlsplit(cfg.get('url',''));user=cfg.get('username',unquote(url.username or ''))
    password=cfg.get('password',unquote(url.password or ''));headers={'Content-Type':'application/json'}
    if user or password:headers['Authorization']='Basic '+base64.b64encode((user+':'+password).encode()).decode()
    try:
        connection.request('POST' if payload else 'GET','/api/ahrs/calibration',
                           json.dumps(payload) if payload else None,headers=headers)
        response=connection.getresponse();result=json.loads(response.read())
        if response.status not in (200,202):raise ValueError('Profile HTTP '+str(response.status))
        return result
    finally:connection.close()


class StationaryNorth:
    def __init__(self,directory):
        self.directory=directory
        self.path=directory/'survey-magnetic-north.json'
        self.receipt=json.loads(self.path.read_text()) if self.path.exists() else {}
        self.window=deque(maxlen=96)
        self.key=self.latest=None
        self.working=False
        self.retry_at=0.
        self.error=None

    def observe(self,pose,nav,env,now):
        ref=pose.reference
        if not ref or self.receipt.get('zero_id')==ref['id'] or self.working or now<self.retry_at:return
        sample=pose.sample
        if not sample or pose.rotation is None or now-sample['at']>1:return
        world=nav.world_body
        if (world.body is None or not world.on_floor or not nav.rest_gate.get('imu_quiet') or
                np.linalg.norm(world.body-pose.body[:3,3])>.015 or
                np.trace(pose.rotation.T@pose.body[:3,:3])<1+2*np.cos(np.deg2rad(3))):
            self.window.clear();return
        a=env.get('ahrs') or {};m=a.get('mag') or {};imu=a.get('imu') or {}
        if not (env.get('timing_qualified') and m.get('valid') and m.get('frame')=='ak8963_native_xyz'
                and imu.get('valid') and 0<=m.get('age_ms',9999)<200 and 0<=imu.get('age_ms',9999)<200):return
        key=ref['id'],env.get('receiver_id'),env.get('boot_id')
        if self.key!=key:self.window.clear();self.latest=None;self.key=key
        identity=m.get('sample_count'),m.get('observed_uptime_ms')
        if identity==self.latest:return
        field=np.asarray(m.get('field_uT'),dtype=float);acc=np.asarray(imu.get('accel_m_s2'),dtype=float)
        if field.shape!=(3,) or acc.shape!=(3,) or not np.isfinite(field).all() or not 8.5<np.linalg.norm(acc)<11.:return
        self.latest=identity;self.window.append((now,field,acc))
        rows=[row for row in self.window if now-row[0]<8.]
        if len(rows)<31 or now-rows[0][0]<1.:return
        fields=np.array([r[1] for r in rows]);accs=np.array([r[2] for r in rows])
        median=np.median(fields,axis=0);noise=float(np.median(np.linalg.norm(fields-median,axis=1)))
        if noise>max(2.,.035*np.linalg.norm(median)):return
        accel=np.median(accs,axis=0);unit=accel/np.linalg.norm(accel)
        imu_field=median[[1,0,2]]*np.array([1.,1.,-1.])
        if np.linalg.norm(imu_field-unit*(unit@imu_field))<max(5.,5*noise):return
        profile=dict(schema='rover.ahrs.calibration/1',world_id=pose.world_id,
                     reference_id='survey-'+ref['id'],world_imu_rotation=pose.imu[:3,:3].tolist(),
                     north_accel_m_s2=accel.tolist(),north_magnetic_uT=median.tolist())
        stats=dict(zero_id=key[0],receiver_id=key[1],boot_id=key[2],sample_count=len(rows),
                   span_s=now-rows[0][0],field_mad_uT=noise,observed_at=now,world_id=pose.world_id)
        self.working=True
        threading.Thread(target=self._save,args=(pose,nav,profile,stats),daemon=True,name='survey-magnetic-north').start()

    def _save(self,pose,nav,profile,stats):
        from tof_survey_pose import atomic_json
        try:
            before=profile_request(self.directory,stats['receiver_id'])
            if before.get('reference_id')!=profile['reference_id']:
                with nav.lock:
                    if not pose.reference or pose.reference['id']!=stats['zero_id']:return
                profile_request(self.directory,stats['receiver_id'],profile)
            for _ in range(25):
                saved=profile_request(self.directory,stats['receiver_id'])
                if (saved.get('persistent') and not saved.get('pending') and saved.get('world_reference_valid')
                        and saved.get('reference_id')==profile['reference_id'] and not saved.get('storage_error')
                        and saved.get('world_id')==profile['world_id']):break
                time.sleep(.2)
            else:raise ValueError('Flash profile not verified')
            if not np.allclose(saved['north_magnetic_uT'],profile['north_magnetic_uT'],atol=.015):
                # A previous process may already have saved this exact zero. Its
                # flash vector is authoritative on recovery.
                if before.get('reference_id')!=profile['reference_id']:raise ValueError('Flash magnetic vector mismatch')
            receipt=dict(stats,profile_revision=saved['revision'],reference_id=saved['reference_id'],
                         north_magnetic_uT=saved['north_magnetic_uT'],north_accel_m_s2=saved['north_accel_m_s2'],
                         stored=True,multi_orientation_calibrated=saved.get('mag_calibrated',False))
            with nav.lock:
                if pose.reference and pose.reference['id']==stats['zero_id']:
                    updated=dict(pose.reference,profile=saved['reference_id'],persistent=pose.imu[:3,:3].tolist())
                    atomic_json(pose.path,updated);pose.reference=updated
                    atomic_json(self.path,receipt);self.receipt=receipt;self.error=None
        except (OSError,ValueError,KeyError,sqlite3.Error,http.client.HTTPException) as exc:
            self.error=type(exc).__name__+': '+str(exc)[:120];self.retry_at=time.time()+10
        finally:self.working=False
