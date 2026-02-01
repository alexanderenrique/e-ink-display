#!/usr/bin/env bash
# Build, create manifest (version + sha256), and SCP to Pi OTA server.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
OTA_HOST="adenton@10.0.0.2"
OTA_PATH="/srv/ota/xiao_test"

cd "$ROOT"
VERSION="$(cat version.txt)"
echo "=== deploy_ota.sh (version: $VERSION) ==="

echo "=== Building firmware ==="
pio run

echo "=== Copying firmware to ota/ ==="
python3 scripts/post_build.py

echo "=== Creating manifest (version + sha256) ==="
python3 scripts/make_manifest.py

echo "=== Uploading to $OTA_HOST:$OTA_PATH ==="
scp ota/firmware.bin ota/manifest.json "$OTA_HOST:$OTA_PATH/"

echo "Done."
