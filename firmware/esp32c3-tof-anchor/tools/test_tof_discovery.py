import json,unittest
from tof_discovery import discovery_records,publish_responders

class ResponderDiscoveryTests(unittest.TestCase):
 def fixture(self):
  return {"anchors":[{"bssid":"14:63:93:91:86:89","ssid":"C3-TOF-A","channel":9,
   "ip":"192.168.1.70","received_age_s":1,"ranges":[
    {"target_bssid":"2A:84:85:4B:57:98","ssid":"C3-TOF-ROVER-R","channel":9,
     "seen_age_ms":1000,"valid":False,"status":"measuring","distance_m":99}]}]}
 def test_rover_discovered_without_local_scan_or_successful_range(self):
  rows,hosts=discovery_records(self.fixture())
  self.assertEqual({r["bssid"] for r in rows},{"14:63:93:91:86:89","2a:84:85:4b:57:98"})
  self.assertEqual(hosts,["192.168.1.70"])
 def test_only_identity_is_announced(self):
  rows,_=discovery_records(self.fixture())
  for row in rows:
   self.assertEqual(set(row),{"schema","bssid","ssid","channel","ftm_responder"})
   self.assertLess(len(json.dumps(row).encode()),250)
 def test_stale_source_and_stale_peer_are_not_reannounced(self):
  data=self.fixture();data["anchors"][0]["received_age_s"]=31
  self.assertEqual(discovery_records(data),([],[]))
  data=self.fixture();data["anchors"][0]["ranges"][0]["seen_age_ms"]=60001
  self.assertEqual(len(discovery_records(data)[0]),1)
 def test_bad_identity_channel_or_prefix_rejected(self):
  for field,value in [("target_bssid","bad"),("channel",0),("channel",True),("ssid","unrelated")]:
   data=self.fixture();data["anchors"][0]["ranges"][0][field]=value
   self.assertEqual(len(discovery_records(data)[0]),1)
 def test_duplicate_reports_do_not_duplicate_peers(self):
  data=self.fixture();data["anchors"]*=2
  rows,hosts=discovery_records(data)
  self.assertEqual(len(rows),2);self.assertEqual(len(hosts),1)
 def test_sends_complete_packets_and_reports_delivery_errors(self):
  class Socket:
   def __init__(self,fail=False):self.rows=[];self.fail=fail
   def sendto(self,payload,target):
    if self.fail:raise OSError("offline")
    self.rows.append((json.loads(payload),target))
  sock=Socket();status=publish_responders(sock,self.fixture(),7446)
  self.assertEqual(status["sent"],2);self.assertEqual(len(sock.rows),2)
  self.assertEqual(publish_responders(Socket(True),self.fixture(),7446)["errors"],2)
 def test_bad_endpoint_not_contacted(self):
  data=self.fixture();data["anchors"][0]["ip"]="bad"
  self.assertEqual(discovery_records(data)[1],[])
