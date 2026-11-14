import unittest,itertools,math
import numpy as np
from tof_baseline import fit_geometry,lateration_certificate,AnchorBaseline
P={str(i):p for i,p in enumerate([(0,0,0),(4,0,0),(0,5,0),(0,0,3),(3,3,2),(1.2,1.4,.5)])}
def edges(points,skip=()):
 return [{"a":a,"b":b,"distance_m":math.dist(points[a],points[b]),"sigma_m":.25,"samples":6,"span_s":50,"last_observed_at":1050}
         for a,b in itertools.combinations(points,2) if (a,b) not in skip]
class SparseAnchorAdmission(unittest.TestCase):
 def test_sixth_anchor_needs_four_independent_links_not_all_pairs(self):
  measured=edges(P,[("4","5")]);fit=fit_geometry(list(P),measured,3)
  self.assertIsNotNone(fit);self.assertEqual(len(fit["positions"]),6)
  self.assertEqual(len(measured),14)
  for e in measured:self.assertAlmostEqual(math.dist(fit["positions"][e["a"]],fit["positions"][e["b"]]),e["distance_m"],places=4)
 def test_three_reference_links_stay_ambiguous(self):
  self.assertIsNone(fit_geometry(list(P),edges(P,[("3","5"),("4","5")]),3))
 def test_coplanar_references_have_no_lateration_certificate(self):
  points={**P,"3":(2,2,0),"4":(3,3,0)}
  self.assertFalse(lateration_certificate(list(points),edges(points,[("4","5")]),points))
 def test_inconsistent_triangle_is_not_rescued_by_feasibility_search(self):
  measured=edges(P);measured[0]["distance_m"]=100
  self.assertIsNone(fit_geometry(list(P),measured,3))
 def test_new_anchor_admission_preserves_existing_observable_frame(self):
  initial=fit_geometry(list(P)[:5],edges({k:P[k] for k in list(P)[:5]}),3)
  initial.update(epoch=1,frame_id="kept",created_at=900,revision=1)
  b=AnchorBaseline(clock=lambda:1060);b.fit=initial;b.epoch=1;b.spatial_dimension=3
  measured=edges(P,[("4","5")])
  for seq in range(6):
   for src in P:
    rows=[dict(e,target_bssid=e["b"],status="ok",valid=True,age_ms=0,range_seq=seq,samples=16) for e in measured if e["a"]==src]
    rows += [dict(e,target_bssid=e["a"],status="ok",valid=True,age_ms=0,range_seq=seq,samples=16) for e in measured if e["b"]==src]
    # The public parser requires MAC-shaped identities.
    mac=lambda v:"00:00:00:00:00:"+str(v).zfill(2)
    for row in rows:row["target_bssid"]=mac(row["target_bssid"])
    b.observe(dict(schema="rover.tof.anchor/1",bssid=mac(src),ftm_responder=True,boot_id=1,ranges=rows),1000+seq*10)
  b.fit["positions"]={("00:00:00:00:00:"+k.zfill(2)):v for k,v in b.fit["positions"].items()}
  for e in b.fit["edges"]:
   for k in ["a","b"]:e[k]="00:00:00:00:00:"+e[k].zfill(2)
  state=b.update(1060)
  self.assertEqual(len(state["positions"]),6);self.assertEqual(state["frame_id"],"kept")
 def test_recorded_noisy_six_anchor_graph_keeps_all_original_residual_gates(self):
  raw=[(0, 4, 2.4750000235, 0.34738996441759984), (0, 3, 5.962499976249999, 0.38918239362345), (2, 3, 6.150000095499999, 0.4447799295764998), (2, 4, 8.100000381, 0.44477887025880236), (0, 2, 3.975000143, 0.5559749992587006), (3, 4, 10.57499981, 2.5574848598942985), (1, 4, 7.650000094999999, 2.77987499962935), (1, 3, 9.450000286, 0.5559749992586986), (1, 2, 3.975000143, 1.11195), (0, 1, 7.4249999525, 1.2231450711647995), (0, 5, 1.200000048, 0.44477992883520007), (1, 5, 6.0, 0.44478028317659946), (4, 5, 9.75, 3.7806302816940005), (3, 5, 2.700000048, 0.4447799303177999)]
  measured=[dict(a=str(a),b=str(b),distance_m=d,sigma_m=s) for a,b,d,s in raw]
  fit=fit_geometry([str(i) for i in range(6)],measured,3);self.assertIsNotNone(fit)
  for e in fit["edges"]:
   self.assertLessEqual(abs(e["residual_m"]),max(.75,.12*e["distance_m"],2.5*e["sigma_m"]))

 def test_new_anchor_jointly_corrects_previously_estimated_coordinates(self):
  # The old five-anchor coordinates are deliberately wrong, not reference truth.
  import tempfile,json
  from pathlib import Path
  mac=lambda v:"00:00:00:00:00:"+str(v).zfill(2)
  truth={mac(k):v for k,v in P.items()}
  old={k:tuple(1.12*x for x in v) for k,v in truth.items() if k!=mac("5")}
  initial=fit_geometry(list(old),edges(old),3)
  initial.update(epoch=1,frame_id="relative",created_at=900,revision=1)
  old_distance=math.dist(initial["positions"][mac("0")],initial["positions"][mac("1")])
  with tempfile.TemporaryDirectory() as directory:
   path=Path(directory)/"baseline.json"
   # Even a leftover prior file must have no authority over self-localization.
   path.with_name("anchor-placement-priors.json").write_text(json.dumps(
    {"schema":"rover.tof.anchor-placement/1","anchors":{mac("5"):{"between":[mac("0"),mac("0")]}}}))
   b=AnchorBaseline(path,clock=lambda:1060);b.fit=initial;b.epoch=1;b.spatial_dimension=3
   measured=edges(truth)
   for seq in range(6):
    for src in truth:
     rows=[]
     for e in measured:
      if src not in (e["a"],e["b"]):continue
      dst=e["b"] if src==e["a"] else e["a"]
      rows.append(dict(target_bssid=dst,status="ok",valid=True,age_ms=0,range_seq=seq,samples=16,distance_m=e["distance_m"]))
     b.observe(dict(schema="rover.tof.anchor/1",bssid=src,ftm_responder=True,boot_id=1,ranges=rows),1000+10*seq)
   state=b.update(1060)
   points={p["bssid"]:[p["x_m"],p["y_m"],p["z_m"]] for p in state["positions"]}
   self.assertEqual(set(points),set(truth))
   self.assertEqual(state["frame_id"],"relative")
   self.assertGreater(abs(math.dist(points[mac("0")],points[mac("1")])-old_distance),.4)
   for e in measured:
    self.assertAlmostEqual(math.dist(points[e["a"]],points[e["b"]]),e["distance_m"],places=4)
   self.assertEqual({k for e in b.fit["edges"] for k in (e["a"],e["b"])},set(truth))
   self.assertNotIn("extensions",b.fit)
