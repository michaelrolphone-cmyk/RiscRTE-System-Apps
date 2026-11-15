import io,json,sqlite3,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tof_gateway_routes import GatewayRoutes,neighbor_hosts
from tof_anchor_transport import AnchorHTTPFallback
from tof_coordinator import http_json

MAC="14:63:93:91:86:89"
class RoutesTest(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
  self.path=Path(self.temp.name)/"routes.sqlite"
  with sqlite3.connect(self.path) as db:
   db.execute("CREATE TABLE routes(device_id TEXT PRIMARY KEY,host TEXT NOT NULL,revision INTEGER NOT NULL,observed_at REAL NOT NULL,identity TEXT NOT NULL)")
   db.execute("INSERT INTO routes VALUES(?,?,?,?,?)",("camera","192.168.1.4",1,0,"{}"))
  self.routes=GatewayRoutes(self.path)
 def test_camera_http_reads_current_identity_route_each_time(self):
  seen=[]
  def open_request(req,timeout):seen.append(req.full_url);return io.BytesIO(b'{}')
  with patch("urllib.request.urlopen",open_request):
   http_json("http://192.168.1.45:80","/api/telemetry",route_registry=self.path,device_id="camera")
   with sqlite3.connect(self.path) as db:db.execute("UPDATE routes SET host='192.168.1.22' WHERE device_id='camera'")
   http_json("http://192.168.1.45:80","/api/telemetry",route_registry=self.path,device_id="camera")
  self.assertEqual(seen,["http://192.168.1.4:80/api/telemetry","http://192.168.1.22:80/api/telemetry"])
 def test_anchor_recovers_moved_address_only_after_identity_verification(self):
  def fetch(ip):
   if ip=="192.168.1.70":raise OSError("old route")
   return {"schema":"rover.tof.anchor/1","bssid":MAC,"ftm_responder":True}
  transport=AnchorHTTPFallback(Path(self.temp.name)/"anchors.json",fetch=fetch,routes=self.routes,clock=lambda:10)
  transport.register(MAC,"192.168.1.70")
  with patch("tof_gateway_routes.neighbor_hosts",return_value=["192.168.1.3"]):self.assertTrue(transport.poll_once(MAC,"192.168.1.70"))
  self.assertEqual(transport.drain()[0][0]["ip"],"192.168.1.3")
  self.assertEqual(self.routes.get("ftm:"+MAC)["host"],"192.168.1.3")
 def test_wrong_neighbor_identity_cannot_rebind_anchor(self):
  transport=AnchorHTTPFallback(Path(self.temp.name)/"anchors.json",fetch=lambda _:{"schema":"rover.tof.anchor/1","bssid":"other","ftm_responder":True},routes=self.routes)
  transport.register(MAC,"192.168.1.70")
  with patch("tof_gateway_routes.neighbor_hosts",return_value=["192.168.1.3"]):self.assertFalse(transport.poll_once(MAC,"192.168.1.70"))
  self.assertIsNone(self.routes.get("ftm:"+MAC));self.assertEqual(transport.drain(),[])
 def test_neighbor_candidates_accept_both_esp_station_ap_patterns(self):
  text="? (192.168.1.3) at 14:63:93:91:86:88 on en1\n? (192.168.1.5) at 44:1b:f6:f7:90:50 on en1"
  with patch("subprocess.check_output",return_value=text):
   self.assertEqual(neighbor_hosts(MAC),["192.168.1.3"])
   self.assertEqual(neighbor_hosts("46:1b:f6:f7:90:50"),["192.168.1.5"])
if __name__=="__main__":unittest.main()
