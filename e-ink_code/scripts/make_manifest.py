import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = (ROOT / "version.txt").read_text().strip()
FIRMWARE_BIN = ROOT / "ota" / "firmware.bin"

# Get app name from argument or default to fun
APP_NAME = sys.argv[1] if len(sys.argv) > 1 else "fun"

# Map app names to OTA URL paths
APP_OTA_URL_PATH = {
    "fun": "fun_app",
    "shelf": "shelf",
    "sensor": "sensor",
    "messages": "messages"
}

# Validate app name
if APP_NAME not in APP_OTA_URL_PATH:
    print(f"Error: Invalid app name '{APP_NAME}'")
    print(f"Valid app names: {', '.join(APP_OTA_URL_PATH.keys())}")
    sys.exit(1)

OTA_URL_PATH = APP_OTA_URL_PATH[APP_NAME]

# Compute SHA256
sha256 = hashlib.sha256(FIRMWARE_BIN.read_bytes()).hexdigest()

manifest = {
    "version": VERSION,
    "sha256": sha256,
    "url": f"https://ota.denton.works/{OTA_URL_PATH}/firmware.bin"
}

(ROOT / "ota" / "manifest.json").write_text(json.dumps(manifest, indent=2))

print(f"make_manifest.py (version: {VERSION}, app: {APP_NAME}): manifest.json generated")
