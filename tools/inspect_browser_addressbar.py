"""Read-only inspection of Chromium address-bar extension support.

Reads shipped code and API schemas; does not attach to a browser, execute an
extension, read account configuration, or change the browser installation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mmap
import struct
from pathlib import Path


METHODS = (
    "setProductName",
    "setCustomLocationTextVisible",
    "getCustomLocationTextVisible",
    "showNativePageInfoDialog",
)


def inspect_kernel(path: Path) -> dict:
    result = {"path": str(path.resolve()), "schema_found": False, "methods": []}
    with path.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
        result["sha256"] = hashlib.sha256(data).hexdigest()
        position = data.find(b'"namespace":"superbrowser"')
        if position >= 0:
            start = data.rfind(b"\0", 0, position) + 1
            end = data.find(b"\0", position)
            if end < 0:
                raise ValueError("The API schema is not NUL terminated")
            schema = json.loads(data[start:end])
            if schema.get("namespace") != "superbrowser":
                raise ValueError("Unexpected API namespace")
            result["schema_found"] = True
            result["schema_file_offset"] = hex(start)
            result["methods"] = [item for item in schema.get("functions", []) if item.get("name") in METHODS]
        result["registration_strings"] = {}
        for method in METHODS:
            offset = data.find(("superbrowser." + method).encode("ascii"))
            result["registration_strings"][method] = hex(offset) if offset >= 0 else None
        hint = b"chrome\\browser\\extensions\\api\\superbrowser\\superbrowser_api.cc"
        offset = data.find(hint)
        result["native_source_hint"] = {"path": hint.decode(), "file_offset": hex(offset)} if offset >= 0 else None
        result["configuration_key_candidates"] = {}
        for name in ("product_name_click_url", "force_custom_location_text"):
            offset = data.find(name.encode())
            result["configuration_key_candidates"][name] = hex(offset) if offset >= 0 else None
    return result


def inspect_extension(root: Path) -> dict:
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8-sig"))
    result = {
        "path": str(root.resolve()),
        "name": manifest.get("name"),
        "version": manifest.get("version"),
        "manifest_version": manifest.get("manifest_version"),
        "superbrowser_permission": "superbrowser" in manifest.get("permissions", []),
        "references": [],
    }
    terms = ["chrome.superbrowser." + method for method in METHODS]
    terms += ["generateUrlPannelShowText", "isUrlPannelShowName", "isUrlPannelShowIP", "isUrlPannelShowIpRegion"]
    for path in sorted(root.rglob("*.js")):
        source = path.read_text(encoding="utf-8", errors="replace")
        for term in terms:
            offset = source.find(term)
            if offset >= 0:
                result["references"].append({
                    "term": term,
                    "file": path.relative_to(root).as_posix(),
                    "line": source.count("\n", 0, offset) + 1,
                    "character_offset": offset,
                })
    return result


def inspect_client(path: Path) -> dict:
    """Inspect one known static launcher entry without extracting the archive."""
    with path.open("rb") as file:
        header = file.read(16)
        if len(header) != 16:
            raise ValueError("Truncated ASAR header")
        header_size = struct.unpack_from("<I", header, 4)[0]
        json_size = struct.unpack_from("<I", header, 12)[0]
        if not 0 < json_size < header_size < 10_000_000:
            raise ValueError("Unexpected ASAR header")
        tree = json.loads(file.read(json_size))
        entry = tree["files"]["entries"]["files"]["main.index.js"]
        if entry.get("unpacked"):
            raise ValueError("The launcher entry is outside the ASAR archive")
        file.seek(8 + header_size + int(entry["offset"]))
        source = file.read(int(entry["size"])).decode("utf-8", errors="replace")
    result = {"path": str(path.resolve()), "entry": "entries/main.index.js", "references": []}
    for term in ("product_name_click_url", "force_custom_location_text", "product.html"):
        offset = source.find(term)
        if offset >= 0:
            result["references"].append({"term": term, "line": source.count("\n", 0, offset) + 1,
                                         "character_offset": offset})
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--extension", type=Path)
    parser.add_argument("--compare-kernel", type=Path)
    parser.add_argument("--client-asar", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    report = {"kernel": inspect_kernel(arguments.kernel)}
    if arguments.extension:
        report["extension"] = inspect_extension(arguments.extension)
    if arguments.compare_kernel:
        report["comparison"] = inspect_kernel(arguments.compare_kernel)
    if arguments.client_asar:
        report["client"] = inspect_client(arguments.client_asar)
    report["limitations"] = [
        "Static evidence of schema registration and shipped JavaScript references, not a runtime invocation.",
        "A missing literal is not proof of absence in arbitrary packed or differently encoded binaries.",
        "Extension-ID restrictions and custom browser startup requirements have not been verified.",
        "Configuration-key strings are candidates, not verified command-line switches or a public API.",
    ]
    encoded = json.dumps(report, ensure_ascii=False, indent=2)
    if arguments.output:
        arguments.output.write_text(encoded + "\n", encoding="utf-8")
    else:
        print(encoded)


if __name__ == "__main__":
    main()
