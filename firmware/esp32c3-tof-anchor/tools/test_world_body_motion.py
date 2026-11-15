import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
import numpy as np

from tof_inertial import InertialNavigator, InertialRefinement

class TestWorldBody(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        body = np.eye(4); body[:3,3] = [.04,.64,0]
        camera = body.copy(); camera[:3,3] += [0,.048,.064]
        (self.root/'survey-world.json').write_text(json.dumps(dict(world_id='room',
            reference=dict(T_enu_camera=camera.tolist(),T_enu_body=body.tolist()))))
        self.start=body[:3,3]
        self.radio=camera[:3,3]
        (self.root/'rover-world-body-pose.json').write_text(json.dumps(dict(world_id='room',
            position_enu_m=self.start.tolist(),source='surveyed body checkpoint',observed_at=1)))
        self.nav=InertialNavigator(self.root)
        self.nav.configure(dict(frame_id='enu',epoch=1,world_alignment='configured_east_north_up'))
        self.gate=InertialRefinement(self.nav)
        self.nav.world_id='room'; self.nav.alignment=np.eye(3)

    def sample(self, t, displacement, rest):
        h=(t,np.array(displacement,dtype=float),0.,rest)
        self.nav.history.append(h)
        self.nav.world_body.track(self.nav,h)
        return h

    def test_stationary_and_unobserved_fixes_cannot_move_or_bias_pose(self):
        h=self.sample(2,[0,0,0],True)
        for delta in ([4,0,0],[0,0,3],[0,0,-3],[.2,0,0]):
            p=self.radio+delta
            self.assertFalse(self.nav.position_gate(p,h,100.))
            result=dict(position=dict(zip(('x_m','y_m','z_m'),p)),valid=True)
            self.gate.publish(result,2)
            self.assertIsNone(result['position'])
            np.testing.assert_allclose(result['inertial_navigation']['world_body_pose']['position_enu_m'],self.start)
        self.gate.configure(dict(frame_id='enu',epoch=1,valid=True,world_alignment='configured_east_north_up'))
        ref=SimpleNamespace(observe=lambda *a,**k:self.fail('unobserved range reached averages'))
        self.gate.observe(ref,'a',7,12.,12.,'forward','session')
        self.assertEqual(self.nav.last_decision,'awaiting_time_aligned_inertial_evidence')

    def test_supported_xyz_and_hold_through_loss_and_restart(self):
        self.sample(2,[0,0,0],True)
        h=self.sample(3,[.6,-.2,.3],False)
        self.assertTrue(self.nav.position_gate(self.radio+h[1],h,.1))
        expected=self.start+h[1]
        pose=self.nav.display(3)['world_body_pose']
        np.testing.assert_allclose(pose['position_enu_m'],expected)
        self.assertFalse(pose['held'])
        self.nav.reset_motion()
        for nav in [self.nav,InertialNavigator(self.root)]:
            p=nav.display(4)['world_body_pose']
            self.assertTrue(p['held']);np.testing.assert_allclose(p['position_enu_m'],expected)
        self.sample(5,[0,0,0],True)
        np.testing.assert_allclose(self.nav.display(5)['world_body_pose']['position_enu_m'],expected)

    def test_unknown_heading_still_checks_height_and_distance(self):
        self.nav.alignment=None
        self.sample(2,[0,0,0],True);h=self.sample(3,[.5,0,0],False)
        self.assertFalse(self.nav.position_gate(self.radio+[0,0,.5],h,.1))
        candidate=self.radio+[0,.49,0]
        self.assertTrue(self.nav.position_gate(candidate,h,.1))
        self.nav.correct(candidate,h,.1)
        np.testing.assert_allclose(self.nav.display(3)['world_body_pose']['position_enu_m'],self.start+[0,.5,0])

    def test_capture_clock_acceleration_and_braking_drive_pose(self):
        # One second at rest initializes the AHRS, followed by equal acceleration/braking.
        for i in range(251):
            acceleration=1. if 100<=i<150 else -1. if 150<=i<200 else 0.
            us=1000000+10000*i
            self.nav.sample(dict(monotonic_us=us,valid=True,attitude_valid=True,
                quaternion_wxyz=[1.,0.,0.,0.],accel_m_s2=[acceleration,0.,9.80665],
                gyro_rad_s=[0.,0.,0.]),us/1e6)
        xyz=self.nav.display(3.5)['world_body_pose']['position_enu_m']
        np.testing.assert_allclose(xyz,self.start+[.25,0,0],atol=.006)
