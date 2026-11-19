from argparse import Namespace
import contextlib, io, json, queue, tempfile, threading, time, unittest
from pathlib import Path
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from biomerte.host_io_client import ClockBounds
import tof_host_io
from tof_gateway_routes import GatewayRoutes
from tof_coordinator import http_json
from tof_dual_radio import DualRadioRanging
from tof_magnetic_reference import profile_request
from tof_inertial import VisionFeed

class FakeHost:
    def __init__(self): self.calls=[]
    def route(self,device_id):
        self.calls.append(("route",device_id));return {"host":"192.0.2.7","device_id":device_id}
    def origin(self,url,device_id):
        self.calls.append(("origin",url,device_id));return "http://192.0.2.7"
    def candidates(self,bssid):
        self.calls.append(("candidates",bssid));return ["192.0.2.7"]
    def observe_anchor(self,bssid,host): self.calls.append(("observe",bssid,host))
    def device_request(self,device_id,path,payload=None):
        self.calls.append(("request",device_id,path,payload));return {"ok":True}

class PortabilityTests(unittest.TestCase):
    def tearDown(self):
        tof_host_io._client=None
        tof_host_io.configure_survey()

    def test_remote_routes_never_open_sqlite_or_arp(self):
        host=FakeHost();tof_host_io._client=host
        with patch("sqlite3.connect",side_effect=AssertionError("SQLite crossed boundary")), \
                patch("subprocess.check_output",side_effect=AssertionError("ARP crossed boundary")):
            routes=GatewayRoutes("/not-mounted/device-routes.sqlite3")
            self.assertEqual(routes.get("camera")["host"],"192.0.2.7")
            self.assertEqual(routes.origin("http://stale.invalid","camera"),"http://192.0.2.7")
            self.assertEqual(routes.candidates("aa:bb:cc:dd:ee:ff"),["192.0.2.7"])
            routes.observe_anchor("aa:bb:cc:dd:ee:ff","192.0.2.7")
        self.assertEqual(len(host.calls),4)

    def test_transitive_operations_preserve_device_identity_and_payload(self):
        host=FakeHost();tof_host_io._client=host
        radio=DualRadioRanging.__new__(DualRadioRanging)
        radio.rear_id="registered-rear";radio.directory=Path("/not-mounted")
        payload={"address":"aa:bb:cc:dd:ee:ff","channel":6}
        with patch("sqlite3.connect",side_effect=AssertionError("SQLite crossed boundary")), \
                patch("http.client.HTTPConnection",side_effect=AssertionError("credentials copied")):
            radio.request(payload["address"],payload["channel"])
            profile_request(Path("/not-mounted"),"registered-front",{"reference_id":"existing-zero"})
            http_json("http://stale.invalid","/api/telemetry/ftm",payload,
                      device_id="registered-front",host_io=host)
        self.assertEqual(host.calls,[("request","registered-rear","/api/telemetry/ftm",payload),
            ("request","registered-front","/api/ahrs/calibration",{"reference_id":"existing-zero"}),
            ("request","registered-front","/api/telemetry/ftm",payload)])

    def test_raw_output_returns_to_native_epoch_without_changing_utc_state(self):
        bounds=ClockBounds("native-boot",900.,900.02,100.,clock=lambda:100.,host_id="host")
        host=type("Host",(),{"clock":lambda self:bounds})()
        source={"rover_ranges":[{"observed":99.5,"directional_measurements":[{"observed":99.4}]}],
                "rover_directional_ranges":[{"observed":99.3}],
                "rover_refinement":{"observed":1795120000.},"baseline":{"created_at":1795110000.}}
        out=tof_host_io.native_output_clock(source,host)
        self.assertEqual(source["rover_ranges"][0]["observed"],99.5)
        self.assertEqual(out["rover_ranges"][0]["observed"],999.5)
        self.assertEqual(out["rover_ranges"][0]["observed_host_monotonic_interval_s"],[999.5,999.52])
        self.assertEqual(out["rover_refinement"],source["rover_refinement"])
        self.assertEqual(out["baseline"],source["baseline"])
        self.assertEqual(out["transport_clock"]["clock_id"],"native-boot")

    def test_persisted_motion_uses_utc_not_container_uptime(self):
        self.assertEqual(tof_host_io.wall_observed(99.5,now_monotonic=100.,now_wall=1795120000.),
                         1795119999.5)

    def test_translated_stale_vision_interval_is_not_reclassified_fresh(self):
        feed=VisionFeed(None,"/not-mounted/vision.sock");now=time.monotonic()
        base={"sensor_tick":True,"_host_io_clock":{"qualified":True}}
        feed._ranging(dict(base,ranging_observation={"capture_host_monotonic_interval_s":[now-3,now-2.9]}))
        self.assertIsNone(feed.take_ranging())
        feed._ranging(dict(base,ranging_observation={"capture_host_monotonic_interval_s":[now-.1,now-.09]}))
        self.assertIsNotNone(feed.take_ranging())

    def test_anchor_gap_resets_partial_packet_assembly(self):
        bridge=tof_host_io.AnchorSocket.__new__(tof_host_io.AnchorSocket)
        bridge.inbox=queue.Queue();bridge.timeout=.1;bridge.last=None
        for seq in (1,3):
            bridge.inbox.put(dict(payload=b"fragment",peer=("192.0.2.7",7446),
                received_interval_s=[99.,99.02],sequence=seq,source_clock_id="native-boot"))
        self.assertEqual(bridge.recvfrom(16384),(b"fragment",("192.0.2.7",7446)))
        self.assertFalse(bridge.reset_required)
        bridge.recvfrom(16384)
        self.assertTrue(bridge.reset_required)
        self.assertEqual(bridge.received_at,99.)

    def test_preparation_is_read_only_and_never_reports_active(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);state=root/"tof";vision=root/"vision";state.mkdir();vision.mkdir()
            for n in ("baseline.json","geometry-config.json","anchor-registry.json","rover-refinement.json"):
                (state/n).write_text("{}")
            for n in ("config.json","camera-rig.json","survey-world.json"):(vision/n).write_text("{}")
            before={str(p.relative_to(root)):p.read_bytes() for p in root.rglob("*.json")}
            args=Namespace(state=str(state),vision_state=str(vision),camera_id="camera",
                           check=True,health_port=0,health_listen="127.0.0.1")
            with patch.object(tof_host_io,"AnchorSocket",side_effect=AssertionError("UDP owner started")), \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                tof_host_io.prepare(args)
            receipt=json.loads(output.getvalue())
            self.assertFalse(receipt["ready"]);self.assertEqual(receipt["phase"],"preparing")
            self.assertEqual(before,{str(p.relative_to(root)):p.read_bytes() for p in root.rglob("*.json")})

    def test_prepared_liveness_is_distinct_from_readiness(self):
        health=tof_host_io.RuntimeHealth(0)
        try:
            url="http://127.0.0.1:"+str(health.server.server_port)
            with urlopen(url+"/healthz",timeout=2) as r:self.assertTrue(json.load(r)["live"])
            with self.assertRaises(HTTPError) as e:urlopen(url+"/readyz",timeout=2)
            self.assertEqual(e.exception.code,503)
        finally:health.close()

    def test_survey_control_preserves_authenticated_request_and_world(self):
        calls=[]
        class Pose:
            def zero(self,nav,world,request,now):
                calls.append((world,request));return {"ok":True,"reset_id":request}
        nav=type("Nav",(),{"lock":threading.Lock()})()
        with tempfile.TemporaryDirectory() as name:
            token=Path(name)/"token";token.write_text("isolated-test-token-"*4);token.chmod(0o600)
            server=tof_host_io.start_survey_http(nav,Pose(),
                dict(token_file=str(token),listen="127.0.0.1",port=0))
            try:
                url="http://127.0.0.1:"+str(server.server_port)+"/v1/survey-zero"
                body=json.dumps({"world_id":"survey","request_id":"same-request"}).encode()
                with self.assertRaises(HTTPError) as e:urlopen(Request(url,body),timeout=2)
                self.assertEqual(e.exception.code,401);self.assertEqual(calls,[])
                req=Request(url,body,{"Authorization":"Bearer "+token.read_text()})
                with urlopen(req,timeout=2) as r:self.assertEqual(json.load(r)["reset_id"],"same-request")
                self.assertEqual(calls,[("survey","same-request")])
            finally:server.shutdown();server.server_close()

if __name__=="__main__":unittest.main()
