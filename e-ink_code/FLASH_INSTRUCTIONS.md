# Flashing ESP32-C3 Firmware Manually

## Installing esptool

You have a few options to install `esptool.py`:

### Option 1: Install via pipx (Recommended)
```bash
# First install pipx (if not already installed)
brew install pipx
pipx ensurepath

# Then install esptool
pipx install esptool
```

After installation, restart your terminal or run `source ~/.zshrc` to update PATH.

### Option 2: Install via pip with --break-system-packages
```bash
pip3 install --break-system-packages esptool
```

### Option 3: Use PlatformIO's esptool (if dependencies are available)
PlatformIO includes esptool, but it requires pyserial. You can try:
```bash
pip3 install --break-system-packages pyserial
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32c3 ...
```

## Flashing Command

Once `esptool` is installed, navigate to your build directory and run:

**For esptool v5+ (new syntax with hyphen):**
```bash
cd /Users/adenton/Desktop/e-ink-display/e-ink_code/.pio/build/seeed_xiao_shelf_sensor

esptool --chip esp32c3 --port /dev/cu.usbmodem101 --flash-size 4MB write-flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

**For esptool v4 and earlier (old syntax with underscore):**
```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodem101 --flash_size 4MB write_flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

**Note:** If `esptool` is not in your PATH, use the full path:
```bash
~/.local/bin/esptool --chip esp32c3 --port /dev/cu.usbmodem101 --flash-size 4MB write-flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

## Flash Addresses Explained

- **0x0**: Bootloader starts here (first 32KB)
- **0x8000**: Partition table at 32KB offset (standard ESP32-C3 location, takes 4KB)
- **0x10000**: Application firmware starts at 64KB, matching your `partitions.csv` where `ota_0` starts

## Using the Helper Script

Alternatively, use the provided script (after installing esptool):

```bash
./scripts/flash_firmware.sh [PORT]
```

Example:
```bash
./scripts/flash_firmware.sh /dev/cu.usbmodem101
```

## Troubleshooting

- **Port not found**: Check available ports with `pio device list` or `ls /dev/cu.*`
- **Permission denied**: You may need to add your user to the dialout group or use `sudo` (not recommended)
- **Flash fails**: Make sure the device is in bootloader mode (hold BOOT button while connecting)
