import unittest
from tof_inertial import VisionFeed


class PushedRangingTest(unittest.TestCase):
    def test_only_newest_sensor_observation_keeps_device_time_and_imu(self):
        feed = VisionFeed(None, 'unused')
        def packet(tick, report):
            return dict(sensor_tick=True, ahrs={'ahrs':{'imu':{'sample_count':report}}},
                ranging_observation={'capture_host_monotonic_interval_s':[tick,tick+.02],
                                     'boot_id':'b','ftm':{'report_id':report}})
        feed._ranging({'frame_id':'jpeg'})
        self.assertIsNone(feed.take_ranging())
        feed._ranging(packet(2.,5))
        feed._ranging(packet(1.,4))
        current, observed = feed.take_ranging()
        self.assertEqual(current['ftm']['report_id'],5)
        self.assertEqual(current['ahrs']['imu']['sample_count'],5)
        self.assertAlmostEqual(observed,2.01)
        self.assertIsNone(feed.take_ranging())
