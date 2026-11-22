from contextlib import closing
import os
import socket
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


if __name__ == "__main__":
    unittest.main()
