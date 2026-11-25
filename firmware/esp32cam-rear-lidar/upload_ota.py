"""Upload one checked rear image; record and resume physical boot confirmation."""

import argparse
import base64
from contextlib import contextmanager
import fcntl
from datetime import datetime, timezone
import hashlib
import hmac
import http.client
import ipaddress
import json
import os
from pathlib import Path
import secrets
import select
import shlex
import socket
import sys
import tempfile
import time

from check_ota_image import check_image
import rear_udp_ota


def transfer_image(connection, image, *, idle_timeout=90, total_timeout=None, progress=None):
    """Advance only by bytes accepted by send(), while draining the ESP's ACKs."""
    connection.setblocking(False)
    data = memoryview(image)
    sent = 0
    suffix = b""
    started = last_progress = reported = time.monotonic()
    while True:
        now = time.monotonic()
        deadline = last_progress + idle_timeout
        if total_timeout is not None:
            deadline = min(deadline, started + total_timeout)
        remaining = deadline - now
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


RECOVERY_PORT = 3233


def udp_control(host, message):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as transport:
        transport.connect((host, RECOVERY_PORT))
        transport.settimeout(1)
        for attempt in range(3):
            transport.send(message)
            try:
                return transport.recv(512)
            except TimeoutError:
                if attempt == 2:
                    raise


def _http_state(host, recovery):
    headers = {}
    user = os.environ.get("REAR_CAMERA_USER")
    if user and not recovery:
        credentials = f"{user}:{os.environ['REAR_CAMERA_PASSWORD']}".encode()
        headers["Authorization"] = "Basic " + base64.b64encode(credentials).decode("ascii")
    client = http.client.HTTPConnection(host, RECOVERY_PORT if recovery else 80, timeout=8)
    try:
        client.request("GET", "/api/rear", headers=headers)
        response = client.getresponse()
        if response.status in (429, 502, 503, 504):
            raise OSError(f"Rear identity check returned HTTP {response.status}")
        if response.status != 200:
            raise RuntimeError(f"Rear identity check returned HTTP {response.status}")
        return json.loads(response.read(131072))
    finally:
        client.close()


