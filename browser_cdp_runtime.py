"""Apply ProfilePower fingerprint settings through Chrome DevTools Protocol."""

from __future__ import annotations

import base64
import hashlib
import json
import os
import socket
import struct
import sys
import time
import urllib.request
from pathlib import Path
from urllib.parse import urlsplit


class WebSocket:
    def __init__(self, url: str):
        parsed = urlsplit(url)
        self.sock = socket.create_connection((parsed.hostname, parsed.port or 80), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        path = parsed.path or "/"
        if parsed.query:
            path += "?" + parsed.query
        request = (
            f"GET {path} HTTP/1.1\r\nHost: {parsed.hostname}:{parsed.port or 80}\r\n"
            f"Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(request.encode("ascii"))
        response = self._read_until(b"\r\n\r\n")
        if b" 101 " not in response.split(b"\r\n", 1)[0]:
            raise RuntimeError(f"WebSocket upgrade failed: {response[:200]!r}")
        expected = base64.b64encode(
            hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()
        )
        if expected.lower() not in response.lower():
            raise RuntimeError("WebSocket accept key mismatch")
        # Chrome can legitimately leave the DevTools socket idle for long periods.
        # Keep the timeout only for the handshake; an idle connection is not a failure.
        self.sock.settimeout(None)

    def _read_until(self, marker: bytes) -> bytes:
        data = bytearray()
        while marker not in data:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("WebSocket closed during handshake")
            data.extend(chunk)
        return bytes(data)

    def _read_exact(self, size: int) -> bytes:
        data = bytearray()
        while len(data) < size:
            chunk = self.sock.recv(size - len(data))
            if not chunk:
                raise ConnectionError("WebSocket closed")
            data.extend(chunk)
        return bytes(data)

    def send(self, value: dict) -> None:
        payload = json.dumps(value, separators=(",", ":")).encode()
        mask = os.urandom(4)
        size = len(payload)
        if size < 126:
            header = bytes((0x81, 0x80 | size))
        elif size <= 0xFFFF:
            header = bytes((0x81, 0xFE)) + struct.pack("!H", size)
        else:
            header = bytes((0x81, 0xFF)) + struct.pack("!Q", size)
        masked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        self.sock.sendall(header + mask + masked)

    def receive(self) -> dict:
        fragments = bytearray()
        while True:
            first, second = self._read_exact(2)
            final, opcode = bool(first & 0x80), first & 0x0F
            size = second & 0x7F
            if size == 126:
                size = struct.unpack("!H", self._read_exact(2))[0]
            elif size == 127:
                size = struct.unpack("!Q", self._read_exact(8))[0]
            mask = self._read_exact(4) if second & 0x80 else b""
            payload = self._read_exact(size)
            if mask:
                payload = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
            if opcode == 0x8:
                raise ConnectionError("WebSocket closed by Chrome")
            if opcode == 0x9:
                self._send_control(0xA, payload)
                continue
            if opcode in (0x0, 0x1):
                fragments.extend(payload)
                if final:
                    return json.loads(fragments.decode("utf-8"))

    def _send_control(self, opcode: int, payload: bytes) -> None:
        mask = os.urandom(4)
        masked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        self.sock.sendall(bytes((0x80 | opcode, 0x80 | len(payload))) + mask + masked)


class FingerprintController:
    def __init__(self, port: int, config: dict, browser_version: str):
        self.port = port
        self.config = config
        self.browser_version = browser_version
        self.ws: WebSocket | None = None
        self.next_id = 0
        self.pending: dict[int, tuple] = {}
        self.attaching: set[str] = set()
        self.sessions: dict[str, str] = {}
        self.configured: set[str] = set()
        self.pending_urls: dict[str, str] = {}
        self.startup_urls = list(config.get("startup_url_list") or [])
        self.startup_initialized = False

    def command(self, method: str, params: dict | None = None, session: str | None = None, tag: tuple = ()) -> int:
        self.next_id += 1
        message = {"id": self.next_id, "method": method, "params": params or {}}
        if session:
            message["sessionId"] = session
        if tag:
            self.pending[self.next_id] = tag
        self.ws.send(message)
        return self.next_id

    def metadata(self) -> dict:
        major = str(self.config.get("browser_major_version") or self.browser_version.split(".", 1)[0] or "146")
        full = self.browser_version if self.browser_version.startswith(major + ".") else f"{major}.0.0.0"
        product = "Microsoft Edge" if self.config.get("browser_engine") == "Edge" else "Google Chrome"
        return {
            "brands": [
                {"brand": "Not_A Brand", "version": "24"},
                {"brand": "Chromium", "version": major},
                {"brand": product, "version": major},
            ],
            "fullVersionList": [
                {"brand": "Not_A Brand", "version": "24.0.0.0"},
                {"brand": "Chromium", "version": full},
                {"brand": product, "version": full},
            ],
            "platform": self.config.get("ua_ch_platform") or "Windows",
            "platformVersion": self.config.get("ua_ch_platform_version") or "",
            "architecture": self.config.get("ua_ch_architecture") or "x86",
            "model": "",
            "mobile": bool(self.config.get("ua_ch_mobile", False)),
            "bitness": self.config.get("ua_ch_bitness") or "64",
            "wow64": False,
        }

    def attach(self, target_id: str) -> None:
        if target_id in self.attaching or target_id in self.sessions:
            return
        self.attaching.add(target_id)
        self.command("Target.attachToTarget", {"targetId": target_id, "flatten": True}, tag=("attach", target_id))

    def configure(self, target_id: str, session: str) -> None:
        if session in self.configured:
            return
        self.sessions[target_id] = session
        self.configured.add(session)
        user_agent = str(self.config.get("user_agent") or "")
        if user_agent:
            accept_language = str(self.config.get("accept_language") or "").strip()
            override = {
                "userAgent": user_agent,
                "acceptLanguage": accept_language,
                "platform": self.config.get("navigator_platform") or "",
                "userAgentMetadata": self.metadata(),
            }
            self.command("Network.enable", session=session)
            self.command("Emulation.setUserAgentOverride", override, session=session)
            self.command("Network.setUserAgentOverride", override, session=session)
        timezone = self.config.get("timezone")
        if timezone and self.config.get("timezone_mode_index") != 1:
            self.command("Emulation.setTimezoneOverride", {"timezoneId": timezone}, session=session)
        if self.config.get("geolocation_mode_index") != 1:
            latitude, longitude = self.config.get("latitude"), self.config.get("longitude")
            if latitude is not None and longitude is not None:
                self.command(
                    "Emulation.setGeolocationOverride",
                    {"latitude": float(latitude), "longitude": float(longitude), "accuracy": 30},
                    session=session,
                )
        print(json.dumps({"event": "configured", "target": target_id, "metadata": self.metadata()}), flush=True)
        self._initialize_startup(target_id, session)
        if target_id in self.pending_urls:
            self.command("Page.navigate", {"url": self.pending_urls.pop(target_id)}, session=session)

    def _initialize_startup(self, target_id: str, session: str) -> None:
        if self.startup_initialized or not self.startup_urls:
            return
        self.startup_initialized = True
        self.command("Page.navigate", {"url": self.startup_urls[0]}, session=session)
        for url in self.startup_urls[1:]:
            self.command("Target.createTarget", {"url": "about:blank"}, tag=("create", url))

    def handle(self, message: dict) -> None:
        if "id" in message:
            tag = self.pending.pop(message["id"], ())
            if tag and "error" in message:
                print(json.dumps({"event": "cdp_error", "command": tag, "error": message["error"]}), flush=True)
            elif tag and tag[0] == "targets":
                for info in message.get("result", {}).get("targetInfos", []):
                    if info.get("type") == "page":
                        self.attach(info["targetId"])
            elif tag and tag[0] == "attach":
                target_id = tag[1]
                self.attaching.discard(target_id)
                session = message.get("result", {}).get("sessionId")
                if session:
                    self.configure(target_id, session)
            elif tag and tag[0] == "create":
                target_id = message.get("result", {}).get("targetId")
                if target_id:
                    self.pending_urls[target_id] = tag[1]
                    if target_id in self.sessions:
                        self.command("Page.navigate", {"url": self.pending_urls.pop(target_id)}, session=self.sessions[target_id])
            return

        method, params = message.get("method"), message.get("params", {})
        if method == "Target.targetCreated":
            info = params.get("targetInfo", {})
            if info.get("type") == "page":
                self.attach(info["targetId"])
        elif method == "Target.attachedToTarget":
            info = params.get("targetInfo", {})
            if info.get("type") == "page":
                self.attaching.discard(info.get("targetId", ""))
                self.configure(info["targetId"], params["sessionId"])
        elif method == "Target.detachedFromTarget":
            self.configured.discard(params.get("sessionId", ""))

    def run(self, websocket_url: str) -> None:
        self.ws = WebSocket(websocket_url)
        self.command("Target.setDiscoverTargets", {"discover": True})
        self.command("Target.getTargets", tag=("targets",))
        while True:
            self.handle(self.ws.receive())


def wait_for_browser(port: int, timeout: float = 20) -> tuple[str, str]:
    deadline = time.time() + timeout
    url = f"http://127.0.0.1:{port}/json/version"
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                info = json.load(response)
            browser = str(info.get("Browser", ""))
            return info["webSocketDebuggerUrl"], browser.partition("/")[2]
        except Exception:
            time.sleep(0.2)
    raise TimeoutError(f"Chrome debugging endpoint did not open on port {port}")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: browser_cdp_runtime.py PORT CONFIG_JSON", file=sys.stderr)
        return 2
    port = int(sys.argv[1])
    config_path = Path(sys.argv[2])
    config = json.loads(config_path.read_text(encoding="utf-8"))
    while True:
        try:
            websocket_url, browser_version = wait_for_browser(port)
            FingerprintController(port, config, browser_version).run(websocket_url)
        except KeyboardInterrupt:
            return 0
        except Exception as error:
            print(json.dumps({"event": "runtime_error", "error": str(error)}), flush=True)
            time.sleep(1)


if __name__ == "__main__":
    raise SystemExit(main())
