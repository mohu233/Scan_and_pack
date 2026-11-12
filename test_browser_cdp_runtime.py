import base64
import hashlib
import unittest
from unittest.mock import patch

from browser_cdp_runtime import FingerprintController, WebSocket


class FakeSocket:
    def __init__(self, response):
        self.response = bytearray(response)
        self.timeout_values = []
        self.sent = []

    def recv(self, size):
        if not self.response:
            return b""
        chunk = bytes(self.response[:size])
        del self.response[:size]
        return chunk

    def sendall(self, data):
        self.sent.append(data)

    def settimeout(self, value):
        self.timeout_values.append(value)


class RecordingWebSocket:
    def __init__(self):
        self.messages = []

    def send(self, message):
        self.messages.append(message)


class WebSocketTests(unittest.TestCase):
    def test_switches_to_blocking_mode_after_handshake(self):
        nonce = b"0123456789abcdef"
        key = base64.b64encode(nonce).decode()
        accept = base64.b64encode(
            hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()
            ).digest()
        )
        response = (
            b"HTTP/1.1 101 Switching Protocols\r\n"
            b"Upgrade: websocket\r\n"
            b"Connection: Upgrade\r\n"
            + b"Sec-WebSocket-Accept: "
            + accept
            + b"\r\n\r\n"
        )
        fake_socket = FakeSocket(response)

        with patch("browser_cdp_runtime.os.urandom", return_value=nonce), patch(
            "browser_cdp_runtime.socket.create_connection", return_value=fake_socket
        ) as create_connection:
            WebSocket("ws://127.0.0.1:9222/devtools/browser/test")

        create_connection.assert_called_once_with(("127.0.0.1", 9222), timeout=10)
        self.assertEqual(fake_socket.timeout_values, [None])


class FingerprintControllerTests(unittest.TestCase):
    def test_preserves_accept_language_quality_weights(self):
        controller = FingerprintController(
            9222,
            {
                "user_agent": "Test Agent",
                "accept_language": "zh-CN,zh;q=0.9,en;q=0.8",
                "navigator_platform": "Win32",
                "timezone_mode_index": 1,
                "geolocation_mode_index": 1,
            },
            "146.0.0.0",
        )
        websocket = RecordingWebSocket()
        controller.ws = websocket

        with patch("builtins.print"):
            controller.configure("target", "session")

        override = next(
            message
            for message in websocket.messages
            if message["method"] == "Emulation.setUserAgentOverride"
        )
        self.assertEqual(
            override["params"]["acceptLanguage"],
            "zh-CN,zh;q=0.9,en;q=0.8",
        )


if __name__ == "__main__":
    unittest.main()
