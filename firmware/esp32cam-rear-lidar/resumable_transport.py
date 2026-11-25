"""Resumable transfer for authenticated rear camera OTA sessions."""
import hashlib
import os
import re
import secrets
import select
import socket
import struct
import time


def read_line(connection, maximum=28):
    line = bytearray()
    while len(line) < maximum:
        part = connection.recv(1)
        if not part:
            raise ConnectionError("Peer closed before sending an OTA offset")
        line.extend(part)
        if part == b"\n":
            return bytes(line)
    raise ValueError("Malformed OTA offset")


def authenticate(host, image, secret, listener, udp_port=3232):
    address = (host, udp_port)
    md5 = hashlib.md5(image).hexdigest()
    invitation = f"101 {listener.getsockname()[1]} {len(image)} {md5}\n".encode()
    for _ in range(3):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
            udp.settimeout(5)
            udp.sendto(invitation, address)
            try:
                challenge, source = udp.recvfrom(80)
            except TimeoutError:
                continue
            match = re.fullmatch(rb"AUTH ([0-9a-f]{32})", challenge)
            if not match or source != address:
                raise ValueError("Unexpected OTA challenge source or format")
            nonce = secrets.token_hex(16).encode()
            key = hashlib.md5(secret.encode()).hexdigest().encode()
            proof = hashlib.md5(b":".join((key, match[1], nonce))).hexdigest().encode()
            message = b"200 " + nonce + b" " + proof + b"\n"
            acknowledged = False
            for _ in range(5):
                if not acknowledged:
                    udp.sendto(message, address)
                ready, _, _ = select.select([udp, listener], [], [], 2.5)
                if listener in ready:
                    print("RESUME_OTA_AUTHENTICATED_CALLBACK", flush=True)
                    return
                if udp in ready:
                    response, source = udp.recvfrom(80)
                    if response != b"OK" or source != address:
                        raise RuntimeError("The receiver rejected OTA authentication")
                    acknowledged = True
            if acknowledged:
                print("RESUME_OTA_AUTHENTICATED_WAITING_CALLBACK", flush=True)
                return
            raise TimeoutError("No authenticated OTA response")
    raise TimeoutError("No rear OTA challenge")


def serve_image(listener, host, data, *, force_drop=0):
    submitted = acknowledged = 0
    progress_at = reported = time.monotonic()
    interrupted = False
    resumed = False
    attempt = 0
    while time.monotonic() - progress_at < 145:
        listener.settimeout(max(1, 145 - (time.monotonic() - progress_at)))
        connection, peer = listener.accept()
        with connection:
            if peer[0] != host:
                continue
            attempt += 1
            connection.settimeout(10)
            try:
                line = read_line(connection)
            except OSError as err:
                print("OTA_OFFSET_RETRY",type(err).__name__,flush=True)
                continue
            match = re.fullmatch(rb"RESUME ([0-9]+)\n", line)
            if not match:
                raise ValueError("The board did not request a valid resume offset")
            offset = int(match[1])
            if not acknowledged <= offset <= submitted <= len(data):
                raise ValueError("Resume offset outside the bytes offered")
            if offset > acknowledged:
                progress_at = time.monotonic()
            acknowledged = offset
            resumed = resumed or (attempt > 1 and offset > 0)
            print("ESP_CONFIRMED_RESUME_OFFSET",attempt,offset,flush=True)
            connection.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
            connection.setsockopt(socket.SOL_SOCKET,socket.SO_SNDBUF,8192)
            connection.setblocking(False)
            end = min(force_drop, len(data)) if force_drop and not interrupted else len(data)
            reply = b""
            last_io = time.monotonic()
            while time.monotonic() - last_io < 26:
                readable, writable, _ = select.select([connection],[connection] if offset < end else [],[],1.0)
                try:
                    if readable:
                        response = connection.recv(16)
                        if not response:
                            break
                        reply += response
                        if reply == b"OK\n" and offset == len(data):
                            return resumed
                        if not b"OK\n".startswith(reply):
                            raise ValueError("Invalid final OTA reply")
                    if writable:
                        count = connection.send(data[offset:min(offset+1024,end)])
                        if not count:
                            break
                        offset += count
                        if offset > submitted:
                            progress_at = time.monotonic()
                            submitted = offset
                        last_io = time.monotonic()
                        if last_io - reported >= 15:
                            print("OTA_BYTES_OFFERED",submitted,len(data),flush=True)
                            reported = last_io
                        if offset == end and end < len(data):
                            interrupted = True
                            print("OTA_TEST_INTERRUPTION",offset,flush=True)
                            connection.setsockopt(socket.SOL_SOCKET,socket.SO_LINGER,struct.pack("ii",1,0))
                            break
                except BlockingIOError:
                    continue
                except (BrokenPipeError,ConnectionResetError,TimeoutError) as error:
                    print("OTA_SOCKET_RECONNECT",type(error).__name__,flush=True)
                    break
    raise TimeoutError("No OTA progress within the reconnect window")



def send_resumable_image(host, image, password):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route, socket.socket() as listener:
        route.connect((host, 3232))
        callback_port = int(os.environ.get("REAR_OTA_CALLBACK_PORT", "46321"))
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((route.getsockname()[0], callback_port))
        listener.listen(2)
        authenticate(host, image, password, listener)
        serve_image(listener, host, memoryview(image))
