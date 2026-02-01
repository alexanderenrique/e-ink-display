import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = (ROOT / "version.txt").read_text().strip()
FIRMWARE_BIN = ROOT / "ota" / "firmware.bin"

# Compute SHA256
sha256 = hashlib.sha256(FIRMWARE_BIN.read_bytes()).hexdigest()

manifest = {
    "version": VERSION,
    "sha256": sha256,
    "url": "https://ota.denton.works/xiao_test/firmware.bin"
}

(ROOT / "ota" / "manifest.json").write_text(json.dumps(manifest, indent=2))

print(f"make_manifest.py (version: {VERSION}): manifest.json generated")
