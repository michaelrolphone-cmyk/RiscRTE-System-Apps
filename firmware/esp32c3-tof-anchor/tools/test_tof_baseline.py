import json
import math
from pathlib import Path
import tempfile
import unittest
import numpy as np
from tof_baseline import AnchorBaseline, fit_geometry, localize_relative
from tof_geometry import RangeGraph

IDS=["aa:00:00:00:00:%02x"%i for i in range(1,6)]
XYZ=np.array([[0,0,0],[4,0,0],[1,3,0],[.5,.7,2.8],[3,2,2]],float)

class Fixture:
    def __init__(self,path=None,points=XYZ,**kw):
        self.now=1000.;self.sequence=0;self.points=np.array(points,float)
        self.model=AnchorBaseline(path,clock=lambda:self.now,fit_interval_s=0,**kw)
    def packets(self,points=None,boot=1,missing=(),bias=None):
        self.sequence+=1;p=self.points if points is None else points;out=[]
        for i,src in enumerate(IDS[:len(p)]):
            if src in missing:continue
            ranges=[]
            for j,dst in enumerate(IDS[:len(p)]):
                if i==j:continue
                distance=float(np.linalg.norm(p[i]-p[j]))
                if bias:distance+=bias.get(tuple(sorted((src,dst))),0)
                ranges.append({"target_bssid":dst,"status":"ok","valid":True,"distance_m":distance,
                               "range_seq":self.sequence,"age_ms":0,"samples":12})
            out.append({"schema":"rover.tof.anchor/1","bssid":src,"boot_id":boot,"seq":self.sequence,
                        "ftm_responder":True,"ranges":ranges,"height_known":False})
        return out
    def tick(self,points=None,**kw):
        self.now+=10.
        for p in self.packets(points,**kw):self.model.observe(p)
        return self.model.update()
    def train(self):
        for _ in range(8):status=self.tick()
        return status

