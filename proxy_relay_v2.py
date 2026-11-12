#!/usr/bin/env python3
"""HTTP frontend proxy with HTTP/HTTPS/SOCKS5 authenticated upstream support."""

import base64
import os
import select
import socket
import ssl
import struct
import sys
import threading
from urllib.parse import urlsplit


def recv_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("unexpected end of stream")
        data.extend(chunk)
    return bytes(data)


def recv_http_headers(sock, limit=1024 * 1024):
    """Read a complete HTTP header block without assuming TCP packet boundaries."""
    data = bytearray()
    while b"\r\n\r\n" not in data:
        if len(data) >= limit:
            raise ValueError("HTTP request headers are too large")
        chunk = sock.recv(min(65536, limit - len(data)))
        if not chunk:
            raise ConnectionError("unexpected end of HTTP headers")
        data.extend(chunk)
    return bytes(data)


def split_host_port(value, default_port):
    value = value.strip()
    if value.startswith("["):
        host, _, rest = value[1:].partition("]")
        return host, int(rest[1:]) if rest.startswith(":") else default_port
    host, sep, port = value.rpartition(":")
    return (host, int(port)) if sep and port.isdigit() else (value, default_port)


def socks5_connect(proxy_host, proxy_port, username, password, target_host, target_port):
    upstream = socket.create_connection((proxy_host, proxy_port), timeout=12)
    methods = b"\x00\x02" if username or password else b"\x00"
    upstream.sendall(b"\x05" + bytes([len(methods)]) + methods)
    version, method = recv_exact(upstream, 2)
    if version != 5 or method == 0xFF:
        raise ConnectionError("SOCKS5 authentication method rejected")
    if method == 2:
        user = username.encode("utf-8")
        secret = password.encode("utf-8")
        if len(user) > 255 or len(secret) > 255:
            raise ValueError("SOCKS5 credentials are too long")
        upstream.sendall(b"\x01" + bytes([len(user)]) + user + bytes([len(secret)]) + secret)
        if recv_exact(upstream, 2)[1] != 0:
            raise PermissionError("SOCKS5 username/password rejected")
    try:
        address = socket.inet_pton(socket.AF_INET, target_host)
        encoded_target = b"\x01" + address
    except OSError:
        try:
            address = socket.inet_pton(socket.AF_INET6, target_host)
            encoded_target = b"\x04" + address
        except OSError:
            host = target_host.encode("idna")
            encoded_target = b"\x03" + bytes([len(host)]) + host
    upstream.sendall(b"\x05\x01\x00" + encoded_target + struct.pack("!H", target_port))
    version, result, _, address_type = recv_exact(upstream, 4)
    if version != 5 or result != 0:
        raise ConnectionError(f"SOCKS5 connect failed with code {result}")
    address_size = 4 if address_type == 1 else 16 if address_type == 4 else recv_exact(upstream, 1)[0]
    recv_exact(upstream, address_size + 2)
    return upstream


def tunnel(client, upstream):
    client.settimeout(None)
    upstream.settimeout(None)
    sockets = [client, upstream]
    while True:
        readable, _, exceptional = select.select(sockets, [], sockets, 120)
        if exceptional or not readable:
            return
        for source in readable:
            data = source.recv(65536)
            if not data:
                return
            (upstream if source is client else client).sendall(data)


def handle_socks_upstream(client, first_data, host, port, username, password):
    header_end = first_data.find(b"\r\n")
    if header_end < 0:
        raise ValueError("invalid HTTP request")
    request_line = first_data[:header_end].decode("latin-1")
    method, target, version = request_line.split(" ", 2)
    if method.upper() == "CONNECT":
        target_host, target_port = split_host_port(target, 443)
        upstream = socks5_connect(host, port, username, password, target_host, target_port)
        try:
            client.sendall(b"HTTP/1.1 200 Connection Established\r\n\r\n")
            tunnel(client, upstream)
        finally:
            upstream.close()
        return
    parsed = urlsplit(target)
    if not parsed.hostname:
        raise ValueError("SOCKS5 HTTP forwarding requires an absolute URL")
    target_port = parsed.port or (443 if parsed.scheme == "https" else 80)
    upstream = socks5_connect(host, port, username, password, parsed.hostname, target_port)
    path = parsed.path or "/"
    if parsed.query:
        path += "?" + parsed.query
    rewritten = f"{method} {path} {version}".encode("latin-1") + first_data[header_end:]
    try:
        upstream.sendall(rewritten)
        tunnel(client, upstream)
    finally:
        upstream.close()


def handle_http_upstream(client, first_data, host, port, username, password, use_tls):
    upstream = socket.create_connection((host, port), timeout=12)
    if use_tls:
        upstream = ssl.create_default_context().wrap_socket(upstream, server_hostname=host)
    if username or password:
        auth = base64.b64encode(f"{username}:{password}".encode()).decode()
        line_end = first_data.find(b"\r\n")
        first_data = first_data[:line_end + 2] + f"Proxy-Authorization: Basic {auth}\r\n".encode() + first_data[line_end + 2:]
    try:
        upstream.sendall(first_data)
        tunnel(client, upstream)
    finally:
        upstream.close()


def handle_client(client, upstream_type, host, port, username, password):
    try:
        client.settimeout(10)
        first_data = recv_http_headers(client)
        if upstream_type == "socks5":
            handle_socks_upstream(client, first_data, host, port, username, password)
        else:
            handle_http_upstream(client, first_data, host, port, username, password, upstream_type == "https")
    except Exception as error:
        print(f"[Proxy Relay] connection failed: {error}", flush=True)
    finally:
        try:
            client.close()
        except OSError:
            pass


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: proxy_relay_v2.py LOCAL_PORT")
    local_port = int(sys.argv[1])
    upstream_type = os.environ.get("PROFILE_PROXY_TYPE", "http").lower()
    host = os.environ["PROFILE_PROXY_HOST"]
    port = int(os.environ["PROFILE_PROXY_PORT"])
    username = os.environ.get("PROFILE_PROXY_USER", "")
    password = os.environ.get("PROFILE_PROXY_PASS", "")
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", local_port))
    server.listen(100)
    print(f"READY 127.0.0.1:{local_port} -> {upstream_type}://{host}:{port}", flush=True)
    try:
        while True:
            client, _ = server.accept()
            threading.Thread(target=handle_client, args=(client, upstream_type, host, port, username, password), daemon=True).start()
    except KeyboardInterrupt:
        pass
    finally:
        server.close()


if __name__ == "__main__":
    main()
