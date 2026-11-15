"""Independent front/rear FTM sessions reduced to the front radio's reference point."""
import json
import math
import sqlite3
import threading
import time
import urllib.request
from collections import defaultdict, deque
from concurrent.futures import ThreadPoolExecutor

import numpy as np

from tof_survey_pose import atomic_json


class DualRadioRanging:
    def __init__(self, directory, nav):
        self.directory=directory
        self.nav=nav
        survey=json.loads((directory/'survey-world.json').read_text())
        rig=json.loads((directory/'camera-rig.json').read_text())
        self.rear_id=rig['rear']['camera']['device_id']
        self.front_id=json.loads((directory/'config.json').read_text())['camera']['device_id']
        self.survey=np.asarray(survey['reference']['T_enu_body'])
        camera=np.linalg.inv(self.survey)@np.asarray(survey['reference']['T_enu_camera'])
        self.mount={'front':camera[:3,3],'rear':(camera@np.asarray(rig['rear']['T_front_camera_rear_camera_m']))[:3,3]}
        self.anchors={r['peer'].lower():np.asarray(r['vertices_m'][-1]) for r in survey['features']
                      if r.get('kind')=='ftm' and r.get('peer')}
        self.signature=[survey['world_id'],survey['survey_revision'],rig['revision'],self.front_id,self.rear_id]
        self.path=directory/'dual-radio-survey-calibration.json'
        stored=json.loads(self.path.read_text()) if self.path.exists() else {}
        self.cal=stored.get('links',{}) if stored.get('signature')==self.signature else {}
        self.samples=defaultdict(lambda:deque(maxlen=15))
        self.lock=threading.Lock()
        self.pending={}
        self.latest={}
        self.last={}
        self.seen=set()
        self.sequence=deque(maxlen=200)
        self.requests={}
        self.command=None
        self.next_send=0.
        self.rear_status={}
        self.pool=ThreadPoolExecutor(max_workers=1,thread_name_prefix='rear-ftm-command')
        self.combined={}
        self.counts={'front':0,'rear':0}

    def receive(self,meta):
        if not meta.get('sensor_tick'):return
        side=meta.get('camera_id')
        if side not in ('front','rear'):return
        p=meta.get('sensor_packet') or {}
        if p.get('receiver_id')!=({'rear':self.rear_id,'front':self.front_id}[side]):return
        ftm=p.get('ftm') or {}
        interval=meta.get('capture_host_monotonic_interval_s')
        if not isinstance(interval,list) or len(interval)!=2 or interval[1]<interval[0] or interval[1]-interval[0]>1.:return
        stamp=sum(interval)/2
        if not -.05<=time.monotonic()-stamp<2:return
        with self.lock:
            if side=='rear':self.rear_status=dict(ftm,boot_id=p.get('boot_id'),capture=stamp)
            target=str(ftm.get('target') or '').lower()
            age=ftm.get('age_ms')
            if (ftm.get('status')!='success' or ftm.get('busy') or ftm.get('retiring')
                    or not isinstance(age,(int,float)) or not 0<=age<=30000
                    or not isinstance(ftm.get('distance_m'),(int,float)) or not 0<ftm['distance_m']<1000
                    or not p.get('boot_id') or not isinstance(ftm.get('report_id'),int) or not target):return
            token=(side,p['boot_id'],ftm['report_id'])
            if token in self.seen:return
            if len(self.sequence)==self.sequence.maxlen:self.seen.discard(self.sequence[0])
            self.sequence.append(token);self.seen.add(token)
            self.pending[side,target]=dict(ftm,side=side,receiver_id=p['receiver_id'],boot_id=p['boot_id'],
                                          target=target,observed=stamp-age/1000)

    def request(self,target,channel):
        with sqlite3.connect('file:'+str(self.directory/'device-routes.sqlite3')+'?mode=ro',uri=True) as db:
            route=db.execute('SELECT host FROM routes WHERE device_id=?',(self.rear_id,)).fetchone()
        if not route:raise ValueError('rear radio route unavailable')
        url='http://'+route[0]+'/api/telemetry/ftm'
        req=urllib.request.Request(url,data=json.dumps(dict(address=target,channel=channel)).encode(),
                                   headers={'Content-Type':'application/json'})
        with urllib.request.urlopen(req,timeout=3) as response:
            return response.status

    def geometry(self,target,observed):
        n=self.nav
        with n.lock:
            sp=getattr(n,'survey_pose',None)
            if sp is None or sp.rotation is None or sp.sample is None or n.world_body.body is None:return None
            ts=time.time()-(time.monotonic()-observed)
            h=n.at(ts); cur=n.at(time.time())
            stationary=bool(n.rest_gate.get('imu_quiet') and n.rest_gate.get('quiet_duration_s',0)>=max(.4,time.time()-ts))
            if abs(sp.sample['at']-ts)>.65 and not stationary:return None
            pos=n.world_body.body.copy()
            if not stationary:
                if h is None or cur is None or n.alignment is None:return None
                pos=pos+n.alignment@(h[1]-cur[1])
            rot=sp.rotation.copy()
            at_zero=bool(stationary and n.world_body.on_floor and np.linalg.norm(pos-self.survey[:3,3])<.025
                         and np.trace(rot.T@self.survey[:3,:3])>1+2*math.cos(math.radians(3)))
        anchor=self.anchors.get(target)
        if anchor is None:return None
        return ({s:float(np.linalg.norm(pos+rot@v-anchor)) for s,v in self.mount.items()},at_zero)

    def record(self,g,row,now):
        side=row['side'];target=row['target'];self.latest[side,target]=row
        self.counts[side]+=1
        if target not in g.nodes:return
        geom=self.geometry(target,row['observed'])
        if geom is None:return
        distances,at_zero=geom
        if at_zero and target not in self.cal:
            self.samples[side,target].append(row['distance_m']-distances[side])
            front,rear=(self.samples[k,target] for k in ('front','rear'))
            if min(len(front),len(rear))>=4:
                mf,mr=float(np.median(front)),float(np.median(rear))
                mad=max(float(np.median(np.abs(np.asarray(v)-m))) for v,m in ((front,mf),(rear,mr)))
                if mad<=.8:
                    self.cal[target]=dict(front_bias_m=mf,rear_bias_m=mr,scatter_m=mad,
                                          front_samples=len(front),rear_samples=len(rear))
                    atomic_json(self.path,dict(signature=self.signature,links=self.cal))
        if side!='rear':return
        lever=distances['front']-distances['rear']
        bias=self.cal.get(target)
        front=self.latest.get(('front',target))
        equivalent=row['distance_m']+lever
        if bias:equivalent+=bias['front_bias_m']-bias['rear_bias_m']
        elif not front or abs(front['observed']-row['observed'])>10 or abs(front['distance_m']-equivalent)>max(.6,.10*equivalent):
            return
        if equivalent<=0:return
        result=dict(distance_m=equivalent,raw_distance_m=row['distance_m'],observed=row['observed'],
                    samples=row.get('samples'),rtt_ns=row.get('rtt_ns'),sequence=row['report_id'],
                    receiver_id=self.rear_id,radio_origin='rear',lever_correction_m=lever,differential_bias_calibrated=bool(bias))
        self.last[target]=result
        g._refine_report(target,result,'rover_to_anchor',now,'rear/'+row['boot_id'])

    def tick(self,g,now):
        with self.lock:
            observations,self.pending=self.pending,{}
            state=dict(self.rear_status)
        for row in sorted(observations.values(),key=lambda r:r['observed']):self.record(g,row,now)
        if self.command is not None:
            if not self.command.done():return
            try:self.command.result();self.error=None
            except (OSError,ValueError,sqlite3.Error) as exc:self.error=str(exc)
            self.command=None
        if (now<self.next_send or not state or not 0<=now-state['capture']<2.
                or state.get('busy') or state.get('retiring')):return
        eligible=[(k,v['packet']['channel']) for k,v in g.nodes.items()
                  if now-v['received']<30 and v['packet'].get('ftm_state')=='idle']
        if eligible:
            target,channel=min(eligible,key=lambda x:(self.requests.get(x[0],0),x[0]))
            self.next_send=now+2;self.requests[target]=now
            self.command=self.pool.submit(self.request,target,channel)

    def combine(self,combined,directional,active,now):
        by_anchor={r['target_bssid']:r for r in combined}
        self.combined={}
        for anchor,rear in self.last.items():
            if anchor not in active or not 0<=now-rear['observed']<30:continue
            raw=dict(rear,target_bssid=anchor,direction='rover_to_anchor',age_s=now-rear['observed'])
            directional.append(raw)
            row=by_anchor.get(anchor)
            if row is None:
                row=dict(raw,consistent=True,method='rear_lever_corrected',directions=1,directional_measurements=[raw])
                combined.append(row)
            else:
                skew=abs(row['observed']-rear['observed'])
                age_limit=10. if self.nav.rest_gate.get('quiet_duration_s',0.)>=skew else 1.
                if skew>age_limit or row.get('consistent') is False:continue
                a,b=row['distance_m'],rear['distance_m']
                if abs(a-b)>max(1.,.25*min(a,b)):continue
                front=dict(receiver_id=self.front_id,radio_origin='front',distance_m=a,observed=row['observed'])
                row.update(distance_m=(a+b)/2,method='dual_radio_mean',radio_origins=[front,raw],
                           independent_anchor_count=1)
                row['directional_measurements'].append(raw)
                self.combined[anchor]=dict(distance_m=row['distance_m'],front_m=a,rear_equivalent_m=b)
        return combined,directional

    def status(self):
        return dict(receiver_ids={'front':self.front_id,'rear':self.rear_id},origin_body_m={s:p.tolist() for s,p in self.mount.items()},
                    origin_qualification='camera optical centers used as antenna reference points',observations=self.counts.copy(),
                    surveyed_links=self.cal.copy(),combined=self.combined.copy(),rear_request_error=getattr(self,'error',None),
                    reports=[dict(receiver_id=r['receiver_id'],target_bssid=r['target'],report_id=r['report_id'],
                                  raw_m=r['distance_m'],age_s=time.monotonic()-r['observed']) for r in self.latest.values()])
