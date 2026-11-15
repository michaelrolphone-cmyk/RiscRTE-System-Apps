import unittest
from tof_geometry import RangeGraph
from tof_baseline import localize_relative
from tof_rover import RoverRefinement
from unittest.mock import patch
A="aa:00:00:00:00:01"
R="bb:00:00:00:00:01"
def packet(seq=1,distance=5.,age=100,range_seq=1,boot=1,valid=True):
    return {"schema":"rover.tof.anchor/1","bssid":A,"ftm_responder":True,
            "boot_id":boot,"seq":seq,"ranges":[{"target_bssid":R,
            "ssid":"C3-TOF-ROVER-test","valid":valid,"status":"ok" if valid else "measuring",
            "distance_m":distance,"age_ms":age,"range_seq":range_seq,"samples":12,"rtt_ns":33}]}
def responder(boot="one",uptime=100000,active=True):
    return {"boot_id":boot,"uptime_ms":uptime,"ftm_responder":{
        "active":active,"bssid":R,"station_connected":True}}
class ReciprocalTests(unittest.TestCase):
    def graph(self):
        g=RangeGraph();g.observe_rover_responder(responder(),100)
        return g
    def forward(self,g,distance=4.8,**kw):
        return g.ingest_rover(dict(status="success",target=A,distance_m=distance,age_ms=100,**kw),100)
    def test_reverse_without_forward(self):
        g=self.graph();g.ingest(packet(),100);out=g.snapshot(100)
        self.assertEqual(out["reciprocal_ftm"]["reverse_count"],1)
        self.assertEqual(out["reciprocal_ftm"]["forward_count"],0)
        self.assertEqual(out["rover_ranges"][0]["direction"],"anchor_to_rover")
    def test_no_fabricated_reverse(self):
        g=self.graph();g.ingest(dict(packet(),ranges=[]),100);self.forward(g)
        out=g.snapshot(100)
        self.assertEqual(out["reciprocal_ftm"]["reverse_count"],0)
        self.assertEqual(out["rover_ranges"][0]["directions"],1)
    def test_pair_is_one_constraint_with_both_raw_reports(self):
        g=self.graph();g.ingest(packet(distance=5.2),100);self.forward(g)
        out=g.snapshot(100);self.assertEqual(len(out["rover_ranges"]),1)
        row=out["rover_ranges"][0]
        self.assertAlmostEqual(row["distance_m"],5.)
        self.assertEqual(row["directions"],2)
        self.assertEqual(len(row["directional_measurements"]),2)
        self.assertTrue(row["consistent"])
    def test_conflict_is_excluded_by_solver(self):
        g=self.graph();g.ingest(packet(distance=12),100);self.forward(g)
        row=g.snapshot(100)["rover_ranges"][0];self.assertFalse(row["consistent"])
        baseline={"positions":[{"bssid":A,"x_m":0,"y_m":0,"z_m":0}],"dimension":3}
        self.assertEqual(localize_relative(baseline,[row])["status"],"awaiting_rover_ranges")
    def test_old_pair_uses_newest_with_unknown_agreement(self):
        g=self.graph();g.ingest(packet(distance=12,age=10000),100);self.forward(g)
        row=g.snapshot(100)["rover_ranges"][0]
        self.assertEqual(row["method"],"newest_direction")
        self.assertEqual(row["distance_m"],4.8);self.assertIsNone(row["consistent"])
    def test_repeated_reverse_sequence_never_refreshes_timestamp(self):
        g=self.graph();g.ingest(packet(),100);g.ingest(packet(seq=2,age=100),101)
        self.assertAlmostEqual(g.snapshot(101)["rover_ranges"][0]["age_s"],1.1)
    def test_repeated_forward_id_never_refreshes_timestamp(self):
        g=self.graph();g.ingest(dict(packet(),ranges=[]),100)
        ftm={"status":"success","target":A,"distance_m":5.,"age_ms":100,"report_id":1}
        self.assertTrue(g.ingest_rover(ftm,100));self.assertFalse(g.ingest_rover(ftm,101))
        self.assertAlmostEqual(g.snapshot(101)["rover_ranges"][0]["age_s"],1.1)
    def test_failed_attempt_expires_at_original_time(self):
        g=self.graph();g.ingest(packet(),100);g.ingest(packet(seq=2,valid=False),101)
        self.assertEqual(g.snapshot(101)["reciprocal_ftm"]["reverse_count"],1)
        g.ingest(packet(seq=3,valid=False),129);g.observe_rover_responder(responder(),130)
        self.assertEqual(g.snapshot(131)["reciprocal_ftm"]["reverse_count"],0)
    def test_rover_reboot_discards_preboot_reverse(self):
        g=self.graph();g.ingest(packet(),100)
        g.observe_rover_responder(responder(boot="two",uptime=100),101)
        self.assertEqual(g.snapshot(101)["reciprocal_ftm"]["reverse_count"],0)
    def test_disabled_responder_cannot_promote_old_reverse(self):
        g=self.graph();g.ingest(packet(),100)
        g.observe_rover_responder(responder(active=False),101)
        self.assertEqual(g.snapshot(101)["reciprocal_ftm"]["reverse_count"],0)
    def test_rover_never_enters_fixed_baseline_even_before_discovery(self):
        class Recorder:
            def observe(self,p):self.packet=p
        baseline=Recorder();g=RangeGraph(baseline);g.ingest(packet(),100)
        self.assertEqual(baseline.packet["ranges"],[])
        self.assertEqual(len(g.nodes[A]["packet"]["ranges"]),1)
    def test_anchor_reboot_restarts_report_sequence(self):
        g=self.graph();g.ingest(packet(range_seq=9),100)
        g.ingest(packet(boot=2,distance=6),101)
        self.assertEqual(g.snapshot(101)["rover_ranges"][0]["distance_m"],6)

    def test_both_directions_feed_stationary_refinement_without_poll_duplicates(self):
        refinement=RoverRefinement()
        g=RangeGraph(rover_refinement=refinement)
        g.observe_rover_responder(responder(),100)
        g.ingest(dict(packet(),ranges=[]),100)
        with patch("tof_geometry.time.time",return_value=1000.):
            g.ingest_rover(dict(status="success",target=A,distance_m=5.,age_ms=100,report_id=1),100)
            g.ingest(packet(seq=2,distance=5.1),100)
            g.ingest(packet(seq=3,distance=5.1),100.1)
        self.assertEqual(refinement.total_sessions,2)
        self.assertEqual(refinement.rows[A]["direction_counts"],{"rover_to_anchor":1,"anchor_to_rover":1})
    def test_refinement_preserves_direction_session_dedup_across_restart(self):
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/"rover.json"
            r=RoverRefinement(path)
            r.observe(A,5.,100,now=100,direction="anchor_to_rover",session_id="boot:1")
            r.save(100,force=True)
            r=RoverRefinement(path)
            self.assertFalse(r.observe(A,5.,100,now=101,direction="anchor_to_rover",session_id="boot:1"))
            self.assertTrue(r.observe(A,5.,100,now=101,direction="rover_to_anchor",session_id="boot:1"))

if __name__=="__main__":unittest.main()
