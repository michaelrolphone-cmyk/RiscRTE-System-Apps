import json
import time
import threading
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from tof_dual_radio import DualRadioRanging
from tof_magnetic_reference import StationaryNorth


class SurveyRadios(unittest.TestCase):
    def test_unique_receiver_sessions_geometry_and_one_constraint_per_anchor(self):
        with TemporaryDirectory() as td:
            path=Path(td)
            ident=np.eye(4);rear=ident.copy();rear[0,3]=-.0762
            (path/'config.json').write_text(json.dumps({'camera':{'device_id':'f'}}))
            (path/'camera-rig.json').write_text(json.dumps(dict(revision=1,rear=dict(camera={'device_id':'r'},
                                                      T_front_camera_rear_camera_m=rear.tolist()))))
            (path/'survey-world.json').write_text(json.dumps(dict(world_id='survey',survey_revision=2,
                reference=dict(T_enu_body=ident.tolist(),T_enu_camera=ident.tolist()),
                features=[dict(kind='ftm',peer='anchor',vertices_m=[[2,0,0]])])))
            sp=SimpleNamespace(rotation=np.eye(3),sample={'at':time.time()})
            nav=SimpleNamespace(lock=threading.RLock(),survey_pose=sp,rest_gate={'imu_quiet':True,'quiet_duration_s':20},
                world_body=SimpleNamespace(body=np.zeros(3),on_floor=True),at=lambda t:None)
            radio=DualRadioRanging(path,nav);now=time.monotonic();calls=[]
            graph=SimpleNamespace(nodes={'anchor':{}},_refine_report=lambda *a:calls.append(a))
            for index in range(7):
                for side,device,distance in [('front','f',2.2),('rear','r',3.0762)]:
                    ftm=dict(status='success',target='anchor',report_id=index+1,age_ms=0,distance_m=distance,samples=6)
                    tick=dict(sensor_tick=True,camera_id=side,capture_host_monotonic_interval_s=[now,now],
                              sensor_packet=dict(receiver_id=device,boot_id='A',ftm=ftm))
                    radio.receive(tick);radio.receive(tick)
                    rows=radio.pending;radio.pending={}
                    for row in rows.values():radio.record(graph,row,now)
            self.assertEqual(radio.counts,{'front':7,'rear':7})
            self.assertAlmostEqual(radio.last['anchor']['distance_m'],2.2)
            self.assertAlmostEqual(radio.last['anchor']['lever_correction_m'],-.0762)
            front=[dict(target_bssid='anchor',distance_m=2.2,observed=now,consistent=True,directional_measurements=[])]
            result,_=radio.combine(front,[],graph.nodes,now)
            self.assertEqual(len(result),1);self.assertEqual(result[0]['method'],'dual_radio_mean')
            self.assertEqual(result[0]['independent_anchor_count'],1);self.assertTrue(calls)
            self.assertIn('anchor',json.loads(radio.path.read_text())['links'])
            self.assertFalse(radio.combine([],[],graph.nodes,now+60)[0])
            radio.pool.shutdown()

    def test_magnetic_reference_is_read_back_and_preserves_bias(self):
        with TemporaryDirectory() as td:
            p=Path(td);sp=SimpleNamespace(reference={'id':'z','gyro':'preserved'},imu=np.eye(4),path=p/'survey-pose-zero.json')
            nav=SimpleNamespace(lock=threading.RLock());m=StationaryNorth(p)
            desired=dict(reference_id='new',world_id='w',north_magnetic_uT=[23,1,40])
            stored=dict(desired,persistent=True,north_accel_m_s2=[0,0,9.8],world_reference_valid=True,revision=4,mag_calibrated=False)
            with patch('tof_magnetic_reference.profile_request',side_effect=[{'reference_id':'old'},{},stored]) as post:
                m._save(sp,nav,desired,dict(zero_id='z',receiver_id='front'))
                self.assertNotIn('mag_correction',post.call_args_list[1].args[-1])
            self.assertTrue(m.receipt['stored']);self.assertFalse(m.receipt['multi_orientation_calibrated'])
            self.assertEqual(sp.reference['profile'],'new');self.assertEqual(sp.reference['gyro'],'preserved')
