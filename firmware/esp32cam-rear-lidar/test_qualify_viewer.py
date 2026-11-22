import asyncio
import io
import json
import struct
import unittest
from types import SimpleNamespace

from aiohttp import ClientSession, web
from aiohttp.test_utils import TestServer
from PIL import Image

from qualify_viewer import StreamWindow, measure


def encode(metadata, payload=b""):
    header = json.dumps(metadata).encode()
    return struct.pack("!I", len(header)) + header + payload


def jpeg(width=8):
    image = io.BytesIO()
    Image.new("RGB", (width, 6)).save(image, format="JPEG")
    return image.getvalue()


def image_frame(camera, sequence, payload):
    return encode(dict(type="frame", frame_id=f"camera-boot:{sequence}", camera_id=camera,
                       image_available=True), payload)


def scan(tick, front, rear):
    heads = [dict(id=name, valid=True, resolution=64, observed_monotonic_us=observed,
                  distance_mm=[500] * 64, target_status=[5] * 64)
             for name, observed in [("front", front), ("rear", rear)]]
    return encode(dict(type="sensor_tick", camera_id="rear", sensor_tick=True,
                       sensor_packet=dict(boot_id="sensor-boot", capture_monotonic_us=tick,
                                          lidar=dict(sensors=heads))))


class ViewerQualification(unittest.TestCase):
    def test_independent_freshness_and_trailing_silence(self):
        window = StreamWindow(0, (8, 6))
        payload = jpeg()
        for i in range(1, 11):
            for camera in ("front", "rear"):
                window.observe(image_frame(camera, i, payload), i / 5)
            window.observe(scan(i, i, i), i / 5)
        self.assertTrue(window.report(2, 5, 0.5, 1)["passed"])
        self.assertFalse(window.report(4, 5, 0.5, 1)["passed"])

    def test_replayed_head_and_replayed_jpeg_do_not_count(self):
        window = StreamWindow(0, (8, 6))
        payload = jpeg()
        for i in range(1, 11):
            window.observe(image_frame("front", i, payload), i / 5)
            window.observe(image_frame("rear", 1, payload), i / 5)
            window.observe(scan(i, i, 1), i / 5)
        result = window.report(2, 5, 0.5, 1)
        self.assertFalse(result["passed"])
        self.assertEqual(result["streams"]["rear_lidar"]["count"], 1)
        self.assertEqual(result["streams"]["rear_jpeg"]["count"], 1)
        self.assertTrue(result["streams"]["front_lidar"]["passed"])

    def test_reconnect_does_not_count_previous_attachment_replays(self):
        first = StreamWindow(0, (8, 6))
        packet = scan(1, 1, 1)
        frame = image_frame("rear", 1, jpeg())
        for message in (packet, frame):
            first.observe(message, 0.5)
        reconnect = StreamWindow(1, (8, 6), first.identities)
        for message in (packet, frame):
            reconnect.observe(message, 1.1)
        self.assertFalse(any(reconnect.arrivals.values()))
        reconnect.observe(scan(2, 2, 2), 1.2)
        self.assertEqual(len(reconnect.arrivals["rear_lidar"]), 1)

    def test_sockets_with_only_map_manifest_or_stale_scans_fail(self):
        window = StreamWindow(0, (8, 6))
        for kind in ("sensor_manifest", "world_snapshot"):
            window.observe(encode(dict(type=kind)), 0.1)
        window.observe(scan(2_000_000, 1, 2_000_001), 0.2)
        self.assertFalse(window.report(2, 5, 0.5, 1)["passed"])
        self.assertFalse(any(window.arrivals.values()))

    def test_decode_image_and_check_actual_dimensions(self):
        window = StreamWindow(0, (8, 6))
        for payload in (jpeg(4), jpeg()[:-5], b"\xff\xd8broken\xff\xd9"):
            with self.assertRaises((ValueError, OSError)):
                window.observe(image_frame("rear", 1, payload), 0.1)


class SocketReconnect(unittest.IsolatedAsyncioTestCase):
    async def test_real_socket_independent_range_delivery_and_early_close(self):
        attempts = []
        payload = jpeg()

        async def handler(request):
            socket = web.WebSocketResponse()
            await socket.prepare(request)
            attempts.append(1)
            for i in range(1, 4):
                await socket.send_bytes(image_frame("front", i, payload))
                if len(attempts) == 2:
                    await socket.send_bytes(image_frame("rear", i, payload))
                await socket.send_bytes(scan(i, i, i))
            if len(attempts) == 2:
                await socket.close()
            else:
                async for _ in socket:
                    pass
            return socket

        app = web.Application()
        app.router.add_get("/vision", handler)
        async with TestServer(app) as server, ClientSession() as session:
            args = SimpleNamespace(url=str(server.make_url("/vision")), seconds=0.5,
                                   width=8, height=6, target_fps=5, fps_tolerance=1, max_gap=1)
            starved = await measure(session, args)
            closed = await measure(session, args)
        self.assertEqual(len(attempts), 2)
        self.assertFalse(starved["passed"])
        self.assertEqual(starved["streams"]["rear_jpeg"]["count"], 0)
        self.assertEqual(starved["streams"]["rear_lidar"]["count"], 3)
        self.assertFalse(closed["passed"])
        self.assertIn("closed", closed["error"])
        self.assertEqual(closed["streams"]["rear_jpeg"]["count"], 3)

    async def test_real_socket_compression_and_replayed_reconnect(self):
        payload = jpeg()
        for requested, allow, negotiated in ((None, True, 0), (15, True, 15), (15, False, 0)):
            with self.subTest(requested=requested, server_compression=allow):
                handshakes = []

                async def handler(request):
                    socket = web.WebSocketResponse(compress=allow)
                    await socket.prepare(request)
                    handshakes.append((request.headers.get("Sec-WebSocket-Extensions"), socket.compress))
                    for i in range(1, 4):
                        for camera in ("front", "rear"):
                            await socket.send_bytes(image_frame(camera, i, payload))
                        await socket.send_bytes(scan(i, i, i))
                    async for _ in socket:
                        pass
                    return socket

                app = web.Application()
                app.router.add_get("/vision", handler)
                async with TestServer(app) as server, ClientSession() as session:
                    args = SimpleNamespace(url=str(server.make_url("/vision")), seconds=0.5,
                                           width=8, height=6, target_fps=5, fps_tolerance=1, max_gap=1)
                    if requested is not None:
                        args.compression = requested
                    identities = {name: set() for name in
                                  ("front_jpeg", "rear_jpeg", "front_lidar", "rear_lidar")}
                    first = await measure(session, args, identities)
                    replay = await measure(session, args, identities)
                self.assertTrue(first["passed"])
                self.assertFalse(replay["passed"])
                self.assertTrue(all(row["count"] == 0 for row in replay["streams"].values()))
                for result in (first, replay):
                    self.assertEqual(result["compression_requested"], requested or 0)
                    self.assertEqual(result["compression_negotiated"], negotiated)
                self.assertEqual(len(handshakes), 2)
                for extension, server_compression in handshakes:
                    self.assertEqual(bool(server_compression), bool(negotiated))
                    if requested:
                        self.assertIn("permessage-deflate", extension or "")
                    else:
                        self.assertIsNone(extension)
