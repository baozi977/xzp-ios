#!/usr/bin/env python3
"""Verify that an Apple BuildManifest is the pinned iPhone X / 20F66 input."""
import argparse, json, plistlib
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("manifest", type=Path)
p.add_argument("--output", type=Path)
a = p.parse_args()
m = plistlib.loads(a.manifest.read_bytes())
products = sorted(m.get("SupportedProductTypes", []))
identities = m.get("BuildIdentities", [])
models = sorted({i.get("Info", {}).get("DeviceClass") for i in identities})
components = {}
for identity in identities:
    for name, item in identity.get("Manifest", {}).items():
        path = item.get("Info", {}).get("Path")
        if path:
            components.setdefault(name, set()).add(path)
required = {"OS", "Cryptex1,SystemOS", "Cryptex1,AppOS"}
result = {
    "product_build_version": m.get("ProductBuildVersion"),
    "product_version": m.get("ProductVersion"),
    "supported_product_types": products,
    "device_classes": models,
    "build_identity_count": len(identities),
    "selected_components": {k: sorted(components.get(k, [])) for k in sorted(required)},
}
valid = (result["product_build_version"] == "20F66" and result["product_version"] == "16.5"
         and {"iPhone10,3", "iPhone10,6"}.issubset(products)
         and all(components.get(k) for k in required))
result["valid"] = valid
text = json.dumps(result, indent=2) + "\n"
if a.output:
    a.output.parent.mkdir(parents=True, exist_ok=True); a.output.write_text(text)
print(text, end="")
raise SystemExit(0 if valid else 1)
