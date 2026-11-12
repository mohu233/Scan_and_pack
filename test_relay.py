"""Manual integration test for the authenticated proxy relay.

Credentials are read from command-line options or PROFILE_PROXY_* environment
variables so secrets never need to be stored in the repository.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.request


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--type", default=os.getenv("PROFILE_PROXY_TYPE", "http"))
    parser.add_argument("--host", default=os.getenv("PROFILE_PROXY_HOST"))
    parser.add_argument("--port", type=int, default=os.getenv("PROFILE_PROXY_PORT"))
    parser.add_argument("--username", default=os.getenv("PROFILE_PROXY_USER", ""))
    parser.add_argument("--password", default=os.getenv("PROFILE_PROXY_PASS", ""))
    parser.add_argument("--relay-port", type=int, default=55432)
    parser.add_argument("--url", default="https://httpbin.org/ip")
    parser.add_argument(
        "--relay-script",
        type=Path,
        default=Path(__file__).with_name("proxy_relay_v2.py"),
    )
    args = parser.parse_args()
    if not args.host or not args.port:
        parser.error("provide --host and --port, or set PROFILE_PROXY_HOST/PORT")
    return args


def main() -> int:
    args = parse_args()
    environment = os.environ.copy()
    environment.update(
        {
            "PROFILE_PROXY_TYPE": args.type,
            "PROFILE_PROXY_HOST": args.host,
            "PROFILE_PROXY_PORT": str(args.port),
            "PROFILE_PROXY_USER": args.username,
            "PROFILE_PROXY_PASS": args.password,
        }
    )

    relay = subprocess.Popen(
        [sys.executable, str(args.relay_script), str(args.relay_port)],
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        time.sleep(1.0)
        if relay.poll() is not None:
            print(relay.stdout.read() if relay.stdout else "Relay exited early.", file=sys.stderr)
            return 1

        proxy_url = f"http://127.0.0.1:{args.relay_port}"
        opener = urllib.request.build_opener(
            urllib.request.ProxyHandler({"http": proxy_url, "https": proxy_url})
        )
        request = urllib.request.Request(args.url, headers={"User-Agent": "ProfilePower relay test"})
        with opener.open(request, timeout=20) as response:
            print(response.read().decode("utf-8", errors="replace"))
        return 0
    finally:
        relay.terminate()
        try:
            relay.wait(timeout=3)
        except subprocess.TimeoutExpired:
            relay.kill()


if __name__ == "__main__":
    raise SystemExit(main())
