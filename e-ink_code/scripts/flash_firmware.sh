#!/usr/bin/env bash
# Flash ESP32-C3 firmware using esptool via PlatformIO's Python environment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT/.pio/build/seeed_xiao_shelf_sensor"

# Default port (can be overridden)
PORT="${1:-/dev/cu.usbmodem101}"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please build the project first: pio run -e seeed_xiao_shelf_sensor"
    exit 1
fi

# Check if binary files exist
cd "$BUILD_DIR"
for file in bootloader.bin partitions.bin firmware.bin; do
    if [ ! -f "$file" ]; then
        echo "Error: $file not found in $BUILD_DIR"
        exit 1
    fi
done

# Find PlatformIO's Python (usually uses system Python with packages installed)
# Try to use PlatformIO's esptool module
PYTHON_CMD="python3"

# Try to find esptool (new version uses 'esptool' command, old uses 'esptool.py')
ESPTOOL_CMD=""
if command -v esptool &> /dev/null; then
    ESPTOOL_CMD="esptool"
elif command -v esptool.py &> /dev/null; then
    ESPTOOL_CMD="esptool.py"
elif [ -f ~/.local/bin/esptool ]; then
    ESPTOOL_CMD="$HOME/.local/bin/esptool"
elif [ -f ~/.local/bin/esptool.py ]; then
    ESPTOOL_CMD="$HOME/.local/bin/esptool.py"
else
    echo "esptool not found. Installing via pipx (recommended) or pip..."
    echo ""
    echo "Option 1 (Recommended): Install pipx and esptool:"
    echo "  brew install pipx"
    echo "  pipx install esptool"
    echo ""
    echo "Option 2: Install esptool with --break-system-packages:"
    echo "  pip3 install --break-system-packages esptool"
    echo ""
    echo "After installing, restart your terminal or run: source ~/.zshrc"
    exit 1
fi

echo "Using: $ESPTOOL_CMD"
echo "Flashing firmware to ESP32-C3 on $PORT..."
echo "Files:"
ls -lh bootloader.bin partitions.bin firmware.bin

# Check esptool version to determine syntax
ESPTOOL_VERSION=$($ESPTOOL_CMD --version 2>&1 | grep -oP 'v\d+\.\d+' | head -1 || echo "")
if [[ "$ESPTOOL_VERSION" > "v5.0" ]] || [[ "$ESPTOOL_CMD" == *"esptool" ]] && [[ "$ESPTOOL_CMD" != *"esptool.py" ]]; then
    # New esptool v5+ syntax: esptool --chip X --port Y write-flash address file ...
    $ESPTOOL_CMD --chip esp32c3 --port "$PORT" --flash-size 4MB write-flash \
      0x0 bootloader.bin \
      0x8000 partitions.bin \
      0x10000 firmware.bin
else
    # Old esptool syntax: esptool.py --chip X --port Y write_flash address file ...
    $ESPTOOL_CMD --chip esp32c3 --port "$PORT" --flash_size 4MB write_flash \
      0x0 bootloader.bin \
      0x8000 partitions.bin \
      0x10000 firmware.bin
fi

echo ""
echo "Flash complete!"
