from contextlib import closing, redirect_stdout
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from unittest import mock

import upload_ota
import os
import socket
import stat
import threading
import time
import unittest

from upload_ota import transfer_image


class OtaTransferTests(unittest.TestCase):
    def test_backpressure_and_split_reply_preserve_exact_bytes(self):
        image = os.urandom(190000)
        received = bytearray()
        sender, board = socket.socketpair()
        with closing(sender), closing(board):
            sender.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 2048)

            def receiver():
                time.sleep(0.12)
                while len(received) < len(image):
                    chunk = board.recv(521)
                    if not chunk:
                        return
                    received.extend(chunk)
                    board.sendall(str(len(chunk)).encode())
                board.sendall(b"O")
                time.sleep(0.03)
                board.sendall(b"K")

            thread = threading.Thread(target=receiver, daemon=True)
            thread.start()
            transfer_image(sender, image, idle_timeout=2, total_timeout=5)
            thread.join(2)
        self.assertEqual(bytes(received), image)

    def test_closed_connection_without_final_ack_fails(self):
        sender, board = socket.socketpair()
        with closing(sender), closing(board):
            def receiver():
                board.recv(3)
                board.sendall(b"3")
                board.shutdown(socket.SHUT_WR)
            thread = threading.Thread(target=receiver, daemon=True)
            thread.start()
            with self.assertRaisesRegex(ConnectionError, "final OK"):
                transfer_image(sender, b"app", idle_timeout=1, total_timeout=3)
            thread.join(1)

    def test_silent_or_erroneous_receiver_fails(self):
        sender, board = socket.socketpair()
        with closing(sender), closing(board):
            with self.assertRaises(TimeoutError):
                transfer_image(sender, b"bin", idle_timeout=0.03)
        sender, board = socket.socketpair()
        with closing(sender), closing(board):
            board.sendall(b"Error activating partition")
            with self.assertRaisesRegex(RuntimeError, "Error activating partition"):
                transfer_image(sender, b"bin", idle_timeout=1)

    def test_premature_ok_cannot_verify_an_image(self):
        sender, board = socket.socketpair()
        with closing(sender), closing(board):
            board.sendall(b"OK")
            with self.assertRaisesRegex(RuntimeError, "rejected upload"):
                transfer_image(sender, b"unsent", idle_timeout=1)


