#!/usr/bin/env bash
# Build, create manifest (version + sha256), and SCP to Pi OTA server.
# Usage: ./scripts/deploy_ota.sh [app_name]
# App names: fun, shelf, sensor, messages
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
OTA_HOST="adenton@10.0.0.2"

# Get app name from argument or default to fun
APP_NAME="${1:-fun}"

# Map app names to PlatformIO environments and OTA paths
case "$APP_NAME" in
    fun)
        ENV_NAME="seeed_xiao_fun"
        OTA_PATH="/srv/ota/fun_app"
        ;;
    shelf)
        ENV_NAME="seeed_xiao_shelf"
        OTA_PATH="/srv/ota/shelf"
        ;;
    sensor)
        ENV_NAME="seeed_xiao_sensor"
        OTA_PATH="/srv/ota/sensor"
        ;;
    messages)
        ENV_NAME="seeed_xiao_messages"
        OTA_PATH="/srv/ota/messages"
        ;;
    *)
        echo "Error: Invalid app name '$APP_NAME'"
        echo "Valid app names: fun, shelf, sensor, messages"
        exit 1
        ;;
esac

cd "$ROOT"
VERSION="$(cat version.txt)"
echo "=== deploy_ota.sh (version: $VERSION, app: $APP_NAME) ==="

echo "=== Building firmware for $APP_NAME (env: $ENV_NAME) ==="
pio run -e "$ENV_NAME"

echo "=== Copying firmware to ota/ ==="
python3 scripts/post_build.py "$ENV_NAME"

echo "=== Creating manifest (version + sha256) ==="
python3 scripts/make_manifest.py "$APP_NAME"

echo "=== Uploading to $OTA_HOST:$OTA_PATH ==="
scp ota/firmware.bin ota/manifest.json "$OTA_HOST:$OTA_PATH/"

echo "Done."
