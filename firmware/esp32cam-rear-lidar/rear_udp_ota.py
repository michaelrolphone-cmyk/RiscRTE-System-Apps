"""Authenticated selective-repeat firmware transfer for rear recovery boots."""

import hashlib
import hmac
import socket
import struct
import time

BLOCK_BYTES = 1024
WINDOW_BLOCKS = 32
CONTROL_PORT = 3233
MAGIC = b"ROT2"
REPLY = 128
BEGIN, BLOCK, STATUS, COMMIT = range(1, 5)
ACK_BYTES = 63


class Transfer:
    def __init__(self, connection, image, password, boot_id):
        self.connection = connection
        self.image = image
        self.key = password.encode("ascii")
        self.boot = boot_id.encode("ascii")
        self.digest = hashlib.sha256(image).digest()
        self.session = hmac.digest(self.key, b"rear-ota-session-v1" + self.boot + self.digest, "sha256")[:16]
        self.total = (len(image) + BLOCK_BYTES - 1) // BLOCK_BYTES

    def packet(self, operation, body=b""):
        payload = MAGIC + bytes((operation,)) + self.session + body
        return payload + hmac.digest(self.key, self.boot + payload, "sha256")

    def exchange(self, operation, body=b"", *, attempts):
        packet = self.packet(operation, body)
        for _ in range(attempts):
            self.connection.send(packet)
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                self.connection.settimeout(max(0.01, deadline - time.monotonic()))
                try:
                    reply = self.connection.recv(ACK_BYTES + 1)
                except TimeoutError:
                    break
                if (len(reply) != ACK_BYTES or reply[:5] != MAGIC + bytes((REPLY,))
                        or reply[5:21] != self.session or reply[21] != operation):
                    continue
                signature = hmac.digest(self.key, self.boot + reply[:31], "sha256")
                if not hmac.compare_digest(signature, reply[31:]):
                    continue
                status, base, bitmap = struct.unpack("!BII", reply[22:31])
                if status == 0 and base <= self.total:
                    return base, bitmap
                if status not in (0, 1, 2):
                    raise RuntimeError(f"Rear UDP update rejected operation {operation} ({status})")
        raise TimeoutError(f"No authenticated rear update reply for operation {operation}")

    def send_blocks(self, base, bitmap):
        for offset in range(min(WINDOW_BLOCKS, self.total - base)):
            if bitmap & (1 << offset):
                continue
            index = base + offset
            start = index * BLOCK_BYTES
            block = self.image[start:start + BLOCK_BYTES]
            self.connection.send(self.packet(BLOCK, struct.pack("!I", index) + block))
            # Leave room for the receiver's cooperative flash writes and ACKs.
            time.sleep(0.008)

    def run(self, progress, idle_timeout):
        base, bitmap = self.exchange(BEGIN, struct.pack("!I", len(self.image)) + self.digest,
                                     attempts=12)
        last_progress = last_report = time.monotonic()
        while base < self.total:
            if time.monotonic() - last_progress >= idle_timeout:
                raise TimeoutError(f"Rear UDP flash stopped advancing at {base}/{self.total} blocks")
            self.send_blocks(base, bitmap)
            try:
                next_base, next_bitmap = self.exchange(STATUS, attempts=2)
            except TimeoutError:
                continue
            if next_base < base:
                continue
            if next_base > base or next_bitmap & ~bitmap:
                last_progress = time.monotonic()
            base, bitmap = next_base, next_bitmap
            if progress and (base == self.total or last_progress - last_report >= 10):
                progress(min(base * BLOCK_BYTES, len(self.image)), len(self.image))
                last_report = last_progress
        confirmed, _ = self.exchange(COMMIT, attempts=4)
        if confirmed != self.total:
            raise RuntimeError("Rear UDP commit did not acknowledge all blocks")


def send_image(host, image, password, boot_id, *, progress=None, idle_timeout=180):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as connection:
        connection.connect((host, CONTROL_PORT))
        Transfer(connection, image, password, boot_id).run(progress, idle_timeout)
