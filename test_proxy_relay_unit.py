import socket
import threading
import unittest

from proxy_relay_v2 import recv_http_headers


class ReceiveHttpHeadersTests(unittest.TestCase):
    def test_reads_fragmented_headers(self):
        client, server = socket.socketpair()
        chunks = [b"CON", b"NECT example.com:443 HTTP/1.1\r\nHo", b"st: example.com\r\n\r\n"]

        def send_chunks():
            try:
                for chunk in chunks:
                    client.sendall(chunk)
            finally:
                client.close()

        sender = threading.Thread(target=send_chunks)
        sender.start()
        try:
            self.assertEqual(recv_http_headers(server), b"".join(chunks))
        finally:
            server.close()
            sender.join()

    def test_rejects_incomplete_headers(self):
        client, server = socket.socketpair()
        client.sendall(b"GET / HTTP/1.1\r\nHost: example.com")
        client.close()
        try:
            with self.assertRaises(ConnectionError):
                recv_http_headers(server)
        finally:
            server.close()


if __name__ == "__main__":
    unittest.main()
