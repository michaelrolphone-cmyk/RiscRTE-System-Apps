"""Optional portable transport for the unchanged ToF estimation modules.

Native execution remains the default. Portable transport uses the existing
authenticated host I/O boundary; it never mounts the native route database.
"""
import atexit
import copy
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import queue
import socket
import threading
import time

_client = None
_survey_settings = None

def configure(path):
    global _client
    if path:
        from biomerte.host_io_client import HostIOClient
        _client = HostIOClient.from_config(path)
    else:
        _client = None
    return _client

def client():
    return _client

def configure_survey(port=None, listen="127.0.0.1", token_file=None):
    global _survey_settings
    if port is None:
        _survey_settings = None
    else:
        if not token_file or not Path(token_file).is_absolute():
            raise ValueError("portable survey control requires a private token file")
        _survey_settings = dict(port=port, listen=listen, token_file=token_file)

def survey_settings():
    return _survey_settings

def start_survey_http(nav, pose, settings):
    import hmac
    token_path = Path(settings["token_file"])
    if token_path.stat().st_mode & 0o077:
        raise ValueError("survey control token must be owner-only")
    token = token_path.read_text().strip()
    if len(token) < 32:
        raise ValueError("survey control token is too short")
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass
        def reply(self, status, value):
            body = json.dumps(value, allow_nan=False).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
        def do_POST(self):
            if not hmac.compare_digest(self.headers.get("Authorization", ""), "Bearer " + token):
                return self.reply(401, dict(ok=False, error="authentication_required"))
            if self.path != "/v1/survey-zero":
                return self.reply(404, dict(ok=False, error="not_found"))
            try:
                self.connection.settimeout(3)
                size = int(self.headers.get("Content-Length", "0"))
                if not 0 < size <= 2048:
                    raise ValueError("invalid request length")
                data = json.loads(self.rfile.read(size))
                with nav.lock:
                    result = pose.zero(nav, data["world_id"], data["request_id"], time.time())
                self.reply(200, result)
            except (ValueError, KeyError, TypeError, OSError):
                self.reply(400, dict(ok=False, error="survey_request_rejected"))
    server = ThreadingHTTPServer((settings["listen"], settings["port"]), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever,
                     name="tof-survey-http", daemon=True).start()
    atexit.register(server.server_close)
    return server

def wall_observed(monotonic_observed, *, now_monotonic=None, now_wall=None):
    """The persisted refinement/motion records use UTC, not a VM boot epoch."""
    now_monotonic = time.monotonic() if now_monotonic is None else now_monotonic
    now_wall = time.time() if now_wall is None else now_wall
    return now_wall - (now_monotonic - monotonic_observed)

class AnchorSocket:
    """Socket-shaped input retaining native UDP peer IP and receive interval."""
    def __init__(self, host_io, max_packets=128):
        self.client = host_io
        self.timeout = .2
        self.inbox = queue.Queue(maxsize=max_packets)
        self.stop = threading.Event()
        self.received_at = None
        self.received_interval_s = None
        self.reset_required = False
        self.last = None
        self.last_error = None
        self.thread = threading.Thread(target=self._run, name="tof-anchor-host-io", daemon=True)
        self.thread.start()
        atexit.register(self.close)

    def settimeout(self, value):
        self.timeout = value

    def _run(self):
        while not self.stop.is_set():
            try:
                for item in self.client.anchor_datagrams():
                    if self.stop.is_set():
                        return
                    self.last_error = None
                    try:
                        self.inbox.put_nowait(item)
                    except queue.Full:
                        try:
                            self.inbox.get_nowait()
                        except queue.Empty:
                            pass
                        self.inbox.put_nowait(dict(item, dropped=1))
            except (OSError, ValueError, EOFError) as error:
                self.last_error = type(error).__name__
                self.stop.wait(.3)

    def recvfrom(self, size):
        try:
            item = self.inbox.get(timeout=self.timeout)
        except queue.Empty:
            raise socket.timeout()
        if len(item["payload"]) > size:
            raise ValueError("anchor datagram does not fit receive buffer")
        identity = item["source_clock_id"], item["sequence"]
        self.reset_required = bool(item.get("dropped") or (self.last and (
            identity[0] != self.last[0] or identity[1] != self.last[1] + 1)))
        self.last = identity
        self.received_interval_s = item["received_interval_s"]
        # Conservatively retain the oldest possible native receive time.
        self.received_at = self.received_interval_s[0]
        return item["payload"], item["peer"]

    def sendto(self, payload, address):
        return self.client.send_responder(payload, address)

    def close(self):
        self.stop.set()

def native_output_clock(snapshot, host_io):
    """Expose legacy raw range timestamps in the original native host epoch.

    Native vision's tof_registration interpolates rover_ranges.observed against
    native captured frames. Refined/baseline state uses UTC and is deliberately
    not transformed. Local source values and uncertainty remain explicit.
    """
    result = copy.deepcopy(snapshot)
    bounds = host_io.clock()
    def convert(row):
        if not isinstance(row, dict):
            return
        if isinstance(row.get("observed"), (int, float)):
            local = row["observed"]
            interval = bounds.to_host_interval([local, local])
            row["observed_local_monotonic_s"] = local
            row["observed_host_monotonic_interval_s"] = interval
            row["observed"] = interval[0]
        for nested in row.get("directional_measurements", []):
            convert(nested)
    for key in ("rover_ranges", "rover_directional_ranges"):
        for row in result.get(key, []):
            convert(row)
    result["transport_clock"] = dict(
        schema="biomerte.host-clock-boundary/1", host_id=bounds.host_id,
        clock_id=bounds.clock_id, raw_range_epoch="native_host_monotonic",
        state_epoch="unix_utc", offset_interval_s=[bounds.offset_low, bounds.offset_high],
        uncertainty_s=bounds.offset_high - bounds.offset_low)
    return result

