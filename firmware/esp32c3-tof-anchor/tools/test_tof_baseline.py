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
        self.assertEqual(f.model.epoch,epoch)
        self.assertLess(max(np.linalg.norm(np.array(f.model.fit["positions"][k])-v) for k,v in frozen.items()),.5)

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
        after=states[-1];self.assertTrue(after["valid"]);self.assertGreater(after["epoch"],before["epoch"])
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


    def test_refinement_corrects_initial_estimate_with_bounded_steps(self):
        f=Fixture();before=f.train();node=IDS[-1]
        f.model.fit["positions"][node]=(np.array(f.model.fit["positions"][node])+[.35,-.2,.1]).tolist()
        def error():
            points=f.model.fit["positions"]
            return np.sqrt(np.mean([(np.linalg.norm(np.array(points[IDS[i]])-points[IDS[j]])-np.linalg.norm(XYZ[i]-XYZ[j]))**2 for i in range(5) for j in range(i+1,5)]))
        initial=error();old=f.model.fit["positions"].copy()
        for _ in range(50):
            status=f.tick()
            step=max(np.linalg.norm(np.array(f.model.fit["positions"][k])-old[k]) for k in IDS)
            self.assertLessEqual(step,.100001);old=json.loads(json.dumps(f.model.fit["positions"]))
        self.assertGreater(status["refinement"]["accepted"],2)
        self.assertLess(error(),initial*.2)
        self.assertEqual(status["epoch"],before["epoch"]);self.assertEqual(status["frame_id"],before["frame_id"])
        self.assertGreater(status["revision"],1)
        self.assertLess(status["refinement"]["after_rms_m"],status["refinement"]["before_rms_m"])
        self.assertLessEqual(status["refinement"]["validation_after_m"],status["refinement"]["validation_before_m"]+.02)

    def test_refinement_improves_after_biased_initial_samples_are_outnumbered(self):
        f=Fixture()
        for _ in range(8):f.tick(bias={(IDS[0],IDS[1]):.4,(IDS[2],IDS[4]):.3})
        initial=f.model.status()
        for _ in range(50):s=f.tick()
        self.assertGreater(s["refinement"]["accepted"],0)
        self.assertEqual(s["epoch"],initial["epoch"])

    def test_refinement_survives_restart_and_keeps_movement_reference(self):
        with tempfile.TemporaryDirectory() as d:
            f=Fixture(Path(d)/"baseline.json");f.train()
            reference=json.loads(json.dumps(f.model.fit["edges"]))
            f.model.fit["positions"][IDS[-1]][0]+=.3
            for _ in range(12):s=f.tick()
            self.assertGreater(s["refinement"]["accepted"],0)
            self.assertEqual(f.model.fit["movement_reference_edges"],reference)
            restored=AnchorBaseline(f.model.path,clock=lambda:f.now)
            self.assertEqual(restored.status()["revision"],s["revision"])
            self.assertEqual(restored.refinement,f.model.refinement)
            self.assertEqual(restored.fit["movement_reference_edges"],reference)

    def test_no_new_sessions_cannot_refine(self):
        f=Fixture();s=f.train();f.model.fit["positions"][IDS[-1]][0]+=.3
        frozen=json.loads(json.dumps(f.model.fit["positions"]))
        for _ in range(10):
            f.now+=30
            for node in IDS:f.model.last_seen[node]=f.now
            f.model.update()
        self.assertEqual(f.model.fit["positions"],frozen)
        self.assertEqual(f.model.refinement["accepted"],0)

    def test_full_ring_keeps_refining_on_new_session_tokens(self):
        f=Fixture();f.train()
        for _ in range(135):f.tick()
        count=sum(map(len,f.model.history.values()))
        self.assertEqual(count,20*120)
        f.model.fit["positions"][IDS[-1]][0]+=.3
        accepted=f.model.refinement["accepted"]
        for _ in range(15):s=f.tick()
        self.assertEqual(sum(map(len,f.model.history.values())),count)
        self.assertGreater(s["refinement"]["accepted"],accepted)

    def test_offline_refinement_is_paused(self):
        f=Fixture();f.train();f.model.fit["positions"][IDS[-1]][0]+=.3
        for _ in range(12):s=f.tick(missing=[IDS[-1]])
        accepted=s["refinement"]["accepted"]
        frozen=json.loads(json.dumps(f.model.fit["positions"]))
        for _ in range(10):s=f.tick(missing=[IDS[-1]])
        self.assertEqual(s["refinement"]["accepted"],accepted)
        self.assertEqual(f.model.fit["positions"],frozen)
        self.assertEqual(s["refinement"]["state"],"awaiting_online_anchors")


    def test_recalibration_recovers_stationary_subset_when_moved_node_disappears(self):
        f=Fixture();before=f.train();moved=XYZ.copy();moved[-1]+=[3,2,1]
        for _ in range(40):
            s=f.tick(points=moved)
            if s["status"]=="recalibrating":break
        self.assertEqual(s["status"],"recalibrating")
        for _ in range(12):s=f.tick(points=moved,missing=[IDS[-1]])
        self.assertTrue(s["valid"]);self.assertEqual(len(s["positions"]),4)
        self.assertNotIn(IDS[-1],f.model.fit["positions"])
        self.assertGreater(s["epoch"],before["epoch"])


    def test_expansion_of_known_3d_baseline_cannot_silently_fall_back_to_plane(self):
        from unittest.mock import patch
        import tof_baseline
        f=Fixture(points=XYZ[:4]);f.train();f.points=XYZ.copy()
        original=tof_baseline.fit_geometry
        def candidate(nodes,edges,dimension):
            if len(nodes)==5:
                self.assertEqual(dimension,3);return None
            return original(nodes,edges,dimension)
        with patch('tof_baseline.fit_geometry',side_effect=candidate):
            for _ in range(10):s=f.tick()
        self.assertEqual(s["dimension"],3);self.assertEqual(len(s["positions"]),4)
        self.assertEqual(s["status"],"partial")

    def test_loaded_planar_downgrade_revalidates_against_previous_3d_evidence(self):
        with tempfile.TemporaryDirectory() as d:
            f=Fixture(Path(d)/"baseline.json");f.train()
            f.model.previous=[{"positions":json.loads(json.dumps(f.model.fit["positions"]))}]
            f.model.fit["dimension"]=2;f.model.fit["positions"]={k:[v[0],v[1],0.] for k,v in f.model.fit["positions"].items()}
            f.model.spatial_dimension=2;f.model.dirty=True;f.model.save(force=True)
            restored=AnchorBaseline(f.model.path,clock=lambda:f.now,fit_interval_s=0)
            self.assertEqual(restored.spatial_dimension,3)
            previous_frame=restored.fit["frame_id"];status=restored.update()
            self.assertEqual(status["dimension"],3)
            self.assertNotEqual(status["frame_id"],previous_frame)

    def test_tilted_anchor_plane_has_correct_mirror_rover_solutions(self):
        angle=.7;rotation=np.array([[1,0,0],[0,math.cos(angle),-math.sin(angle)],[0,math.sin(angle),math.cos(angle)]])
        planar=XYZ[:4].copy();planar[:,2]=0;f=Fixture(points=planar);s=f.train()
        offset=np.array([2.,-1.,3.])
        for p in s["positions"]:
            transformed=np.array([p["x_m"],p["y_m"],p["z_m"]]) @ rotation.T+offset
            p.update(x_m=float(transformed[0]),y_m=float(transformed[1]),z_m=float(transformed[2]))
        # Work entirely in the fitted planar frame, then tilt both anchors and rover.
        f0=f.model.fit["positions"];plain_rover=np.array([1.,1.,1.5])
        obs=[{"target_bssid":k,"distance_m":float(np.linalg.norm(np.array(v)-plain_rover)),"observed":1000} for k,v in f0.items()]
        result=localize_relative(s,obs);self.assertEqual(result["status"],"mirror_ambiguous_3d")
        expected=plain_rover @ rotation.T+offset
        self.assertLess(min(np.linalg.norm(np.array([p["x_m"],p["y_m"],p["z_m"]])-expected) for p in result["position_hypotheses"]),1e-5)

if __name__=="__main__":unittest.main()