def board_state(host, expected_receiver, *, recovery=False):
    if recovery:
        try:
            state = json.loads(udp_control(host, b"rear-ota-status-v1"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            state = _http_state(host, recovery)
    else:
        state = _http_state(host, recovery)
    if (not isinstance(state, dict)
            or state.get("receiver_id") != expected_receiver or state.get("camera_id") != "rear"
            or state.get("running_partition") not in ("app0", "app1")
            or not isinstance(state.get("firmware"), str) or not state["firmware"]
            or not isinstance(state.get("boot_id"), str) or not state["boot_id"]
            or state.get("ota_supported") is not True):
        raise ValueError("Physical rear identity, partition or OTA availability did not match")
    if recovery and (state.get("ota_mode") not in ("normal", "recovery")
                     or len(state["boot_id"]) != 32
                     or any(value not in "0123456789abcdef" for value in state["boot_id"])):
        raise ValueError("Rear maintenance identity is invalid")
    return state


def _request_recovery(host, receiver, state, key):
    message = f"rear-ota-enter-v1:{receiver}:{state['boot_id']}".encode()
    proof = hmac.new(key.encode(), message, hashlib.sha256).hexdigest()
    try:
        if udp_control(host, b"rear-ota-enter-v1 " + proof.encode()) == b"accepted":
            return 202
    except OSError:
        pass
    client = http.client.HTTPConnection(host, RECOVERY_PORT, timeout=8)
    try:
        client.request("POST", "/api/ota/recovery", headers={"X-Rear-OTA-Proof": proof})
        return client.getresponse().status
    except (OSError, http.client.RemoteDisconnected):
        # The restart can interrupt the reply. Reconcile with the board before retrying.
        return None
    finally:
        client.close()


def enter_recovery(host, receiver, before, key):
    if before["ota_mode"] == "recovery":
        return before
    deadline = time.monotonic() + 90
    last_normal = before
    reply = None
    while time.monotonic() < deadline:
        if reply != 202:
            reply = _request_recovery(host, receiver, last_normal, key)
        try:
            state = board_state(host, receiver, recovery=True)
        except (OSError, http.client.HTTPException):
            time.sleep(1)
            continue
        if (state["running_partition"] != before["running_partition"]
                or state["firmware"] != before["firmware"]):
            raise RuntimeError("Rear firmware or partition changed during the recovery request")
        if state["ota_mode"] == "recovery" and state["boot_id"] != before["boot_id"]:
            print("Verified OTA-only boot", receiver, state["boot_id"], flush=True)
            return state
        if reply in (403, 409) and state["boot_id"] == last_normal["boot_id"]:
            raise RuntimeError(f"Rear camera refused OTA recovery ({reply})")
        if state["ota_mode"] == "normal" and state["boot_id"] != last_normal["boot_id"]:
            last_normal = state
            reply = None
        time.sleep(1)
    raise TimeoutError("Rear camera did not boot into OTA recovery")


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


def _snapshot(state):
    """Retain identity fields only, never arbitrary endpoint data or credentials."""
    fields = ("receiver_id", "camera_id", "firmware", "boot_id", "running_partition", "ota_supported")
    return {key: state[key] for key in (*fields, "ota_mode") if key in state}


def _timestamp():
    return datetime.now(timezone.utc).isoformat()


def _resume_command(path):
    return shlex.join([sys.executable, str(Path(__file__).resolve()),
                       "--verify-only", str(Path(path).resolve())])


@contextmanager
def _receipt_lock(path):
    """Serialize an attempt across processes; the OS releases it on cancellation."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor = os.open(str(path) + ".lock", os.O_CREAT | os.O_RDWR, 0o600)
    try:
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError(
                "OTA receipt is already active; no upload or verification started"
            ) from error
        yield
    finally:
        os.close(descriptor)


def _save_receipt(path, receipt, *, create=False):
    """Publish a complete, fsynced receipt; initial creation cannot overwrite one."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    receipt["updated_at"] = _timestamp()
    descriptor, temporary = tempfile.mkstemp(prefix=".ota-receipt-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w") as output:
            json.dump(receipt, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        if create:
            os.link(temporary, path)
        else:
            os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _confirmed(receipt, state):
    before = receipt["before"]
    return (
        isinstance(state, dict)
        and state.get("receiver_id") == receipt["receiver_id"]
        and state.get("camera_id") == "rear"
        and state.get("ota_supported") is True
        and state.get("firmware") == receipt["expected_release"]
        and isinstance(state.get("boot_id"), str) and bool(state["boot_id"])
        and state["boot_id"] != before["boot_id"]
        and state.get("running_partition") == receipt["expected_partition"]
        and (not receipt.get("recovery_mode") or state.get("ota_mode") == "normal")
    )


def _load_receipt(path):
    receipt = json.loads(Path(path).read_text())
    if not isinstance(receipt, dict) or receipt.get("schema") != 1:
        raise ValueError("Unsupported OTA receipt")
    before = receipt.get("before", {})
    digest = receipt.get("image_sha256", "")
    if (
        type(receipt.get("recovery_mode", False)) is not bool
        or not isinstance(before, dict)
        or before.get("receiver_id") != receipt.get("receiver_id")
        or before.get("camera_id") != "rear" or before.get("ota_supported") is not True
        or not isinstance(before.get("firmware"), str) or not before["firmware"]
        or not isinstance(before.get("boot_id"), str) or not before["boot_id"]
        or before.get("running_partition") not in ("app0", "app1")
        or receipt.get("expected_partition") not in ("app0", "app1")
        or receipt["expected_partition"] == before["running_partition"]
        or not isinstance(receipt.get("receiver_id"), str) or not receipt["receiver_id"]
        or not isinstance(receipt.get("expected_release"), str) or not receipt["expected_release"]
        or not isinstance(digest, str) or len(digest) != 64
        or any(character not in "0123456789abcdef" for character in digest)
        or type(receipt.get("image_bytes")) is not int or receipt["image_bytes"] <= 0
        or receipt.get("transfer") not in ("not_started", "in_progress", "accepted", "outcome_unknown")
        or receipt.get("boot_confirmation") not in ("pending", "confirmed")
        or receipt.get("verification_status") not in ("pending", "confirmed")
    ):
        raise ValueError("OTA receipt identity, image or partition binding is invalid")
    if str(ipaddress.IPv4Address(receipt.get("host", ""))) != receipt["host"]:
        raise ValueError("OTA receipt host must be a canonical IPv4 address")
    if receipt["boot_confirmation"] == "confirmed" and (
        not isinstance(receipt.get("confirmed_at"), str) or not receipt["confirmed_at"]
        or not _confirmed(receipt, receipt.get("confirmed_observation", {}))
    ):
        raise ValueError("OTA receipt has no matching physical boot witness")
    if receipt["verification_status"] == "confirmed" and (
        receipt["boot_confirmation"] != "confirmed"
        or not _confirmed(receipt, receipt.get("observation", {}))
    ):
        raise ValueError("OTA receipt has no matching current observation")
    return receipt


def verify_receipt(path, *, timeout=90):
    """Observe an existing attempt. This path never invites or uploads firmware."""
    with _receipt_lock(path):
        return _verify_receipt(path, timeout=timeout)


def _verify_receipt(path, *, timeout=90):
    path = Path(path)
    receipt = _load_receipt(path)
    # Every explicit verification obtains a fresh witness. Keep any earlier
    # positive boot witness separately from this observation window's status.
    receipt["verification_status"] = "pending"
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        receipt["verification_attempts"] = receipt.get("verification_attempts", 0) + 1
        receipt["last_check_at"] = _timestamp()
        receipt.pop("last_check_error_type", None)
        # A cancelled HTTP call must leave a durable pending attempt, even
        # when the preceding observation window confirmed the image.
        _save_receipt(path, receipt)
        try:
            if receipt.get("recovery_mode"):
                current = board_state(receipt["host"], receipt["receiver_id"], recovery=True)
            else:
                current = board_state(receipt["host"], receipt["receiver_id"])
            receipt["observation"] = _snapshot(current)
            receipt.pop("last_check_error_type", None)
            if _confirmed(receipt, current):
                receipt["boot_confirmation"] = "confirmed"
                receipt["verification_status"] = "confirmed"
                receipt["confirmed_at"] = _timestamp()
                receipt["confirmed_observation"] = _snapshot(current)
                _save_receipt(path, receipt)
                print("Verified physical boot", receipt["receiver_id"], receipt["expected_release"],
                      current["running_partition"], current["boot_id"])
                return True
        except (OSError, http.client.HTTPException) as error:
            # Error messages and arbitrary response fields can contain secrets.
            receipt["last_check_error_type"] = type(error).__name__
        except (ValueError, RuntimeError) as error:
            receipt["last_check_error_type"] = type(error).__name__
            _save_receipt(path, receipt)
            raise
        _save_receipt(path, receipt)
        time.sleep(min(2, max(0, deadline - time.monotonic())))
    print("Awaiting physical boot confirmation; transfer outcome:", receipt["transfer"])
    print("Resume observation without uploading:", _resume_command(path))
    return False


def send_image(host, image, password):
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
            transfer_image(connection, image, progress=lambda n, total:
                           print("Submitted", n, "of", total, flush=True))


def install(host, receiver, build, expected_digest, release, *, receipt_path=None, recovery_key=None):
    image = check_image(build, release)
    if hashlib.sha256(image).hexdigest() != expected_digest:
        raise ValueError("Build does not match the expected image SHA-256")
    path = Path(receipt_path) if receipt_path else (
        Path(build) / "ota-receipts" / f"{receiver}-{expected_digest}.json"
    )
    with _receipt_lock(path):
        return _install_attempt(host, receiver, image, expected_digest, release, path, recovery_key=recovery_key)


def _install_attempt(host, receiver, image, expected_digest, release, path, *, recovery_key=None):
    if path.exists():
        raise RuntimeError("OTA receipt already exists; no upload attempted. Resume with: "
                           + _resume_command(path))
    deadline = time.monotonic() + 60
    while True:
        try:
            before = (board_state(host, receiver, recovery=True) if recovery_key
                      else board_state(host, receiver))
            break
        except (OSError, http.client.HTTPException):
            if time.monotonic() >= deadline:
                raise
            time.sleep(2)
    if before.get("firmware") == release:
        print("Board already reports", release, before["running_partition"], before["boot_id"])
        return True
    password = recovery_key or os.environ["REAR_OTA_PASSWORD"]
    receipt = {
        "schema": 1, "host": host, "receiver_id": receiver,
        "expected_release": release, "image_sha256": expected_digest, "image_bytes": len(image),
        "before": _snapshot(before),
        "expected_partition": "app1" if before["running_partition"] == "app0" else "app0",
        "transfer": "not_started", "boot_confirmation": "pending",
        "verification_status": "pending", "created_at": _timestamp(),
    }
    if recovery_key:
        receipt["recovery_mode"] = True
    # Commit the baseline before even inviting the board. A process interruption
    # after this point must be reconciled with --verify-only, never replayed.
    _save_receipt(path, receipt, create=True)
    print("OTA receipt:", path.resolve(), flush=True)
    try:
        recovery = None
        if recovery_key:
            recovery = enter_recovery(host, receiver, before, recovery_key)
            receipt["recovery_observation"] = _snapshot(recovery)
        receipt["transfer"] = "in_progress"
        use_datagrams = recovery and recovery.get("ota_datagrams") is True
        receipt["transfer_protocol"] = "authenticated_udp_blocks" if use_datagrams else "arduinoota"
        _save_receipt(path, receipt)
        if use_datagrams:
            rear_udp_ota.send_image(host, image, password, recovery["boot_id"],
                                    progress=lambda n, total: print("Flashed", n, "of", total, flush=True))
        else:
            send_image(host, image, password)
    except (OSError, RuntimeError, ValueError) as error:
        # A final reply can be lost as the board reboots. Keep its transfer outcome
        # separate from the identity/version/new-boot/alternate-slot witness.
        if receipt["transfer"] == "in_progress":
            receipt["transfer"] = "outcome_unknown"
        receipt["transfer_error_type"] = type(error).__name__
        print("Transfer outcome unknown:", type(error).__name__, str(error), flush=True)
    else:
        receipt["transfer"] = "accepted"
        print("ESP accepted the image", flush=True)
    _save_receipt(path, receipt)
    return _verify_receipt(path)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", nargs="?", type=ipaddress.IPv4Address)
    parser.add_argument("receiver", nargs="?", help="Enrolled rear receiver ID")
    parser.add_argument("build", nargs="?", type=Path, help="Prepared and compiled normal build directory")
    parser.add_argument("sha256", nargs="?", help="Expected SHA-256 of the compiled OTA application")
    parser.add_argument("--recovery-key-file", type=Path, help="Private key enrolled on this rear camera")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--receipt", type=Path, help="New receipt path for this upload attempt")
    mode.add_argument("--verify-only", type=Path, metavar="RECEIPT",
                      help="Resume physical boot observation; never invite or upload")
    args = parser.parse_args(argv)
    if args.verify_only:
        if any(value is not None for value in (args.host, args.receiver, args.build, args.sha256, args.recovery_key_file)):
            parser.error("--verify-only takes only a saved receipt, without upload arguments")
        confirmed = verify_receipt(args.verify_only)
    else:
        if any(value is None for value in (args.host, args.receiver, args.build, args.sha256)):
            parser.error("upload requires HOST RECEIVER_ID BUILD_DIR IMAGE_SHA256")
        version = json.loads(Path(__file__).with_name("manifest.json").read_text())["variant"]
        recovery_key = None
        if args.recovery_key_file:
            recovery_key = args.recovery_key_file.read_text().strip()
            if len(recovery_key) != 64 or any(value not in "0123456789abcdef" for value in recovery_key):
                parser.error("Recovery key must be 64 lowercase hex digits")
        confirmed = install(str(args.host), args.receiver, args.build, args.sha256, version,
                            receipt_path=args.receipt, recovery_key=recovery_key)
    return 0 if confirmed else 3


if __name__ == "__main__":
    raise SystemExit(main())
