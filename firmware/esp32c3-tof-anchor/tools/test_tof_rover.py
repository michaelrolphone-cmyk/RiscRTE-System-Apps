import math
import tempfile
import unittest
from pathlib import Path
from tof_rover import RoverRefinement

POINTS = [[0,0,0],[8,0,0],[0,8,0],[0,0,5],[8,8,5]]
BASE = dict(valid=True, dimension=3, frame_id='test-frame', epoch=1, revision=1, condition_number=2,
            positions=[dict(bssid=str(i),x_m=p[0],y_m=p[1],z_m=p[2]) for i,p in enumerate(POINTS)])

class RefinementTests(unittest.TestCase):
    def setUp(self):
        self.t = 1000.
        self.r = RoverRefinement(clock=lambda:self.t)

    def imu(self, gyro=0., accel=None):
        return dict(imu=dict(valid=True, age_ms=0, sample_count=int(self.t*100),
                             gyro_rad_s=[0,0,gyro],accel_m_s2=accel or [0,0,9.8]))

    def feed(self, n, p=(2,3,1), noise=.8, spike=False):
        self.t += 10
        self.r.observe_motion(self.imu())
        for i,a in enumerate(POINTS):
            d=math.dist(p,a)+noise*math.sin(n*2.399+i*1.713)
            self.r.observe(str(i),d+(30 if spike and i==0 else 0),self.t,now=self.t)
        return self.r.snapshot(BASE)

    def test_stationary_repeated_sessions_converge_and_reduce_jitter(self):
        estimates=[]
        for n in range(160):
            s=self.feed(n)
            if s['estimate']:
                estimates.append([s['estimate'][k] for k in ['x_m','y_m','z_m']])
        self.assertLess(math.dist(estimates[-1],[2,3,1]),.08)
        self.assertLess(max(math.dist(p,estimates[-1]) for p in estimates[-20:]),.08)
        self.assertGreater(s['minimum_sessions_per_anchor'],140)
        self.assertEqual(s['state'],'stationary_refining')
        self.assertLess(max(math.dist(p,estimates[-1]) for p in estimates[-20:]),max(math.dist(p,estimates[-1]) for p in estimates[:10]))

    def test_duplicate_polls_never_increase_information(self):
        self.feed(1)
        total=self.r.total_sessions
        for _ in range(40):
            self.r.observe('0',math.dist([2,3,1],POINTS[0]),self.t+.1,now=self.t+.2)
        self.assertEqual(self.r.total_sessions,total)

    def test_single_link_outlier_is_rejected_without_motion(self):
        for n in range(30):s=self.feed(n,noise=.2)
        segment=s['segment']
        s=self.feed(31,noise=.2,spike=True)
        self.assertEqual(s['segment'],segment)
        self.assertGreater(self.r.rows['0']['rejected'],0)
        self.assertLess(math.dist([s['estimate'][k] for k in ['x_m','y_m','z_m']],[2,3,1]),.2)

    def test_ahrs_motion_invalidates_then_new_location_refines(self):
        for n in range(20):self.feed(n,noise=.1)
        segment=self.r.segment
        self.t+=1
        self.assertTrue(self.r.observe_motion(self.imu(gyro=.3)))
        self.assertIsNone(self.r.snapshot(BASE)['estimate'])
        self.assertGreater(self.r.segment,segment)
        for n in range(50):s=self.feed(n,p=(4,2,2),noise=.1)
        self.assertLess(math.dist([s['estimate'][k] for k in ['x_m','y_m','z_m']],[4,2,2]),.1)

    def test_visual_motion_uses_fresh_same_map_poses_only(self):
        for n in range(12):self.feed(n)
        self.r.observe_visual(dict(tracking=True,observed_at=self.t,map_id='m',position=[0,0,0],metric=True))
        self.t+=1
        self.assertTrue(self.r.observe_visual(dict(tracking=True,observed_at=self.t,map_id='m',position=[.2,0,0],metric=True)))
        self.assertIsNone(self.r.snapshot(BASE)['estimate'])
        self.assertFalse(self.r.observe_visual(dict(tracking=True,observed_at=self.t-10,map_id='m',position=[20,0,0])))

    def test_sustained_multilink_range_change_reacquires_without_imu(self):
        for n in range(40):self.feed(n,noise=.05)
        segment=self.r.segment
        for n in range(12):
            self.t+=10
            for i,a in enumerate(POINTS):self.r.observe(str(i),math.dist([5,1,3],a),self.t)
            s=self.r.snapshot(BASE)
        self.assertGreater(self.r.segment,segment)
        self.assertLess(math.dist([s['estimate'][k] for k in ['x_m','y_m','z_m']],[5,1,3]),.15)

    def test_persistence_deduplication_and_camera_boot_boundary(self):
        with tempfile.TemporaryDirectory() as d:
            self.r.path=Path(d)/'rover.json'
            self.r.observe_boot('one')
            for n in range(20):s=self.feed(n)
            self.r.save(force=True)
            loaded=RoverRefinement(self.r.path,clock=lambda:self.t)
            self.assertEqual(loaded.snapshot(BASE)['estimate'],s['estimate'])
            total=loaded.total_sessions
            self.assertFalse(loaded.observe('0',4.,self.t))
            self.assertEqual(loaded.total_sessions,total)
            loaded.observe_boot('two')
            self.assertIsNone(loaded.snapshot(BASE)['estimate'])

    def test_stale_or_changed_frame_cannot_present_old_fix(self):
        for n in range(10):self.feed(n)
        self.t+=31
        self.assertIsNone(self.r.snapshot(BASE)['estimate'])
        self.r.snapshot(dict(BASE,frame_id='new',epoch=2))
        self.assertEqual(self.r.rows,{})
        self.assertFalse(self.r.observe('0',3.,self.t-40))

    def test_acceleration_invalidates_but_stale_or_duplicate_imu_does_not(self):
        for n in range(15):self.feed(n)
        segment=self.r.segment
        self.assertFalse(self.r.observe_motion(self.imu(gyro=1.)))
        stale=self.imu(gyro=1.);stale['imu']['age_ms']=900
        self.assertFalse(self.r.observe_motion(stale))
        self.assertEqual(self.r.segment,segment)
        self.t+=1
        self.assertTrue(self.r.observe_motion(self.imu(accel=[1,0,9.8])))
        self.assertIsNone(self.r.snapshot(BASE)['estimate'])

    def test_range_graph_uses_accumulated_shared_ranges_and_keeps_raw_matrix(self):
        from unittest.mock import patch
        from tof_geometry import RangeGraph
        class Baseline:
            def update(self):return BASE
        graph=RangeGraph(baseline=Baseline(),rover_refinement=self.r)
        for n in range(15):
            self.t+=10
            now=100+n*10
            self.r.observe_motion(self.imu())
            for i,a in enumerate(POINTS):
                graph.nodes[str(i)]=dict(received=now,packet=dict(bssid=str(i)),ranges={})
                with patch('tof_geometry.time.time',return_value=self.t):
                    graph.ingest_rover(dict(status='success',target=str(i),age_ms=0,distance_m=math.dist([2,3,1],a)+.1*math.sin(n+i),samples=8,rtt_ns=30),received=now)
            result=graph.snapshot(now)
        self.assertEqual(len(result['rover_ranges']),5)
        self.assertGreater(result['rover_refinement']['session_count'],60)
        self.assertEqual(result['status'],'refined_relative_position')
        self.assertTrue(result['valid'])
        self.assertLess(math.dist([result['position'][k] for k in ['x_m','y_m','z_m']],[2,3,1]),.1)

if __name__=='__main__':unittest.main()
