import math
import unittest
from tof_geometry import RangeGraph,triangle,trilaterate,horizontal

class GeometryTests(unittest.TestCase):
 def test_exact_triangle_and_rover(self):
  p,q=triangle(4,3,5);s=trilaterate(p,[math.sqrt(2),math.sqrt(10),math.sqrt(5)])
  self.assertAlmostEqual(s["x_m"],1);self.assertAlmostEqual(s["y_m"],1)
 def test_collinear_and_impossible_rejected(self):
  for edges in [(1,1,2),(1,1,4),(0,3,4)]:
   with self.assertRaises(ValueError):triangle(*edges)
 def test_height_projection(self):
  self.assertAlmostEqual(horizontal(5,3,0),4)
  with self.assertRaises(ValueError):horizontal(2,3,0)
 def test_graph_freshness_reboot_heights(self):
  g=RangeGraph(); ids=["aa:00:00:00:00:01","aa:00:00:00:00:02","aa:00:00:00:00:03"]
  xy=[(0,0),(4,0),(0,3)]
  for i,node in enumerate(ids):
   ranges=[]
   for j,target in enumerate(ids):
    if i!=j:ranges.append({"target_bssid":target,"distance_m":math.dist(xy[i],xy[j]),"valid":True,"status":"ok","age_ms":0})
   p={"schema":"rover.tof.anchor/1","bssid":node,"ftm_responder":True,"boot_id":1,"seq":1,"ranges":ranges,"height_known":True,"height_m":1}
   self.assertTrue(g.ingest(p,100))
   self.assertFalse(g.ingest(p,101))
   d=math.sqrt((xy[i][0]-1)**2+(xy[i][1]-1)**2+(1-.0635)**2)
   self.assertTrue(g.ingest_rover({"status":"success","target":node,"age_ms":0,"distance_m":d},100))
  s=g.snapshot(101);self.assertTrue(s["valid"]);self.assertAlmostEqual(s["position"]["x_m"],1)
  self.assertFalse(g.snapshot(131)["valid"])
  g.nodes[ids[0]]["packet"]["height_known"]=False
  self.assertEqual(g.snapshot(101)["status"],"mirror_ambiguous_3d")
 def test_failed_and_stale_do_not_become_ranges(self):
  g=RangeGraph()
  p={"schema":"rover.tof.anchor/1","bssid":"aa:00:00:00:00:01","ftm_responder":True,"boot_id":1,"seq":1,"ranges":[{"target_bssid":"aa:00:00:00:00:02","valid":True,"status":"ok","age_ms":40000,"distance_m":3}]}
  g.ingest(p,100);self.assertEqual(g.nodes[p["bssid"]]["ranges"],{})
 def test_inconsistent_rover_distances_reported(self):
  p,_=triangle(4,3,5)
  with self.assertRaises(ValueError):trilaterate(p,[.1,.1,20])
 def test_pending_and_failed_attempts_do_not_refresh_last_success(self):
  g=RangeGraph();a="aa:00:00:00:00:01";b="aa:00:00:00:00:02"
  p={"schema":"rover.tof.anchor/1","bssid":a,"ftm_responder":True,"boot_id":1,"seq":1,
     "ranges":[{"target_bssid":b,"valid":True,"status":"ok","age_ms":0,"distance_m":3}]}
  g.ingest(p,100)
  p["seq"]=2;p["ranges"]=[{"target_bssid":b,"valid":False,"status":"measuring"}]
  g.ingest(p,101);self.assertEqual(g.nodes[a]["ranges"][b]["observed"],100)
  p["seq"]=3;p["ranges"][0]["status"]="ftm_failed"
  g.ingest(p,110);self.assertEqual(g.nodes[a]["ranges"][b]["latest_attempt_status"],"ftm_failed")
  p["seq"]=4;g.ingest(p,131);self.assertNotIn(b,g.nodes[a]["ranges"])
  p["seq"]=5;p["ranges"]=[{"target_bssid":b,"valid":True,"status":"ok","age_ms":0,"distance_m":3}]
  g.ingest(p,200);p["boot_id"]=2;p["seq"]=1;p["ranges"]=[]
  g.ingest(p,201);self.assertEqual(g.nodes[a]["ranges"],{})
if __name__=="__main__":unittest.main()
