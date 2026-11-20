"""Exercise the real active coordinator with isolated host-I/O fixtures.

No production directories or devices are used. This test runs inside the same
image as the solver and verifies processing, native clock output and restart.
"""
import base64
import contextlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from urllib.request import urlopen
from biomerte.host_io_client import PROTOCOL
from test_tof_baseline import Fixture, IDS, XYZ


class FixtureHost:
    host_id = "fixture-native-host"
    clock_id = "fixture-native-boot"
    offset = 900.
    token = "isolated-host-fixture-token-only"

    def __init__(self):
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.requests = []
        self.ftm = dict(status="never_requested", target=None, busy=False, retiring=False)
        self.producer = Fixture()
        self.producer.sequence = 20
        self.started = time.monotonic()
        host = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def auth(self):
                return self.headers.get("Authorization") == "Bearer " + host.token

            def reply(self, status, value):
                data = json.dumps(value, allow_nan=False).encode()
                self.send_response(status)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def do_POST(self):
                if not self.auth():
                    return self.reply(401, {})
                if self.path != "/v1/host-io/call":
                    return self.reply(404, {})
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                if body.get("protocol") != PROTOCOL:
                    return self.reply(400, {})
                operation, args = body["operation"], body["arguments"]
                if operation == "clock":
                    result = host.clock()
                elif operation == "routes.get":
                    result = dict(device_id=args["device_id"], host="192.0.2.20")
                elif operation == "routes.observe_anchor":
                    result = dict(device_id="ftm:" + args["bssid"],host=args["host"])
                elif operation in ("routes.list","routes.candidates"):
                    result = []
                elif operation == "anchors.send":
                    result = len(json.dumps(args["record"]).encode())
                elif operation == "health":
                    result = dict(host_id=host.host_id,anchors_active=True,device_commands_active=True)
                elif operation == "device.request":
                    if args["device_id"] != "fixture-front" or args["path"] != "/api/telemetry/ftm":
                        return self.reply(400, {})
                    payload = args["payload"]
                    if payload:
                        address = payload["address"].lower()
                        point = XYZ[IDS.index(address)]
                        distance = math.sqrt(sum((float(v)-1.)**2 for v in point))
                        with host.lock:
                            host.requests.append(dict(device_id=args["device_id"],payload=payload))
                            host.ftm = dict(status="success",target=address,distance_m=distance,
                                            age_ms=0,busy=False,retiring=False,channel=6)
                    result = dict(ok=True)
                else:
                    return self.reply(400, {})
                self.reply(200,dict(protocol=PROTOCOL,host_id=host.host_id,
                                    clock_id=host.clock_id,result=result))

            def do_GET(self):
                if not self.auth():
                    return self.reply(401, {})
                if self.path not in ("/v1/host-io/anchors","/v1/host-io/vision/live"):
                    return self.reply(404,{})
                self.send_response(200)
                self.send_header("Connection","close")
                self.end_headers()
                self.close_connection = True
                sequence = 0
                try:
                    while not host.stop.is_set():
                        if self.path.endswith("/anchors"):
                            for index,packet in enumerate(host.producer.packets()):
                                packet.update(channel=6,ftm_state="idle")
                                sequence += 1
                                item = dict(clock_id=host.clock_id,
                                    received_monotonic=time.monotonic()+host.offset,
                                    peer=["192.0.2."+str(index+1),7446],sequence=sequence,
                                    payload=base64.b64encode(json.dumps(packet).encode()).decode())
                                self.wfile.write(json.dumps(item).encode()+b"\n")
                        else:
                            stamp=time.monotonic()+host.offset
                            with host.lock:
                                ftm=dict(host.ftm)
                            metadata=dict(sensor_tick=True,sensor_delivery="websocket_push",
                                ranging_observation=dict(
                                    capture_host_monotonic_interval_s=[stamp-.01,stamp],
                                    boot_id="camera-fixture-boot",
                                    uptime_ms=int((time.monotonic()-host.started)*1000)+100,
                                    ftm=ftm,wifi=[]))
                            envelope=json.dumps(dict(metadata=metadata,clock=host.clock())).encode()
                            self.wfile.write(struct.pack("!II",len(envelope)+4,len(envelope))+envelope)
                        self.wfile.flush()
                        host.stop.wait(.1)
                except (OSError,ValueError):
                    pass

        self.server = ThreadingHTTPServer(("127.0.0.1",0), Handler)
        self.server.daemon_threads = True
        self.thread = threading.Thread(target=self.server.serve_forever,daemon=True)
        self.thread.start()

    def clock(self):
        return dict(host_id=self.host_id,clock_id=self.clock_id,
                    monotonic=time.monotonic()+self.offset,wall_time=time.time())

    def close(self):
        self.stop.set()
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1",0))
        return sock.getsockname()[1]


