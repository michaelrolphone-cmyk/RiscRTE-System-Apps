from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
import unittest

import numpy as np
from tof_inertial import InertialNavigator


WORLD = [[0.,-1.,0.],[1.,0.,0.],[0.,0.,1.]]


def packet(frame, boot='after-reboot', world='survey'):
    capture = 1_000_000 + frame*200_000
    samples = [dict(monotonic_us=capture-j*20_000, generation=1, valid=True,
                    attitude_valid=True, quaternion_wxyz=[1,0,0,0],
                    accel_m_s2=[0,0,9.80665], gyro_rad_s=[0,0,0]) for j in reversed(range(10))]
    attitude = dict(valid=True, generation=1, yaw_origin_uptime_ms=20,
                    quaternion_wxyz=[1,0,0,0], world_alignment_valid=True,
                    world_reference_persistent=True, world_reference_id='persisted-north',
                    world_id=world, world_rotation_enu_imu=WORLD,
                    heading_quality='factory_only_magnetic_reference')
    return dict(receive_decode_timestamp=datetime.fromtimestamp(1000+frame*.2, timezone.utc).isoformat(),
                ahrs=dict(timing_qualified=True, boot_id=boot, receiver_id='camera',
                          capture_monotonic_us=capture, snapshot={'elapsed_since_receive_s':0},
                          ahrs=dict(attitude=attitude, imu_samples=samples)),
                ultrasonic=dict(boot_id=boot, fresh=True, last_receive_age_s=0,
                    ultrasonic=dict(sensors=[dict(id='rear',valid=True,age_ms=30,distance_m=.574),
                                            dict(id='right',valid=True,age_ms=40,distance_m=.0299),
                                            dict(id='left',valid=False,age_ms=20,distance_m=None),
                                            dict(id='front',valid=True,age_ms=20,distance_m=1.+.3*(frame%2))])))


class InertialReferenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.root.joinpath('survey-world.json').write_text(json.dumps({'world_id':'survey'}))
        # A previous boot's file must not be reused as if it matched the current gyro origin.
        self.root.joinpath('attitude-world-reference.json').write_text(json.dumps({'boot_id':'old'}))
        self.nav = InertialNavigator(self.root)
        self.nav.configure({'frame_id':'ENU','epoch':30,'world_alignment':'configured_east_north_up'})

    def tearDown(self):
        self.temp.cleanup()

    def feed(self, count=8, edit=None, start=0):
        for frame in range(start, start+count):
            value = packet(frame)
            if edit:
                edit(value)
            self.nav.ingest(value, 1000+frame*.2)
        return self.nav.display(1000+(start+count-1)*.2)

    def test_restored_firmware_reference_and_valid_near_sonar_initialize(self):
        result = self.feed()
        self.assertEqual(result['state'],'stationary')
        self.assertEqual(result['world_id'],'survey')
        self.assertEqual(result['reference_source'],'persistent_firmware_world_rotation')
        self.assertEqual(result['heading_quality'],'factory_only_magnetic_reference')
        np.testing.assert_allclose(self.nav.alignment,WORLD)
        np.testing.assert_allclose(self.nav.v,[0,0,0])
        self.assertEqual(result['stationary_reference']['evidence'], 'imu_quiet_assumed_zero_velocity')
        h = self.nav.at(1001.4)
        self.nav.correct(np.array([1.,2.,3.]),h,.5)
        np.testing.assert_allclose(list(self.nav.display(1001.4)['position'].values()), [1,2,3])
        self.assertEqual(json.loads(self.root.joinpath('survey-world.json').read_text()),{'world_id':'survey'})

    def test_missing_sonar_does_not_block_inertial_initialization(self):
        result = self.feed(edit=lambda m:m.pop('ultrasonic'))
        self.assertEqual(result['state'],'stationary')
        self.assertTrue(result['stationary_reference']['imu_quiet'])
        self.assertFalse(result['stationary_reference']['constant_velocity_observable'])

    def test_noisy_stale_or_invalid_sonar_has_no_effect_on_navigation(self):
        for mode in ['stale', 'invalid', 'noisy']:
            self.nav = InertialNavigator(self.root)
            def edit(m):
                for sensor in m['ultrasonic']['ultrasonic']['sensors']:
                    sensor.update(valid=mode!='invalid',age_ms=700 if mode=='stale' else 0,
                                  distance_m=1000 if mode=='noisy' else None)
            result = self.feed(edit=edit)
            self.assertEqual(result['state'],'stationary')
            self.assertEqual(result['world_id'],'survey')

    def test_quiet_imu_does_not_zero_a_previously_integrated_velocity(self):
        self.feed()
        def accelerate(m):
            for sample in m['ahrs']['ahrs']['imu_samples']:
                sample['accel_m_s2']=[.7,0,9.80665]
        self.feed(count=10,start=8,edit=accelerate)
        speed=np.linalg.norm(self.nav.v)
        self.assertGreater(speed,1.)
        result=self.feed(count=15,start=18)
        self.assertEqual(result['state'],'propagating')
        self.assertGreater(np.linalg.norm(self.nav.v),1.)
        self.assertFalse(result['stationary_reference']['velocity_consistent'])

    def test_wrong_world_or_improper_rotation_cannot_supply_alignment(self):
        for transform in ['world','rotation']:
            self.nav = InertialNavigator(self.root)
            def edit(m):
                a=m['ahrs']['ahrs']['attitude']
                if transform=='world':a['world_id']='unrelated'
                else:a['world_rotation_enu_imu']=[[-1,0,0],[0,1,0],[0,0,1]]
            result = self.feed(edit=edit)
            self.assertIsNone(result['world_id'])
            self.assertEqual(result['reference_status'],'missing_world_alignment')
            self.assertIsNone(self.nav.alignment)

    def test_sample_gap_still_resets_unobserved_motion(self):
        self.feed()
        gap=packet(12)
        self.nav.ingest(gap,1002.4)
        self.assertEqual(self.nav.gaps,1)
        self.assertFalse(self.nav.history)
        np.testing.assert_allclose(self.nav.p,[0,0,0])
        self.assertIsNotNone(self.nav.alignment)
        result=self.feed(start=13)
        self.assertEqual(result['state'],'stationary')

    def test_reboot_uses_firmware_reference_without_carrying_position(self):
        self.feed()
        self.nav.offset=np.array([7.,8.,9.])
        m=packet(10,boot='another-reboot')
        m['ahrs']['ahrs']['attitude']['world_rotation_enu_imu']=np.eye(3).tolist()
        self.nav.ingest(m,1002.)
        self.assertEqual(self.nav.identity[0],'another-reboot')
        self.assertIsNone(self.nav.offset)
        np.testing.assert_allclose(self.nav.alignment,np.eye(3))

    def test_push_prevents_old_camera_boot_from_replacing_live_reference(self):
        self.feed(edit=lambda m:m.update(sensor_delivery='websocket_push'))
        identity=self.nav.identity
        stale_camera=packet(8,boot='previous-camera-boot')
        self.nav.ingest(stale_camera,1001.6)
        self.assertEqual(self.nav.identity,identity)

    def test_buffered_measured_samples_bridge_transport_interruption(self):
        self.feed()
        # No packets arrive for 1.6s, but the device retained every actual sample.
        m=packet(15)
        m['ahrs']['ahrs']['imu_history_window_ms']=2500
        m['ahrs']['ahrs']['imu_samples']=[dict(monotonic_us=us,generation=1,valid=True,
            attitude_valid=True,quaternion_wxyz=[1,0,0,0],accel_m_s2=[0,0,9.80665],
            gyro_rad_s=[0,0,0]) for us in range(2420000,4000001,20000)]
        self.nav.ingest(m,1003.)
        self.assertEqual(self.nav.gaps,0)
        self.assertEqual(self.nav.display(1003.)['state'],'stationary')


if __name__=='__main__':
    unittest.main()
