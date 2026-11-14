import copy
import json
import math
from pathlib import Path
import tempfile
import unittest
import numpy as np
from tof_baseline import AnchorBaseline
from tof_config import SCHEMA, validate, replace_configuration, read_configuration, anchored_fit

IDS = ["02:00:00:00:00:%02x" % i for i in range(1, 7)]
POINTS = [[0,0,1],[-2,4,0],[0,5,.3],[-1,9,.4]]
def document(points=POINTS):
    return dict(schema=SCHEMA, frame_id="test-house", axes="east_north_up", units="m",
                anchors=[dict(bssid=k, label=str(i), position=list(p)) for i,(k,p) in enumerate(zip(IDS,points))])
def edges(points):
    return [dict(a=a,b=b,distance_m=math.dist(pa,pb),sigma_m=.25,samples=20,span_s=40)
            for i,(a,pa) in enumerate(points.items()) for b,pb in list(points.items())[i+1:]]

class RuntimeGeometryTests(unittest.TestCase):
    def test_fixed_points_are_exact_despite_conflicting_radio(self):
        with tempfile.TemporaryDirectory() as d:
            b=AnchorBaseline(Path(d)/"baseline.json", clock=lambda:1000)
            replace_configuration(Path(d)/"geometry-config.json",document(),None)
            for i,k in enumerate(IDS[:4]): b.last_seen[k]=1000
            b._edges=lambda now: [dict(e,distance_m=e["distance_m"]+3) for e in edges(dict(zip(IDS,POINTS)))]
            out=b.update()
            self.assertTrue(out["valid"])
            self.assertEqual({p["bssid"]:[p["x_m"],p["y_m"],p["z_m"]] for p in out["positions"]},dict(zip(IDS,POINTS)))
            self.assertGreater(out["residual_rms_m"],2.9)
            self.assertTrue(all(p["position_source"]=="measured_fixed" for p in out["positions"]))

    def test_unknown_node_is_solved_and_fixed_nodes_never_move(self):
        fixed=dict(zip(IDS,POINTS)); allpoints=dict(fixed);allpoints[IDS[4]]=[3,2,2]
        fit=anchored_fit(fixed,list(allpoints),edges(allpoints))
        self.assertLess(math.dist(fit["positions"][IDS[4]],allpoints[IDS[4]]),1e-7)
        for k,p in fixed.items():self.assertEqual(fit["positions"][k],p)

    def test_insufficient_or_planar_links_do_not_invent_unknown_position(self):
        fixed=dict(zip(IDS,[[0,0,0],[2,0,0],[0,2,0],[2,2,0]]))
        allpoints=dict(fixed);allpoints[IDS[4]]=[1,1,2]
        fit=anchored_fit(fixed,list(allpoints),edges(allpoints))
        self.assertNotIn(IDS[4],fit["positions"])
        fixed=dict(zip(IDS,POINTS));allpoints=dict(fixed);allpoints[IDS[4]]=[1,1,2]
        fit=anchored_fit(fixed,list(allpoints),[e for e in edges(allpoints) if e["a"]!=IDS[0] and e["b"]!=IDS[0]])
        self.assertNotIn(IDS[4],fit["positions"])

    def test_live_replace_add_remove_and_frame_invalidation_without_restart(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"geometry-config.json"; b=AnchorBaseline(Path(d)/"baseline.json",clock=lambda:1000)
            rev=replace_configuration(p,document(),None);one=b.update()
            new=document();new["anchors"][0]["position"]=[8,7,6]
            new["anchors"].append(dict(bssid=IDS[4],position=[4,3,2]))
            rev=replace_configuration(p,new,rev);two=b.update()
            self.assertNotEqual(one["frame_id"],two["frame_id"])
            self.assertGreater(two["epoch"],one["epoch"])
            self.assertEqual(len(two["fixed_anchors"]),5)
            self.assertEqual(next(a["x_m"] for a in two["positions"] if a["bssid"]==IDS[0]),8)
            new["anchors"]=new["anchors"][1:]
            replace_configuration(p,new,rev);three=b.update()
            self.assertNotIn(IDS[0],three["fixed_anchors"])

    def test_inches_units_and_restart_load(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"geometry-config.json";doc=document();doc["units"]="in"
            replace_configuration(p,doc,None)
            b=AnchorBaseline(Path(d)/"baseline.json",clock=lambda:1000);out=b.update();b.save(force=True)
            expected={k:[v*.0254 for v in pt] for k,pt in zip(IDS,POINTS)}
            self.assertEqual({a["bssid"]:[a["x_m"],a["y_m"],a["z_m"]] for a in out["positions"]},expected)
            restart=AnchorBaseline(Path(d)/"baseline.json",clock=lambda:1001).update()
            self.assertEqual(restart["positions"],out["positions"])

    def test_missing_or_empty_config_preserves_unconfigured_solver(self):
        with tempfile.TemporaryDirectory() as d:
            b=AnchorBaseline(Path(d)/"baseline.json",clock=lambda:1000)
            self.assertEqual(b.update()["status"],"collecting")
            p=Path(d)/"geometry-config.json"
            rev=replace_configuration(p,document(),None);self.assertTrue(b.update()["valid"])
            doc=document();doc["anchors"]=[]
            replace_configuration(p,doc,rev)
            self.assertFalse(b.update()["valid"])
            self.assertIsNone(b.fit)

    def test_malformed_file_is_visible_and_never_exposes_old_fix(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"geometry-config.json";replace_configuration(p,document(),None)
            b=AnchorBaseline(Path(d)/"baseline.json",clock=lambda:1000);self.assertTrue(b.update()["valid"])
            p.write_text("{")
            out=b.update()
            self.assertFalse(out["valid"]);self.assertEqual(out["positions"],[])
            self.assertEqual(out["status"],"geometry_configuration_error")
            p.write_text(json.dumps(document()))
            self.assertTrue(b.update()["valid"])

    def test_changed_module_discards_only_its_old_ranging_history(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"geometry-config.json";rev=replace_configuration(p,document(),None)
            b=AnchorBaseline(Path(d)/"baseline.json",clock=lambda:1000);b.update()
            changed_key=IDS[0]+">"+IDS[1];stable_key=IDS[1]+">"+IDS[2]
            b.history={changed_key:[[999,"a",4,8,999]],stable_key:[[999,"b",4,8,999]]}
            doc=document();doc["anchors"][0]["position"]=[1,2,3]
            replace_configuration(p,doc,rev);b.update()
            self.assertNotIn(changed_key,b.history)
            self.assertIn(stable_key,b.history)

    def test_configuration_validation_and_cas(self):
        for mutate in [lambda d:d.update(units="feet"),
                       lambda d:d["anchors"].append(copy.deepcopy(d["anchors"][0])),
                       lambda d:d["anchors"][0].update(position=[True,0,0]),
                       lambda d:d["anchors"][0].update(position=[float("nan"),0,0]),
                       lambda d:d["anchors"][0].update(bssid="room-name")]:
            doc=document();mutate(doc)
            with self.assertRaises(ValueError):validate(doc)
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"geometry-config.json";rev=replace_configuration(p,document(),None)
            with self.assertRaises(ValueError):replace_configuration(p,document(),None)
            self.assertEqual(read_configuration(p)[1],rev)

if __name__=="__main__":unittest.main()
