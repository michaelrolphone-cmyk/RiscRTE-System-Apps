import json
import math
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
from types import SimpleNamespace
import numpy as np
from scipy.spatial.transform import Rotation
from tof_survey_pose import SurveyPose
from tof_lidar_floor import fit_floor

CAM=np.array([[0,0,1,.048],[-1,0,0,0],[0,-1,0,.0635],[0,0,0,1.]])
NORTH=np.array([[0,-1,0,0],[1,0,0,0],[0,0,1,0],[0,0,0,1.]])
ID='8f2a45a8-2761-4980-a1f8-d5492537f929'

def grid(rear=False,wall=False):
    r=np.diag([-1,1,-1,1]) if rear else np.eye(4)
    r[1,3]=-.02
    mount=dict(T_front_camera_sensor_m=r.tolist(),flip_x=True,flip_y=True,
               distance_semantics='axial_z',valid_statuses=[5,9],fov_horizontal_deg=45,fov_vertical_deg=45)
    m=CAM@r
    mm=[]; status=[]
    for i in range(64):
        u=(1-2*(i%8+.5)/8)*math.tan(math.pi/8)
        v=(1-2*(i//8+.5)/8)*math.tan(math.pi/8)
        ray=m[:3,:3]@np.array([u,v,1])
        depth=.5 if wall else -m[2,3]/ray[2]
        mm.append(round(1000*depth) if depth>0 else 0)
        status.append(5 if depth>0 else 0)
    return dict(ready=True,valid=True,distance_mm=mm,target_status=status),mount

class Floors(unittest.TestCase):
    def test_each_mount_observes_floor_without_accepting_wall(self):
        for rear in (False,True):
            raw,mount=grid(rear)
            fit=fit_floor(raw,mount,CAM)
            self.assertGreaterEqual(fit['inliers'],12)
            self.assertGreaterEqual(fit['tilt_sigma_rad'],0.)
            np.testing.assert_allclose(fit['normal'],[0,0,1],atol=.002)
            self.assertIsNone(fit_floor(grid(rear,True)[0],mount,CAM))

    def test_reset_maps_west_tilt_to_survey_and_tracks_turns(self):
        with TemporaryDirectory() as td:
            p=Path(td)
            (p/'survey-world.json').write_text(json.dumps(dict(world_id=ID,reference=dict(
                T_enu_body=NORTH.tolist(),T_enu_camera=(NORTH@CAM).tolist(),T_enu_imu=NORTH.tolist()))))
            s=SurveyPose(p)
            q=Rotation.from_euler('ZYX',[85,16,-20],degrees=True).as_matrix()
            s.sample=dict(at=10.,gyro=q,identity=['front','boot',1,0],persistent=q,profile='firmware')
            world=SimpleNamespace(display=lambda *a: None)
            nav=SimpleNamespace(reset_motion=lambda:None,world_body=world)
            req='4873bf63-0b0a-4afa-9c32-d481951f22c3'
            self.assertEqual(s.zero(nav,ID,req,10.)['reset_id'],req)
            np.testing.assert_allclose(s.rotation,NORTH[:3,:3],atol=1e-9)
            restored=SurveyPose(p)
            self.assertEqual(restored.reference['id'],req)
            s.sample['gyro']=q@Rotation.from_euler('Z',.5).as_matrix()
            s.update_rotation(nav,10.2)
            np.testing.assert_allclose(s.rotation,NORTH[:3,:3]@Rotation.from_euler('Z',.5).as_matrix(),atol=1e-9)
            # A plane tilt correction cannot manufacture or erase a yaw turn.
            s.tilt_bias[:]=[.2,-.1];s.update_rotation(nav,10.2)
            self.assertAlmostEqual(math.atan2(s.rotation[1,0],s.rotation[0,0]),math.pi/2+.5)
            self.assertEqual(s.zero(nav,ID,req,20.)['reset_id'],req)
            with self.assertRaises(ValueError):s.zero(nav,ID,'7cc3facd-78d0-46ff-bdc8-5de49d5ad308',20.)

    def test_stationary_yaw_drift_restart_and_real_turn(self):
        with TemporaryDirectory() as td:
            p=Path(td)
            (p/'survey-world.json').write_text(json.dumps(dict(world_id=ID,reference=dict(
                T_enu_body=NORTH.tolist(),T_enu_camera=(NORTH@CAM).tolist(),T_enu_imu=NORTH.tolist()))))
            s=SurveyPose(p)
            world=SimpleNamespace(display=lambda *a: None)
            nav=SimpleNamespace(reset_motion=lambda:None,world_body=world,rest_gate={'imu_quiet':True})
            sample=dict(at=10.,gyro=np.eye(3),identity=['front','boot',1,0],persistent=None,
                        profile=None,rate=np.zeros(3),rate_noise=.0001,turning=False)
            s.sample=sample
            s.zero(nav,ID,'4873bf63-0b0a-4afa-9c32-d481951f22c3',10.)
            for tick in range(1,61):
                sample.update(at=10.+tick,gyro=Rotation.from_euler('ZYX',[.001*tick,.0005*tick,-.0003*tick]).as_matrix())
                s.update_rotation(nav,sample['at'])
                np.testing.assert_allclose(s.rotation,NORTH[:3,:3],atol=1e-9)
                if tick==26:
                    s=SurveyPose(p)
                    s.sample=sample
            sample.update(at=71.,gyro=Rotation.from_euler('ZYX',[.56,.03+.17,-.018-.14]).as_matrix(),rate=np.array([0,0,.5]),turning=True)
            s.update_rotation(nav,71.)
            np.testing.assert_allclose(s.rotation,NORTH[:3,:3]@Rotation.from_euler('ZYX',[.5,.17,-.14]).as_matrix(),atol=1e-9)
            sample.update(at=71.1,rate=None)
            s.update_rotation(nav,71.1)
            np.testing.assert_allclose(s.rotation,NORTH[:3,:3]@Rotation.from_euler('ZYX',[.5,.17,-.14]).as_matrix(),atol=1e-9)

    def test_floor_fit_uncertainty_rejects_jitter_and_corrects_gyro_tilt(self):
        import time
        from collections import deque
        with TemporaryDirectory() as td:
            p=Path(td)
            (p/'survey-world.json').write_text(json.dumps(dict(world_id=ID,reference=dict(
                T_enu_body=NORTH.tolist(),T_enu_camera=(NORTH@CAM).tolist(),T_enu_imu=NORTH.tolist()))))
            s=SurveyPose(p)
            s.reference={'floor_tilt_at_zero':{}}
            gyro=np.eye(3)
            for observed_deg in [1.,12.]:
                r=Rotation.from_euler('Y',observed_deg,degrees=True).as_matrix()
                fit=dict(normal=r[2],tilt_sigma_rad=math.radians(1.),inliers=18,residual_m=.003)
                with patch('tof_survey_pose.fit_floor',return_value=fit):
                    for seq in range(20):
                        now=100.+observed_deg*10+seq*.3
                        s.attitudes=deque([(now,gyro)])
                        scan=dict(received_monotonic=time.monotonic(),sensors=[
                            dict(sensor_id=sid,mount={'configured':True},sequence=int(now*10))
                            for sid in ('front','rear')])
                        s.observe_scans({'lidar':scan},now,now)
                if observed_deg==1.:
                    np.testing.assert_allclose(s.tilt_bias,0.,atol=1e-10)
                else:
                    self.assertGreater(math.degrees(s.tilt_bias[0]),8.)
                    self.assertEqual(s.floor_sources,['front','rear'])