class BaselineTests(unittest.TestCase):
    def test_joint_3d_fit_all_five_and_frozen_noise(self):
        f=Fixture();s=f.train();self.assertTrue(s["valid"]);self.assertEqual(s["dimension"],3)
        self.assertEqual(len(s["positions"]),5);self.assertEqual(s["edge_count"],10)
        frozen=f.model.fit["positions"].copy();epoch=s["epoch"]
        rng=np.random.default_rng(4)
        for _ in range(20):
            f.now+=10
            for p in f.packets():
                for r in p["ranges"]:r["distance_m"]+=float(rng.normal(0,.15))
                f.model.observe(p)
            f.model.update()
        self.assertEqual(f.model.epoch,epoch);self.assertEqual(f.model.fit["positions"],frozen)

    def test_duplicate_success_is_not_new_evidence(self):
        f=Fixture();p=f.packets()[0]
        for i in range(20):
            f.now+=1;p["seq"]=i+1
            f.model.observe(p)
        self.assertEqual(sum(map(len,f.model.history.values())),4)
        self.assertFalse(f.model.update()["valid"])

    def test_invalid_stale_failed_and_unknown_session_ignored(self):
        f=Fixture();p=f.packets()[0]
        for r in p["ranges"]:r["status"]="ftm_failed"
        f.model.observe(p);self.assertFalse(f.model.history)
        for r in p["ranges"]:r.update(status="ok",age_ms=31000)
        f.model.observe(p);self.assertFalse(f.model.history)
        for r in p["ranges"]:r.update(age_ms=0,distance_m=float("nan"))
        f.model.observe(p);self.assertFalse(f.model.history)
        for r in p["ranges"]:r.update(distance_m=3,range_seq=None)
        f.model.observe(p);self.assertFalse(f.model.history)

    def test_restart_keeps_frame_fit_and_dedup(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"baseline.json";f=Fixture(path);before=f.train()
            restored=AnchorBaseline(path,clock=lambda:f.now+1)
            self.assertEqual(restored.status()["positions"],before["positions"])
            self.assertEqual(restored.status()["frame_id"],before["frame_id"])
            count=sum(map(len,restored.history.values()))
            # Same raw session after a collector restart is still one observation.
            p=f.packets();f.sequence-=1
            for packet in p:
                packet["seq"]-=1
                for r in packet["ranges"]:r["range_seq"]-=1
                restored.observe(packet)
            self.assertEqual(sum(map(len,restored.history.values())),count)

    def test_stationary_board_reboot_does_not_mean_movement(self):
        f=Fixture();before=f.train()
        for _ in range(20):s=f.tick(boot=2)
        self.assertEqual(s["epoch"],before["epoch"])
        self.assertTrue(s["valid"]);self.assertEqual(s["movement_anchors"],[])

    def test_offline_node_does_not_mean_movement(self):
        f=Fixture();before=f.train()
        for _ in range(20):s=f.tick(missing=[IDS[-1]])
        self.assertEqual(s["epoch"],before["epoch"]);self.assertTrue(s["valid"])
        self.assertFalse(any(e["type"]=="anchor_movement_detected" for e in f.model.events))

    def test_single_link_spike_does_not_mean_movement(self):
        f=Fixture();before=f.train()
        for _ in range(30):s=f.tick(bias={(IDS[0],IDS[1]):4.})
        self.assertEqual(s["epoch"],before["epoch"]);self.assertTrue(s["valid"])

    def test_major_move_invalidates_then_recalibrates_and_preserves_frame(self):
        f=Fixture();before=f.train();original=f.model.fit["positions"].copy()
        moved=XYZ.copy();moved[-1]+=[3.,2.,1.]
        states=[]
        for _ in range(40):states.append(f.tick(points=moved))
        self.assertTrue(any(s["status"]=="recalibrating" and not s["valid"] for s in states))
        after=states[-1];self.assertTrue(after["valid"]);self.assertEqual(after["epoch"],before["epoch"]+1)
        self.assertEqual(after["frame_id"],before["frame_id"])
        for node in IDS[:-1]:np.testing.assert_allclose(f.model.fit["positions"][node],original[node],atol=.05)
        displacement=np.linalg.norm(np.array(f.model.fit["positions"][IDS[-1]])-original[IDS[-1]])
        self.assertGreater(displacement,2.)
        event=next(e for e in f.model.events if e["type"]=="anchor_movement_detected")
        self.assertEqual(event["anchors"],[IDS[-1]])

    def test_reference_anchor_can_move_without_dragging_stationary_frame(self):
        f=Fixture();before=f.train();original=f.model.fit["positions"].copy()
        moved=XYZ.copy();moved[0]+=[-3,-2,-1]
        for _ in range(40):after=f.tick(points=moved)
        self.assertTrue(after["valid"]);self.assertGreater(after["epoch"],before["epoch"])
        for node in IDS[1:]:np.testing.assert_allclose(f.model.fit["positions"][node],original[node],atol=.05)

    def test_recalibration_survives_restart(self):
        with tempfile.TemporaryDirectory() as d:
            f=Fixture(Path(d)/"baseline.json");f.train();moved=XYZ.copy();moved[-1]+=[3,2,1]
            for _ in range(40):
                s=f.tick(points=moved)
                if s["status"]=="recalibrating":break
            self.assertEqual(s["status"],"recalibrating")
            restored=AnchorBaseline(f.model.path,clock=lambda:f.now)
            self.assertEqual(restored.status()["status"],"recalibrating")
            self.assertFalse(restored.status()["valid"])

    def test_clock_rollback_does_not_reuse_future_samples(self):
        with tempfile.TemporaryDirectory() as d:
            f=Fixture(Path(d)/"baseline.json");f.train()
            restored=AnchorBaseline(f.model.path,clock=lambda:1.)
            self.assertFalse(restored.history);self.assertFalse(restored.last_seen)

    def test_sparse_graph_does_not_invent_five_positions(self):
        f=Fixture()
        for _ in range(8):
            f.now+=10
            for p in f.packets():
                i=IDS.index(p["bssid"])
                p["ranges"]=[r for r in p["ranges"] if abs(IDS.index(r["target_bssid"])-i)==1]
                f.model.observe(p)
            s=f.model.update()
        self.assertFalse(s["valid"]);self.assertEqual(s["positions"],[])

    def test_planar_baseline_reports_intrinsic_plane_and_rover_ambiguity(self):
        planar=XYZ[:4].copy();planar[:,2]=0
        f=Fixture(points=planar);s=f.train();self.assertTrue(s["valid"]);self.assertEqual(s["dimension"],2)
        rover=np.array([1,1,1.5])
        obs=[{"target_bssid":k,"distance_m":float(np.linalg.norm(p-rover)),"observed":1000} for k,p in zip(IDS,planar)]
        pose=localize_relative(s,obs);self.assertFalse(pose["valid"])
        self.assertEqual(pose["status"],"mirror_ambiguous_3d")
        self.assertEqual(len(pose["position_hypotheses"]),2)

    def test_partial_baseline_is_explicit(self):
        f=Fixture()
        for _ in range(8):
            f.now+=10
            for p in f.packets():
                p["ranges"]=[r for r in p["ranges"] if p["bssid"] in IDS[:3] and r["target_bssid"] in IDS[:3]]
                f.model.observe(p)
            s=f.model.update()
        self.assertTrue(s["valid"]);self.assertEqual(s["status"],"partial")
        self.assertEqual(len(s["positions"]),3);self.assertEqual(set(s["unfitted_anchors"]),set(IDS[3:]))

    def test_corrupt_state_is_preserved_and_collects_again(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"baseline.json";path.write_text("{broken")
            f=Fixture(path);self.assertIsNotNone(f.model.load_error)
            s=f.train();self.assertTrue(s["valid"])
            self.assertEqual(path.with_suffix(".invalid.json").read_text(),"{broken")
            self.assertEqual(json.loads(path.read_text())["schema"],"rover.tof.baseline/1")

    def test_ring_is_bounded_and_expired_samples_pruned(self):
        f=Fixture()
        for i in range(150):
            f.now+=1
            for p in f.packets():f.model.observe(p)
        self.assertLessEqual(max(map(len,f.model.history.values())),120)
        f.now+=2000;f.model.update();self.assertFalse(f.model.history)

    def test_rover_uses_frozen_baseline_and_relative_3d(self):
        f=Fixture();s=f.train();g=RangeGraph(f.model)
        for p in f.packets():g.ingest(p,100)
        rover=np.array([1.3,1.2,1.1])
        for k,point in zip(IDS,XYZ):
            g.ingest_rover({"status":"success","target":k,"distance_m":float(np.linalg.norm(point-rover)),"age_ms":0},100)
        out=g.snapshot(101);self.assertTrue(out["valid"])
        self.assertEqual(out["position"]["baseline_epoch"],s["epoch"]);self.assertFalse(out["navigation_authority"])
        self.assertEqual(out["frame"]["world_alignment"],"unregistered")
        self.assertFalse(g.snapshot(131)["valid"])

    def test_bad_rover_ranges_do_not_change_baseline(self):
        f=Fixture();s=f.train();before=f.model.fit["positions"].copy()
        observations=[{"target_bssid":k,"distance_m":.01 if i==0 else 20.,"observed":1000+i} for i,k in enumerate(IDS)]
        pose=localize_relative(s,observations);self.assertFalse(pose["valid"])
        self.assertEqual(f.model.fit["positions"],before)


    def test_two_moved_nodes_do_not_relocate_stationary_neighbours(self):
        f=Fixture();before=f.train();original=f.model.fit["positions"].copy()
        moved=XYZ.copy();moved[3]+=[-3,2,2];moved[4]+=[3,2,1]
        for _ in range(45):s=f.tick(points=moved)
        self.assertTrue(s["valid"]);self.assertGreater(s["epoch"],before["epoch"])
        event=next(e for e in f.model.events if e["type"]=="anchor_movement_detected")
        self.assertEqual(set(event["anchors"]),set(IDS[3:]))
        self.assertNotEqual(s["frame_id"],before["frame_id"])
        for node in IDS[:3]:np.testing.assert_allclose(f.model.fit["positions"][node],original[node],atol=.05)

    def test_network_change_recalibrates_with_new_frame_when_controls_lost(self):
        f=Fixture();before=f.train();moved=XYZ*1.8
        invalid=False
        for _ in range(45):
            s=f.tick(points=moved);invalid |= not s["valid"]
        self.assertTrue(invalid);self.assertTrue(s["valid"])
        self.assertNotEqual(s["frame_id"],before["frame_id"])
        self.assertTrue(any(e["type"]=="baseline_geometry_change_detected" for e in f.model.events))

    def test_no_false_confidence_for_a_3d_hinge(self):
        # Two tetrahedra sharing a triangle can reflect independently.
        points=XYZ.copy()
        edges=[]
        for i in range(5):
            for j in range(i+1,5):
                if (i,j)==(3,4):continue
                edges.append({"a":IDS[i],"b":IDS[j],"distance_m":float(np.linalg.norm(points[i]-points[j])),"sigma_m":.25})
        self.assertIsNone(fit_geometry(IDS,edges,3))


    def test_repeating_a_changed_window_without_new_sessions_cannot_confirm_move(self):
        f=Fixture();before=f.train();moved=XYZ.copy();moved[-1]+=[3,2,1]
        for _ in range(40):
            s=f.tick(points=moved)
            if s["status"]=="movement_suspected":break
        self.assertEqual(s["status"],"movement_suspected")
        for _ in range(30):
            f.now+=5;s=f.model.update()
        self.assertEqual(s["epoch"],before["epoch"])
        self.assertFalse(any(e["type"]=="anchor_movement_detected" for e in f.model.events))

if __name__=="__main__":unittest.main()
