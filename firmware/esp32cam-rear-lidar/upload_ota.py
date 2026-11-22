"""Upload one checked rear image over ArduinoOTA and verify the physical boot."""

import argparse
import base64
import hashlib
import http.client
import ipaddress
import json
import os
from pathlib import Path
import secrets
import select
import socket
import time

from check_ota_image import check_image


def transfer_image(connection, image, *, idle_timeout=90, total_timeout=900, progress=None):
    """Advance only by bytes accepted by send(), while draining the ESP's ACKs."""
    connection.setblocking(False)
    data = memoryview(image)
    sent = 0
    suffix = b""
    started = last_progress = reported = time.monotonic()
    while True:
        now = time.monotonic()
        remaining = min(last_progress + idle_timeout, started + total_timeout) - now
        if remaining <= 0:
            raise TimeoutError(f"No final ArduinoOTA reply after submitting {sent}/{len(data)} bytes")
        readable, writable, _ = select.select(
            [connection], [connection] if sent < len(data) else [], [], remaining
        )
        if readable:
            reply = connection.recv(4096)
            if not reply:
                raise ConnectionError(f"ESP closed before final OK at {sent}/{len(data)} bytes")
            last_progress = time.monotonic()
            # The ESP sends unframed decimal write sizes. Its final OK can be
            # split across reads or follow a numeric ACK in the same read.
            response = suffix + reply
            acknowledgement = response.lstrip(b"0123456789")
            if acknowledgement == b"OK" and sent == len(data):
                return
            if acknowledgement not in (b"", b"O"):
                raise RuntimeError(f"ESP rejected upload at {sent}/{len(data)} bytes: {acknowledgement[:96]!r}")
            suffix = acknowledgement
        if writable:
            try:
                accepted = connection.send(data[sent:sent + 16384])
            except BlockingIOError:
                continue
            if accepted == 0:
                raise ConnectionError(f"ESP stopped accepting data at {sent}/{len(data)} bytes")
            sent += accepted
            last_progress = time.monotonic()
        if progress and last_progress - reported >= 10:
            progress(sent, len(data))
            reported = last_progress


def board_state(host, expected_receiver):
    headers = {}
    user = os.environ.get("REAR_CAMERA_USER")
    if user:
        credentials = f"{user}:{os.environ['REAR_CAMERA_PASSWORD']}".encode()
        headers["Authorization"] = "Basic " + base64.b64encode(credentials).decode("ascii")
    client = http.client.HTTPConnection(host, timeout=8)
    try:
        client.request("GET", "/api/rear", headers=headers)
        response = client.getresponse()
        if response.status in (429, 502, 503, 504):
            raise OSError(f"Rear identity check returned HTTP {response.status}")
        if response.status != 200:
            raise RuntimeError(f"Rear identity check returned HTTP {response.status}")
        state = json.loads(response.read(131072))
    finally:
        client.close()
    if (state.get("receiver_id") != expected_receiver or state.get("camera_id") != "rear"
            or state.get("running_partition") not in ("app0", "app1")
            or not state.get("boot_id") or state.get("ota_supported") is not True):
        raise ValueError("Physical rear identity, partition or OTA availability did not match")
    return state


def invite(remote, listener, password, image):
    digest = hashlib.md5(image).hexdigest()  # Wire protocol required by ArduinoOTA.
    message = f"0 {listener.getsockname()[1]} {len(image)} {digest}\n".encode()
    for attempt in range(3):
        remote.send(message)
        try:
            nonce = remote.recv(80).split()
            break
        except TimeoutError:
            if attempt == 2:
                raise
    if len(nonce) != 2 or nonce[0] != b"AUTH":
        raise RuntimeError("Device did not request ArduinoOTA authentication")
    cnonce = secrets.token_hex(16).encode("ascii")
    password_hash = hashlib.md5(password.encode()).hexdigest().encode()
    proof = hashlib.md5(b":".join((password_hash, nonce[1], cnonce))).hexdigest().encode()
    remote.send(b"200 " + cnonce + b" " + proof + b"\n")
    # The final UDP reply can be lost independently of the authenticated TCP callback.
    ready, _, _ = select.select([remote, listener], [], [], 30)
    if not ready:
        raise TimeoutError("ArduinoOTA did not acknowledge authentication or connect back")
    if remote in ready and remote.recv(80).strip() != b"OK":
        raise RuntimeError("ArduinoOTA authentication failed")


def install(host, receiver, build, expected_digest, release):
    image = check_image(build, release)
    if hashlib.sha256(image).hexdigest() != expected_digest:
        raise ValueError("Build does not match the expected image SHA-256")
    deadline = time.monotonic() + 60
    while True:
        try:
            before = board_state(host, receiver)
            break
        except (OSError, http.client.HTTPException):
            if time.monotonic() >= deadline:
                raise
            time.sleep(2)
    if before.get("firmware") == release:
        print("Board already reports", release, before["running_partition"], before["boot_id"])
        return
    password = os.environ["REAR_OTA_PASSWORD"]
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as remote, socket.socket() as listener:
        remote.connect((host, 3232))
        listener.bind((remote.getsockname()[0], 0))
        listener.listen(1)
        listener.settimeout(30)
        remote.settimeout(6)
        invite(remote, listener, password, image)
        connection, address = listener.accept()
        with connection:
            if address[0] != host:
                raise ValueError("Unexpected ArduinoOTA callback address")
            connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            connection.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 32768)
            transfer_error = None
            try:
                transfer_image(connection, image, progress=lambda n,total: print("Submitted", n, "of", total, flush=True))
                print("ESP accepted the image", flush=True)
            except (OSError, RuntimeError) as error:
                # A final packet can be lost as the board reboots. Its running
                # identity, partition and release are independent witnesses.
                transfer_error = error
                print(type(error).__name__, str(error), flush=True)
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        try:
            current = board_state(host, receiver)
            if (current.get("firmware") == release and current["boot_id"] != before["boot_id"]
                    and current["running_partition"] != before["running_partition"]):
                print("Verified physical boot", receiver, release, current["running_partition"], current["boot_id"])
                return
        except (OSError, http.client.HTTPException):
            pass
        time.sleep(2)
    raise RuntimeError("Physical board did not confirm the expected release, changed boot and alternate slot") from transfer_error


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", type=ipaddress.IPv4Address)
    parser.add_argument("receiver", help="Enrolled rear receiver ID")
    parser.add_argument("build", type=Path, help="Prepared and compiled normal build directory")
    parser.add_argument("sha256", help="Expected SHA-256 of the compiled OTA application")
    args = parser.parse_args()
    version = json.loads(Path(__file__).with_name("manifest.json").read_text())["variant"]
    install(str(args.host), args.receiver, args.build, args.sha256, version)
