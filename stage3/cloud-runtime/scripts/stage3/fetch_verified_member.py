#!/usr/bin/env python3
"""Range-fetch one member of the pinned iOS 16.5 IPSW, with ZIP validation.

Apple payloads belong under an ignored output directory.  Alongside OUTPUT this
writes OUTPUT.metadata.json, which is safe to use as an audit input after the
payload itself has been removed.  Unlike upstream's resumable helper, every HTTP
response is checked and the completed member is checked against the central
directory's uncompressed size and CRC-32.
"""

import argparse
import hashlib
import json
import os
import struct
import urllib.request
import zlib
from pathlib import Path

URL = ("https://updates.cdn-apple.com/2023SpringFCS/fullrestores/032-85130/"
       "6AB0D10A-8AF9-4F6C-8CF5-A1B315CABE88/"
       "iPhone10,3,iPhone10,6_16.5_20F66_Restore.ipsw")
ARCHIVE_SIZE = 6_165_698_198


def get_range(start: int, end: int) -> bytes:
    request = urllib.request.Request(URL, headers={"Range": f"bytes={start}-{end}"})
    with urllib.request.urlopen(request, timeout=120) as response:
        expected = f"bytes {start}-{end}/{ARCHIVE_SIZE}"
        actual = response.headers.get("Content-Range")
        if response.status != 206 or actual != expected:
            raise RuntimeError(f"range {start}-{end}: HTTP {response.status}, Content-Range={actual!r}")
        data = response.read()
    if len(data) != end - start + 1:
        raise RuntimeError(f"range {start}-{end}: got {len(data)} bytes")
    return data


def members():
    tail_start = ARCHIVE_SIZE - 65_536
    tail = get_range(tail_start, ARCHIVE_SIZE - 1)
    at = tail.rfind(b"PK\x06\x06")
    if at < 0:
        raise RuntimeError("ZIP64 end record not found")
    cd_size, cd_offset = struct.unpack("<QQ", tail[at + 40:at + 56])
    cd = get_range(cd_offset, cd_offset + cd_size - 1)
    p = 0
    while cd[p:p + 4] == b"PK\x01\x02":
        method, crc, csize, usize, nl, el, cl = struct.unpack("<H4xIIIHHH", cd[p + 10:p + 34])
        offset, = struct.unpack("<I", cd[p + 42:p + 46])
        name = cd[p + 46:p + 46 + nl].decode()
        extra = cd[p + 46 + nl:p + 46 + nl + el]
        q = 0
        while q + 4 <= len(extra):
            tag, length = struct.unpack("<HH", extra[q:q + 4])
            if tag == 1:
                values = list(struct.unpack("<" + "Q" * (length // 8), extra[q + 4:q + 4 + length]))
                if usize == 0xffffffff: usize = values.pop(0)
                if csize == 0xffffffff: csize = values.pop(0)
                if offset == 0xffffffff: offset = values.pop(0)
            q += 4 + length
        yield name, method, crc, csize, usize, offset
        p += 46 + nl + el + cl


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("member")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    entry = next((e for e in members() if e[0] == args.member), None)
    if entry is None:
        raise SystemExit(f"not in IPSW: {args.member}")
    name, method, expected_crc, csize, usize, offset = entry
    if method not in (0, 8):
        raise SystemExit(f"unsupported ZIP compression method {method}")
    header = get_range(offset, offset + 29)
    if header[:4] != b"PK\x03\x04":
        raise RuntimeError("bad local header signature")
    nl, el = struct.unpack("<HH", header[26:30])
    compressed = get_range(offset + 30 + nl + el, offset + 30 + nl + el + csize - 1)
    payload = compressed if method == 0 else zlib.decompress(compressed, -15)
    crc = zlib.crc32(payload) & 0xffffffff
    if len(payload) != usize or crc != expected_crc:
        raise RuntimeError(f"validation failed: size={len(payload)}/{usize}, crc={crc:08x}/{expected_crc:08x}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    metadata = {
        "archive_url": URL, "archive_size": ARCHIVE_SIZE, "member": name,
        "compression_method": method, "compressed_size": csize,
        "size": usize, "zip_crc32": f"{crc:08x}",
        "sha256": hashlib.sha256(payload).hexdigest(),
    }
    Path(str(args.output) + ".metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, sort_keys=True))


if __name__ == "__main__":
    main()