class RuntimeHealth:
    def __init__(self, port, listen="127.0.0.1"):
        self.lock = threading.Lock()
        self.phase = "preparing"
        self.last_loop = None
        self.vision_fresh = False
        self.anchor_fresh = False
        self.clock_qualified = False
        self.camera_ok = False
        self.inputs_valid = False
        self.server = None
        if port is not None:
            owner = self
            class Handler(BaseHTTPRequestHandler):
                def log_message(self, *args):
                    pass
                def do_GET(self):
                    if self.path not in ("/healthz", "/readyz"):
                        self.send_error(404)
                        return
                    value = owner.snapshot()
                    ready = value["live"] if self.path == "/healthz" else value["ready"]
                    body = json.dumps(value, allow_nan=False).encode()
                    self.send_response(200 if ready else 503)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(body)))
                    self.send_header("Cache-Control", "no-store")
                    self.end_headers()
                    self.wfile.write(body)
            self.server = ThreadingHTTPServer((listen, port), Handler)
            self.server.daemon_threads = True
            threading.Thread(target=self.server.serve_forever,
                             name="tof-runtime-health", daemon=True).start()
            atexit.register(self.close)

    def note(self, snapshot, nav, host_io=None):
        with self.lock:
            self.phase = "active"
            self.last_loop = time.monotonic()
            age = snapshot.get("rover_measurements", {}).get("age_s")
            self.vision_fresh = (type(age) in (int, float) and 0 <= age < 2
                                 and not nav.feed_error)
            self.anchor_fresh = any(0 <= row.get("received_age_s", 1e9) < 30
                                    for row in snapshot.get("anchors", []))
            self.clock_qualified = host_io is None or bool(host_io.clock().valid)
            self.camera_ok = snapshot.get("camera_error") is None
            self.inputs_valid = True

    def snapshot(self):
        with self.lock:
            age = None if self.last_loop is None else time.monotonic() - self.last_loop
            live = self.phase == "preparing" or (age is not None and 0 <= age < 3)
            ready = (self.phase == "active" and live and self.vision_fresh and
                     self.anchor_fresh and self.clock_qualified and self.camera_ok)
            return dict(schema="biomerte.tof-health/1", phase=self.phase,
                        live=live, ready=ready, inputs_valid=self.inputs_valid,
                        loop_age_s=age, vision_fresh=self.vision_fresh,
                        anchor_fresh=self.anchor_fresh, clock_qualified=self.clock_qualified,
                        camera_ok=self.camera_ok)

    def close(self):
        if self.server is not None:
            self.server.shutdown()
            self.server.server_close()
            self.server = None

def inspect_inputs(args):
    """Read-only preparation: no UDP/socket owner, config writer, or actuator."""
    import numpy
    import scipy
    roots = {"tof": Path(args.state), "vision": Path(args.vision_state)}
    names = {"tof": ["baseline.json", "geometry-config.json", "anchor-registry.json",
                     "rover-refinement.json", "display-floor-priors.json"],
             "vision": ["config.json", "camera-rig.json", "survey-world.json", "survey-pose-zero.json",
                        "survey-heading-state.json", "survey-magnetic-north.json",
                        "dual-radio-survey-calibration.json", "rover-world-body-pose.json",
                        "rover-motion.json"]}
    required = {"tof": ("baseline.json", "geometry-config.json", "anchor-registry.json",
                         "rover-refinement.json"),
                "vision": ("config.json", "camera-rig.json", "survey-world.json")}
    for kind, entries in required.items():
        for name in entries:
            if not (roots[kind] / name).is_file():
                raise ValueError("missing qualified state input: " + kind + "/" + name)
    files = {}
    for kind, entries in names.items():
        for name in entries:
            path = roots[kind] / name
            if path.is_file():
                raw = path.read_bytes()
                json.loads(raw)
                files[kind + "/" + name] = hashlib.sha256(raw).hexdigest()
    for path in roots["vision"].glob("attitude-world-reference*.json"):
        raw=path.read_bytes();json.loads(raw)
        files["vision/"+path.name]=hashlib.sha256(raw).hexdigest()
    if client() is not None:
        if not args.camera_id:
            raise ValueError("portable camera identity is required")
        bounds = client().clock()
        if client().route(args.camera_id) is None:
            raise ValueError("registered front camera route unavailable")
        host = dict(host_id=bounds.host_id, clock_id=bounds.clock_id,
                    clock_qualified=bounds.valid)
    else:
        host = None
    return dict(schema="biomerte.tof-preparation/1", ready=False, phase="preparing",
                input_hashes=files, host_io=host,
                dependencies={"numpy": numpy.__version__, "scipy": scipy.__version__},
                state_authority="qualification_required")

def prepare(args):
    receipt = inspect_inputs(args)
    if args.check:
        print(json.dumps(receipt, allow_nan=False, sort_keys=True))
        return
    health = RuntimeHealth(args.health_port, args.health_listen)
    health.inputs_valid = True
    while True:
        time.sleep(.5)
