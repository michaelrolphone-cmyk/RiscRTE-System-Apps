"""Check the rover viewer's vision WebSocket, including a fresh attachment."""
import argparse
import asyncio
import io
import json
import math
import os
import time

import aiohttp
from PIL import Image


class StreamWindow:
    def __init__(self, started, dimensions, identities=None):
        self.started = started
        self.dimensions = dimensions
        self.arrivals = {name: [] for name in ("front_jpeg", "rear_jpeg", "front_lidar", "rear_lidar")}
        self.identities = identities if identities is not None else {name: set() for name in self.arrivals}

    def record(self, name, identity, now):
        if identity not in self.identities[name]:
            self.identities[name].add(identity)
            self.arrivals[name].append(now)

    def observe(self, message, now):
        payload = b""
        if isinstance(message, bytes):
            size = int.from_bytes(message[:4], "big")
            if len(message) < 4 or size == 0 or 4 + size > len(message):
                raise ValueError("Invalid vision message envelope")
            metadata = json.loads(message[4:4 + size])
            payload = message[4 + size:]
        else:
            metadata = json.loads(message)
        if not isinstance(metadata, dict):
            raise ValueError("Expected vision metadata object")
        if metadata.get("type") in ("sensor_manifest", "world_snapshot"):
            return
        packet = metadata.get("sensor_packet") or {}
        if not isinstance(packet, dict):
            raise ValueError("Invalid sensor metadata")
        camera = metadata.get("camera_id") or packet.get("camera_id")
        if camera is None:
            camera = {"front-camera": "front", "rear-camera": "rear"}.get(metadata.get("source_id"))
        if camera not in ("front", "rear"):
            return
        if payload:
            identity = metadata.get("frame_id")
            if type(identity) not in (str, int) or identity == "" or metadata.get("image_available") is False:
                raise ValueError("Image without a camera frame identity")
            if not payload.startswith(b"\xff\xd8") or not payload.endswith(b"\xff\xd9"):
                raise ValueError("Incomplete camera JPEG")
            with Image.open(io.BytesIO(payload)) as image:
                if image.format != "JPEG" or image.size != self.dimensions:
                    raise ValueError(f"{camera} image has unexpected format or size: {image.format} {image.size}")
                image.load()
            self.record(camera + "_jpeg", identity, now)
        if camera != "rear" or not metadata.get("sensor_tick"):
            return
        boot = packet.get("boot_id")
        captured = packet.get("capture_monotonic_us")
        if not boot or type(captured) is not int:
            return
        lidar = packet.get("lidar") or metadata.get("lidar") or {}
        if not isinstance(lidar, dict) or not isinstance(lidar.get("sensors", []), list):
            raise ValueError("Invalid LiDAR sensor list")
        for head in lidar.get("sensors", []):
            if not isinstance(head, dict):
                raise ValueError("Invalid LiDAR sensor head")
            name = head.get("id")
            if name not in ("front", "rear") or head.get("valid") is not True:
                continue
            observed = head.get("observed_monotonic_us")
            if type(observed) is not int or not 0 <= captured - observed <= 1_000_000:
                continue
            if head.get("resolution") != 64 or any(
                not isinstance(head.get(field), list) or len(head[field]) != 64
                or any(type(value) is not int for value in head[field])
                for field in ("distance_mm", "target_status")
            ):
                raise ValueError(f"Incomplete {name} LiDAR grid")
            self.record(name + "_lidar", (boot, observed), now)

    def report(self, ended, target_fps, fps_tolerance, max_gap):
        seconds = ended - self.started
        streams = {}
        for name, arrivals in self.arrivals.items():
            times = [self.started, *arrivals, ended]
            gap = max(b - a for a, b in zip(times, times[1:]))
            rate = len(arrivals) / seconds
            passed = len(arrivals) >= 2 and gap <= max_gap
            if name.endswith("_jpeg"):
                passed = passed and abs(rate - target_fps) <= fps_tolerance
            streams[name] = dict(count=len(arrivals), hz=round(rate, 3),
                                 max_gap_s=round(gap, 3), passed=passed)
        rate_difference = abs(len(self.arrivals["front_jpeg"]) - len(self.arrivals["rear_jpeg"])) / seconds
        return dict(seconds=round(seconds, 3), streams=streams,
                    fps_difference=round(rate_difference, 3),
                    passed=rate_difference <= fps_tolerance and all(row["passed"] for row in streams.values()))


async def measure(session, args, identities=None):
    start = time.monotonic()
    window = StreamWindow(start, (args.width, args.height), identities)
    error = None
    deadline = start + args.seconds
    try:
        async with asyncio.timeout_at(asyncio.get_running_loop().time() + args.seconds):
            async with session.ws_connect(args.url, max_msg_size=0) as socket:
                async for message in socket:
                    if message.type in (aiohttp.WSMsgType.TEXT, aiohttp.WSMsgType.BINARY):
                        window.observe(message.data, time.monotonic())
                    elif message.type == aiohttp.WSMsgType.ERROR:
                        raise socket.exception() or ConnectionError("WebSocket read failed")
                error = f"WebSocket closed before the observation ended (code {socket.close_code})"
    except asyncio.TimeoutError:
        if time.monotonic() < deadline:
            error = "Connection or read timed out before the observation ended"
    except (aiohttp.ClientError, OSError, ValueError, TypeError, KeyError) as exc:
        # Keep credentials and signed query strings out of qualification logs.
        error = f"{type(exc).__name__}: {exc}" if isinstance(exc, ValueError) else type(exc).__name__
    status = window.report(time.monotonic(), args.target_fps, args.fps_tolerance, args.max_gap)
    if error:
        status.update(error=error, passed=False)
    return status


async def main(args):
    headers = {}
    if authorization := os.environ.get("ROVER_VISION_AUTHORIZATION"):
        headers["Authorization"] = authorization
    async with aiohttp.ClientSession(headers=headers) as session:
        passed = True
        identities = {name: set() for name in ("front_jpeg", "rear_jpeg", "front_lidar", "rear_lidar")}
        for phase in ("attach", "reconnect"):
            result = await measure(session, args, identities)
            print(json.dumps(dict(phase=phase, **result)), flush=True)
            passed = passed and result["passed"]
    return 0 if passed else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url", help="Authorized rover viewer vision WebSocket URL")
    parser.add_argument("--seconds", type=float, default=60, help="Observation time per attachment")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--target-fps", type=float, default=5, help="Deployed front camera frame rate")
    parser.add_argument("--fps-tolerance", type=float, default=0.5, help="Allowed rate difference in fps")
    parser.add_argument("--max-gap", type=float, default=1.0, help="Maximum delivery gap in seconds for each stream")
    options = parser.parse_args()
    parameters = (options.width, options.height, options.seconds, options.target_fps, options.max_gap)
    if not all(math.isfinite(n) and n > 0 for n in parameters) or not 0 <= options.fps_tolerance < options.target_fps:
        parser.error("Use positive finite parameters and a tolerance below the target fps")
    raise SystemExit(asyncio.run(main(options)))
