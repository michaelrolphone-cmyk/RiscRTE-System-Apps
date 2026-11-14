import math
import unittest

from tof_config import empty_document, validate
from tof_ground import fit_ground, ground_observations, radio_calibration, surveyed_floor
from tof_rover import RoverRefinement


class GroundTest(unittest.TestCase):
    def setUp(self):
        self.points = {"a": [0., 0., .7], "b": [4., 0., .3], "c": [0., 5., .4], "d": [3., 4., 0.]}
        self.floor = surveyed_floor({"ground_plane": {"elevation": 10}}, .0254, "survey", self.points)
        self.baseline = dict(ground_plane=self.floor, frame_id="survey", epoch=1, revision=2,
                             positions=[dict(bssid=k, x_m=p[0], y_m=p[1], z_m=p[2]) for k,p in self.points.items()])

    def test_ground_slant_fit_and_unobservable_inputs(self):
        expected = [1., 2., .254+.064]
        rows = []
        for k, p in self.points.items():
            r = math.dist(p, expected)
            rows.append(dict(target_bssid=k, distance_m=r, raw_distance_m=r, correction_m=0))
        result = fit_ground(self.baseline, rows[:3], .064)
        self.assertTrue(result["valid"])
        for axis, value in zip("xyz", expected):
            self.assertAlmostEqual(result["position"][axis+"_m"], value)
        self.assertFalse(fit_ground(self.baseline, rows[:2], .064)["valid"])
        self.baseline["positions"][2]["y_m"] = 0.
        self.assertEqual(fit_ground(self.baseline, rows[:3], .064)["status"], "unobservable_ground_geometry")

    def test_directional_holdout_and_drift(self):
        history = {}
        for a,b in [("a","b"), ("a","c"), ("a","d"), ("c","a")]:
            actual = math.dist(self.points[a],self.points[b])
            history[a+">"+b] = [[t, str(t), actual+(1. if t<80 else 1.5), 12, t]
                                  for t in [55, 60, 65, 81, 88, 97]]
        cal = radio_calibration(history, self.points, 100, 20)
        self.assertAlmostEqual(cal["links"][0]["drift_m"], .5)
        self.assertAlmostEqual(cal["corrections"]["a"]["anchor_to_rover"]["subtract_m"], 1.5)
        self.assertNotIn("rover_to_anchor", cal["corrections"]["a"])
        pair = dict(target_bssid="a", method="reciprocal_median",
                    directional_measurements=[dict(direction="anchor_to_rover", distance_m=6.5, observed=99),
                                              dict(direction="rover_to_anchor", distance_m=5., observed=99)])
        self.assertAlmostEqual(ground_observations([pair], cal, False)[0]["distance_m"], 5.)
        self.assertFalse(radio_calibration(history, self.points, 1000, 20)["links"])

    def test_multipath_is_not_transferred(self):
        hist = {"a>"+k:[[i, i, math.dist(self.points["a"],self.points[k])+bias, 12, i] for i in range(5)]
                for k,bias in [("b",1.),("c",-1.),("d",.1)]}
        self.assertFalse(radio_calibration(hist, self.points, 5)["corrections"])

    def test_pickup_without_visual_tracking(self):
        r = RoverRefinement()
        r.observe_visual(dict(last_pickup_at=100., tracking=False), now=101.)
        self.assertEqual(r.last_pickup, 100.)
        self.assertEqual(r.reason, "pickup")

    def test_invalid_configuration(self):
        for plane in [False, {"elevation": float("nan")}, {"elevation": True}]:
            with self.assertRaises(ValueError):
                validate(dict(empty_document(), ground_plane=plane))
        with self.assertRaises(ValueError):
            validate(dict(empty_document(), ground_plane={"elevation":0}, rover_ground_contact={"grounded": True}))

    def test_rover_held_out_geometry_rejects_wrong_transfer(self):
        from tof_ground import validate_transfers
        exact = [1., 2., .318]
        rows = []
        for key, p in self.points.items():
            raw = math.dist(p, exact)
            rows.append(dict(target_bssid=key, raw_distance_m=raw, distance_m=raw, correction_m=0.))
        rows[0].update(distance_m=rows[0]["raw_distance_m"]-.5, correction_m=.5)
        self.assertEqual(validate_transfers(self.baseline, rows, .064)[0]["transfer_check"], "unconfirmed")
        rows[0].update(distance_m=rows[0]["raw_distance_m"], raw_distance_m=rows[0]["raw_distance_m"]+.5)
        self.assertEqual(validate_transfers(self.baseline, rows, .064)[0]["transfer_check"], "supported")

    def test_identical_geometry_keeps_registration_revision(self):
        import json
        import tempfile
        from pathlib import Path
        from tof_baseline import AnchorBaseline
        with tempfile.TemporaryDirectory() as directory:
            cfg = Path(directory)/"geometry-config.json"
            doc = empty_document()
            doc["anchors"] = [dict(bssid="02:00:00:00:00:01", position=[0,0,1])]
            cfg.write_text(json.dumps(doc))
            baseline = AnchorBaseline(geometry_path=cfg)
            first = baseline.update()
            second = baseline.update()
            self.assertEqual(first["revision"], second["revision"])