class ActiveCoordinatorTests(unittest.TestCase):
    def test_actual_solver_process_consumes_streams_and_restarts_with_same_baseline(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);state=root/"tof";vision=root/"vision"
            state.mkdir();vision.mkdir()
            fixture=Fixture(state/"baseline.json")
            fixture.now=time.time()-80
            baseline=fixture.train()
            self.assertTrue(baseline["valid"])
            host=FixtureHost()
            try:
                token=root/"host.token";token.write_text(host.token);token.chmod(0o600)
                config=root/"host.json"
                config.write_text(json.dumps(dict(
                    url="http://127.0.0.1:"+str(host.server.server_port),
                    allow_local_http=True,host_id=host.host_id,token_file=str(token))))
                for restart in range(2):
                    port=free_port()
                    command=[sys.executable,"-u",str(Path(__file__).with_name("tof_coordinator.py")),
                        "--camera","http://registered-camera","--camera-id","fixture-front",
                        "--state",str(state),"--vision-state",str(vision),
                        "--host-io-config",str(config),"--health-port",str(port)]
                    process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                                             text=True,env=dict(os.environ,PYTHONDONTWRITEBYTECODE="1"))
                    failure=None
                    try:
                        deadline=time.monotonic()+20
                        health=None
                        while time.monotonic()<deadline:
                            if process.poll() is not None:
                                self.fail("Active solver exited: "+process.stdout.read())
                            try:
                                with urlopen("http://127.0.0.1:"+str(port)+"/readyz",timeout=1) as response:
                                    health=json.load(response)
                                latest=json.loads((state/"latest.json").read_text())
                                if latest.get("anchor_count")==5 and latest.get("rover_ranges") and host.requests:
                                    break
                            except (OSError,ValueError):
                                pass
                            time.sleep(.1)
                        else:
                            self.fail("Actual solver did not produce a ready, populated solution")
                        self.assertEqual(health["phase"],"active")
                        self.assertTrue(health["ready"])
                        self.assertEqual(latest["baseline"]["frame_id"],baseline["frame_id"])
                        self.assertTrue(latest["baseline"]["valid"])
                        self.assertEqual(latest["transport_clock"]["host_id"],host.host_id)
                        self.assertEqual(latest["transport_clock"]["raw_range_epoch"],"native_host_monotonic")
                        self.assertEqual(latest["anchor_transport"]["registered"],5)
                        self.assertTrue(all(r["device_id"]=="fixture-front" for r in host.requests))
                        for row in latest["rover_ranges"]:
                            difference=row["observed"]-row["observed_local_monotonic_s"]
                            self.assertAlmostEqual(difference,host.offset,delta=.1)
                    finally:
                        if process.poll() is None:
                            process.terminate()
                        try:
                            logs=process.communicate(timeout=10)[0]
                        except subprocess.TimeoutExpired:
                            process.kill();logs=process.communicate()[0]
                            self.fail("Fixture solver did not stop cleanly: "+logs)
                        self.assertEqual(process.returncode,0,logs)
            finally:
                host.close()


if __name__=="__main__":unittest.main()