class SimulatedClock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class OtaReceiptTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.build = Path(self.temporary.name)
        self.receipt = self.build / "attempt.json"
        self.image = b"checked test image"
        self.digest = hashlib.sha256(self.image).hexdigest()
        self.host = "192.0.2.31"
        self.receiver = "rear-1ca14b858428"
        self.release = "1.0-rear.8"
        self.before = {
            "receiver_id": self.receiver, "camera_id": "rear", "firmware": "1.0-rear.7",
            "boot_id": "before-boot", "running_partition": "app1", "ota_supported": True,
            "password": "extra-response-secret",
        }
        self.after = dict(self.before, firmware=self.release, boot_id="after-boot",
                          running_partition="app0")
        self.clock = SimulatedClock()

    def install(self):
        return upload_ota.install(self.host, self.receiver, self.build, self.digest,
                                  self.release, receipt_path=self.receipt)

    def pending(self):
        # A persisted attempt from an earlier process; verification must retain
        # this baseline instead of learning a new one from a reachable board.
        receipt = {
            "schema": 1, "host": self.host, "receiver_id": self.receiver,
            "expected_release": self.release, "image_sha256": self.digest,
            "image_bytes": len(self.image),
            "before": {key: value for key, value in self.before.items() if key != "password"},
            "expected_partition": "app0", "transfer": "accepted",
            "boot_confirmation": "pending", "verification_status": "pending",
        }
        self.receipt.write_text(json.dumps(receipt))
        return receipt

    def test_recovery_requires_a_new_boot_in_the_same_slot_before_upload(self):
        normal = dict(self.before, ota_mode="normal", boot_id="0" * 32)
        recovery = dict(normal, ota_mode="recovery", boot_id="1" * 32)
        live = dict(self.after, ota_mode="normal", boot_id="2" * 32)
        key = "a" * 64
        client = mock.Mock()
        client.getresponse.return_value.status = 202

        def accepted(host, image, password):
            self.assertEqual(self.clock.now, 1)
            self.assertEqual(password, key)
            self.assertTrue(json.loads(self.receipt.read_text())["recovery_mode"])

        observations = [normal, normal, recovery, live]
        with mock.patch.object(upload_ota, "check_image", return_value=self.image), \
                mock.patch.object(upload_ota, "board_state", side_effect=observations) as state, \
                mock.patch.object(upload_ota, "udp_control", side_effect=TimeoutError), \
                mock.patch.object(upload_ota.http.client, "HTTPConnection", return_value=client), \
                mock.patch.object(upload_ota, "send_image", side_effect=accepted) as sender, \
                mock.patch.object(upload_ota.time, "monotonic", self.clock.monotonic), \
                mock.patch.object(upload_ota.time, "sleep", self.clock.sleep), \
                redirect_stdout(io.StringIO()):
            self.assertTrue(upload_ota.install(self.host, self.receiver, self.build, self.digest,
                                               self.release, receipt_path=self.receipt, recovery_key=key))
        self.assertTrue(all(call.kwargs == {"recovery": True} for call in state.call_args_list))
        client.request.assert_called_once_with(
            "POST", "/api/ota/recovery", headers={"X-Rear-OTA-Proof": mock.ANY})
        sender.assert_called_once()
        self.assertEqual(json.loads(self.receipt.read_text())["confirmed_observation"]["ota_mode"], "normal")
        self.assertNotIn(key, self.receipt.read_text())

    def test_recovery_rejects_an_unexpected_active_partition(self):
        active = dict(self.before, ota_mode="normal", boot_id="0" * 32)
        wrong = dict(active, ota_mode="recovery", boot_id="1" * 32, running_partition="app0")
        with mock.patch.object(upload_ota, "_request_recovery", return_value=202), \
                mock.patch.object(upload_ota, "board_state", return_value=wrong):
            with self.assertRaisesRegex(RuntimeError, "partition changed"):
                upload_ota.enter_recovery(self.host, self.receiver, active, "a" * 64)

    def test_udp_recovery_does_not_need_a_tcp_control_connection(self):
        active = dict(self.before, ota_mode="normal", boot_id="0" * 32)
        recovery = dict(active, ota_mode="recovery", boot_id="1" * 32)
        with mock.patch.object(upload_ota, "udp_control", return_value=b"accepted") as datagram, \
                mock.patch.object(upload_ota, "board_state", return_value=recovery), \
                mock.patch.object(upload_ota.http.client, "HTTPConnection") as tcp, \
                redirect_stdout(io.StringIO()):
            self.assertEqual(upload_ota.enter_recovery(self.host, self.receiver, active, "a" * 64),
                             recovery)
        datagram.assert_called_once()
        self.assertTrue(datagram.call_args.args[1].startswith(b"rear-ota-enter-v1 "))
        tcp.assert_not_called()

    def test_invalid_maintenance_proof_leaves_transfer_unstarted(self):
        normal = dict(self.before, ota_mode="normal", boot_id="0" * 32)
        client = mock.Mock()
        client.getresponse.return_value.status = 403
        with mock.patch.object(upload_ota, "check_image", return_value=self.image), \
                mock.patch.object(upload_ota, "board_state", return_value=normal), \
                mock.patch.object(upload_ota, "udp_control", side_effect=TimeoutError), \
                mock.patch.object(upload_ota.http.client, "HTTPConnection", return_value=client), \
                mock.patch.object(upload_ota, "send_image") as sender, \
                mock.patch.object(upload_ota.time, "monotonic", self.clock.monotonic), \
                mock.patch.object(upload_ota.time, "sleep", self.clock.sleep), \
                redirect_stdout(io.StringIO()):
            self.assertFalse(upload_ota.install(self.host, self.receiver, self.build, self.digest,
                                                self.release, receipt_path=self.receipt, recovery_key="a" * 64))
        sender.assert_not_called()
        self.assertEqual(json.loads(self.receipt.read_text())["transfer"], "not_started")

    def test_delayed_reachability_survives_process_reload_without_second_upload(self):
        observations = 0

        def state(host, receiver):
            nonlocal observations
            self.assertEqual((host, receiver), (self.host, self.receiver))
            observations += 1
            if observations == 1:
                return self.before
            raise OSError("unreachable with extra-error-secret")

        def accepted(host, image, password):
            # The recovery baseline is already durable before the network step.
            saved = json.loads(self.receipt.read_text())
            self.assertEqual(saved["before"]["boot_id"], "before-boot")
            self.assertEqual(saved["transfer"], "in_progress")
            self.assertEqual(stat.S_IMODE(self.receipt.stat().st_mode), 0o600)
            self.assertEqual((host, image), (self.host, self.image))

        with mock.patch.object(upload_ota, "check_image", return_value=self.image), \
                mock.patch.object(upload_ota, "board_state", side_effect=state), \
                mock.patch.object(upload_ota, "send_image", side_effect=accepted) as sender, \
                mock.patch.dict(os.environ, {"REAR_OTA_PASSWORD": "test-upload-secret"}), \
                mock.patch.object(upload_ota.time, "monotonic", self.clock.monotonic), \
                mock.patch.object(upload_ota.time, "sleep", self.clock.sleep), \
                redirect_stdout(io.StringIO()) as output:
            self.assertFalse(self.install())
            self.assertEqual(sender.call_count, 1)
        saved = json.loads(self.receipt.read_text())
        self.assertEqual(saved["transfer"], "accepted")
        self.assertEqual(saved["boot_confirmation"], "pending")
        self.assertEqual(self.clock.now, 90)
        self.assertIn("Awaiting physical boot confirmation", output.getvalue())
        for secret in ("test-upload-secret", "extra-response-secret", "extra-error-secret"):
            self.assertNotIn(secret, self.receipt.read_text())

        # Reload the module to discard process memory, then emulate the actual
        # incident: the new boot is only HTTP-reachable several minutes later.
        spec = importlib.util.spec_from_file_location("resumed_upload_ota", upload_ota.__file__)
        resumed = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(resumed)
        self.clock.now = 376
        with mock.patch.object(resumed, "board_state", return_value=self.after), \
                mock.patch.object(resumed, "send_image") as resumed_send, \
                mock.patch.object(resumed, "invite") as invitation, \
                mock.patch.object(resumed.socket, "socket") as sockets, \
                mock.patch.object(resumed, "check_image") as image_check, \
                mock.patch.dict(os.environ, {}, clear=True), \
                redirect_stdout(io.StringIO()):
            self.assertEqual(resumed.main(["--verify-only", str(self.receipt)]), 0)
        resumed_send.assert_not_called()
        invitation.assert_not_called()
        sockets.assert_not_called()
        image_check.assert_not_called()
        final = json.loads(self.receipt.read_text())
        self.assertEqual(final["before"]["boot_id"], "before-boot")
        self.assertEqual(final["image_sha256"], self.digest)
        self.assertEqual(final["boot_confirmation"], "confirmed")
        self.assertEqual(final["observation"]["boot_id"], "after-boot")

    def test_concurrent_verifier_cannot_overwrite_an_active_transfer(self):
        def accepted(host, image, password):
            with self.assertRaisesRegex(RuntimeError, "receipt is already active"):
                upload_ota.verify_receipt(self.receipt)

        with mock.patch.object(upload_ota, "check_image", return_value=self.image), \
                mock.patch.object(upload_ota, "board_state", side_effect=[self.before, self.after]) as state, \
                mock.patch.object(upload_ota, "send_image", side_effect=accepted), \
                mock.patch.dict(os.environ, {"REAR_OTA_PASSWORD": "test-only"}), \
                redirect_stdout(io.StringIO()):
            self.assertTrue(self.install())
        self.assertEqual(state.call_count, 2)
        saved = json.loads(self.receipt.read_text())
        self.assertEqual(saved["transfer"], "accepted")
        self.assertEqual(saved["boot_confirmation"], "confirmed")
        self.assertEqual(saved["verification_status"], "confirmed")

    def test_lost_terminal_reply_can_be_confirmed_by_physical_boot(self):
        with mock.patch.object(upload_ota, "check_image", return_value=self.image), \
                mock.patch.object(upload_ota, "board_state", side_effect=[self.before, self.after]), \
                mock.patch.object(upload_ota, "send_image", side_effect=ConnectionError("lost OK")) as sender, \
                mock.patch.dict(os.environ, {"REAR_OTA_PASSWORD": "test-only"}), \
                redirect_stdout(io.StringIO()):
            self.assertTrue(self.install())
        sender.assert_called_once()
        saved = json.loads(self.receipt.read_text())
        self.assertEqual(saved["transfer"], "outcome_unknown")
        self.assertEqual(saved["boot_confirmation"], "confirmed")

    def test_interruption_keeps_original_baseline_and_blocks_upload_replay(self):
        with mock.patch.object(upload_ota, "check_image", return_value=self.image), \
                mock.patch.object(upload_ota, "board_state", return_value=self.before), \
                mock.patch.object(upload_ota, "send_image", side_effect=KeyboardInterrupt) as sender, \
                mock.patch.dict(os.environ, {"REAR_OTA_PASSWORD": "test-only"}), \
                redirect_stdout(io.StringIO()):
            with self.assertRaises(KeyboardInterrupt):
                self.install()
            self.assertEqual(json.loads(self.receipt.read_text())["transfer"], "in_progress")
            with self.assertRaisesRegex(RuntimeError, "no upload attempted.*--verify-only"):
                self.install()
            self.assertEqual(sender.call_count, 1)
        with mock.patch.object(upload_ota, "board_state", return_value=self.after), \
                mock.patch.object(upload_ota, "send_image") as sender, \
                redirect_stdout(io.StringIO()):
            self.assertTrue(upload_ota.verify_receipt(self.receipt))
        sender.assert_not_called()

    def test_wrong_identity_release_boot_or_slot_never_confirms(self):
        variants = {
            "receiver": dict(self.after, receiver_id="rear-other"),
            "camera": dict(self.after, camera_id="front"),
            "release": dict(self.after, firmware="1.0-rear.7"),
            "boot": dict(self.after, boot_id=self.before["boot_id"]),
            "slot": dict(self.after, running_partition="app1"),
            "availability": dict(self.after, ota_supported=False),
        }
        for name, observation in variants.items():
            with self.subTest(name=name):
                self.pending()
                with mock.patch.object(upload_ota, "board_state", return_value=observation), \
                        mock.patch.object(upload_ota, "send_image") as sender, \
                        mock.patch.object(upload_ota.time, "monotonic", self.clock.monotonic), \
                        mock.patch.object(upload_ota.time, "sleep", self.clock.sleep), \
                        redirect_stdout(io.StringIO()):
                    self.assertFalse(upload_ota.verify_receipt(self.receipt, timeout=2))
                sender.assert_not_called()
                self.assertEqual(json.loads(self.receipt.read_text())["boot_confirmation"], "pending")

    def test_pending_cli_exit_is_distinct_and_never_opens_upload_socket(self):
        self.pending()
        with mock.patch.object(upload_ota, "board_state", side_effect=OSError("not reachable")), \
                mock.patch.object(upload_ota.socket, "socket") as sockets, \
                mock.patch.object(upload_ota, "send_image") as sender, \
                mock.patch.object(upload_ota.time, "monotonic", self.clock.monotonic), \
                mock.patch.object(upload_ota.time, "sleep", self.clock.sleep), \
                redirect_stdout(io.StringIO()):
            self.assertEqual(upload_ota.main(["--verify-only", str(self.receipt)]), 3)
        sockets.assert_not_called()
        sender.assert_not_called()

    def test_bad_receipt_binding_fails_before_contacting_board(self):
        variants = {
            "receiver": {"receiver_id": "rear-other"},
            "image": {"image_sha256": "not-a-sha256"},
            "partition": {"expected_partition": "app1"},
            "witness": {"boot_confirmation": "confirmed"},
        }
        for name, changes in variants.items():
            with self.subTest(name=name):
                receipt = self.pending()
                receipt.update(changes)
                self.receipt.write_text(json.dumps(receipt))
                with mock.patch.object(upload_ota, "board_state") as state, \
                        mock.patch.object(upload_ota, "send_image") as sender:
                    with self.assertRaises(ValueError):
                        upload_ota.verify_receipt(self.receipt)
                state.assert_not_called()
                sender.assert_not_called()

    def test_wrong_image_digest_prevents_receipt_and_upload(self):
        with mock.patch.object(upload_ota, "check_image", return_value=b"different image"), \
                mock.patch.object(upload_ota, "board_state") as state, \
                mock.patch.object(upload_ota, "send_image") as sender:
            with self.assertRaisesRegex(ValueError, "expected image SHA-256"):
                self.install()
        self.assertFalse(self.receipt.exists())
        state.assert_not_called()
        sender.assert_not_called()

    def test_first_check_interruption_persists_pending_before_http(self):
        self.pending()
        with mock.patch.object(upload_ota, "board_state", return_value=self.after), \
                redirect_stdout(io.StringIO()):
            self.assertTrue(upload_ota.verify_receipt(self.receipt))
        prior = json.loads(self.receipt.read_text())

        def interrupted(host, receiver):
            saved = json.loads(self.receipt.read_text())
            self.assertEqual(saved["verification_status"], "pending")
            self.assertEqual(saved["verification_attempts"], prior["verification_attempts"] + 1)
            self.assertTrue(saved["last_check_at"])
            self.assertEqual(saved["confirmed_observation"], prior["confirmed_observation"])
            raise KeyboardInterrupt

        with mock.patch.object(upload_ota, "board_state", side_effect=interrupted):
            with self.assertRaises(KeyboardInterrupt):
                upload_ota.verify_receipt(self.receipt)
        saved = json.loads(self.receipt.read_text())
        self.assertEqual(saved["verification_status"], "pending")
        self.assertEqual(saved["before"], prior["before"])
        self.assertEqual(saved["confirmed_observation"], prior["confirmed_observation"])
        with mock.patch.object(upload_ota, "board_state", return_value=self.after), \
                redirect_stdout(io.StringIO()):
            self.assertTrue(upload_ota.verify_receipt(self.receipt))

    def test_previously_confirmed_receipt_requires_a_fresh_board_observation(self):
        self.pending()
        with mock.patch.object(upload_ota, "board_state", return_value=self.after), \
                redirect_stdout(io.StringIO()):
            self.assertTrue(upload_ota.verify_receipt(self.receipt))
        old_boot = dict(self.after, boot_id=self.before["boot_id"])
        with mock.patch.object(upload_ota, "board_state", return_value=old_boot) as state, \
                mock.patch.object(upload_ota, "send_image") as sender, \
                mock.patch.object(upload_ota.time, "monotonic", self.clock.monotonic), \
                mock.patch.object(upload_ota.time, "sleep", self.clock.sleep), \
                redirect_stdout(io.StringIO()):
            self.assertFalse(upload_ota.verify_receipt(self.receipt, timeout=2))
        state.assert_called_once_with(self.host, self.receiver)
        sender.assert_not_called()
        saved = json.loads(self.receipt.read_text())
        self.assertEqual(saved["verification_status"], "pending")
        self.assertEqual(saved["observation"]["boot_id"], "before-boot")
        self.assertEqual(saved["boot_confirmation"], "confirmed")
        self.assertEqual(saved["confirmed_observation"]["boot_id"], "after-boot")
        self.assertEqual(saved["before"]["boot_id"], "before-boot")


if __name__ == "__main__":
    unittest.main()
