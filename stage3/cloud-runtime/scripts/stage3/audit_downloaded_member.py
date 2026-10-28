#!/usr/bin/env python3
"""Check an already downloaded IPSW member against Apple's ZIP64 directory."""
import argparse, hashlib, json, sys, zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fetch_verified_member import ARCHIVE_SIZE, URL, members  # noqa: E402

p = argparse.ArgumentParser()
p.add_argument("member")
p.add_argument("file", type=Path)
p.add_argument("--output", type=Path)
a = p.parse_args()
entry = next((e for e in members() if e[0] == a.member), None)
if entry is None:
    raise SystemExit(f"not in IPSW: {a.member}")
name, method, expected_crc, csize, usize, _ = entry
sha = hashlib.sha256(); crc = 0; size = 0
with a.file.open("rb") as stream:
    while chunk := stream.read(1 << 20):
        size += len(chunk); crc = zlib.crc32(chunk, crc); sha.update(chunk)
crc &= 0xffffffff
result = {"archive_url": URL, "archive_size": ARCHIVE_SIZE, "member": name,
          "compression_method": method, "compressed_size": csize, "size": size,
          "expected_size": usize, "zip_crc32": f"{crc:08x}",
          "expected_zip_crc32": f"{expected_crc:08x}", "sha256": sha.hexdigest(),
          "valid": size == usize and crc == expected_crc}
text = json.dumps(result, indent=2) + "\n"
if a.output:
    a.output.parent.mkdir(parents=True, exist_ok=True); a.output.write_text(text)
print(text, end="")
raise SystemExit(0 if result["valid"] else 1)
