# PlatformIO CLI Cheat Sheet

# Clean build files

## FUN APP
pio run --target clean
pio run -e seeed_xiao_fun -t upload
pio device monitor

## SENSOR APP
pio run --target clean
pio run -e seeed_xiao_shelf_sensor -t upload
pio device monitor

# Start the ble monitor

cd scripts
python3 scan_ble.py
cd ..

## Installation

```bash
# Install PlatformIO CLI
python3 -m pip install --user platformio

# Verify installation
pio --version
```

## Project Management

```bash
# Initialize new project
pio project init

# Initialize with specific board
pio project init --board esp32dev

# Initialize with board and framework
pio project init --board esp32dev --project-dir myproject

# Show project configuration
pio project config

# Show project configuration as JSON
pio project config --json-output

# Show project structure
pio project structure
```

## Building

```bash
# Build project
pio run

# Build for specific environment
pio run -e esp32dev

# Build with verbose output
pio run -v

# Clean build files
pio run --target clean

# Clean all (including libraries)
pio run --target cleanall

# List all build targets
pio run --list-targets

# Build with specific build flags
pio run -e esp32dev -- -j4
```

## Uploading

```bash
# Upload firmware to device
pio run --target upload

# Upload to specific environment
pio run -e esp32dev --target upload

# Upload and monitor serial
pio run --target upload && pio device monitor

# Upload using specific port
pio run --target upload --upload-port /dev/ttyUSB0

# Upload with program option
pio run --target upload --upload-port /dev/ttyUSB0 --upload-program esptool
```

## Serial Monitor

```bash
# Monitor serial output
pio device monitor

# Monitor with specific baud rate
pio device monitor -b 115200

# Monitor with filters
pio device monitor --filter send_on_enter

# List available serial ports
pio device list

# Show device information
pio device info
```

## Library Management

```bash
# Search for libraries
pio lib search "library name"

# Install library
pio lib install "library name"

# Install specific version
pio lib install "library name@1.2.3"

# Install library from Git repository
pio lib install https://github.com/user/repo.git

# Install library from local path
pio lib install /path/to/library

# Update library
pio lib update

# Update specific library
pio lib update "library name"

# Uninstall library
pio lib uninstall "library name"

# List installed libraries
pio lib list

# Show library information
pio lib show "library name"

# Show library dependencies
pio lib deps
```

## Testing

```bash
# Run tests
pio test

# Run tests for specific environment
pio test -e esp32dev

# Run tests with verbose output
pio test -v

# Run specific test
pio test --filter "test_name"

# Upload and test
pio test --target upload
```

## Platform Management

```bash
# List installed platforms
pio platform list

# Show platform information
pio platform show espressif32

# Install platform
pio platform install espressif32

# Update platform
pio platform update

# Update specific platform
pio platform update espressif32

# Uninstall platform
pio platform uninstall espressif32

# Search platforms
pio platform search esp32
```

## Framework Management

```bash
# List frameworks
pio framework list

# Show framework information
pio framework show arduino
```

## Board Management

```bash
# List available boards
pio boards

# Search boards
pio boards esp32

# Show board information
pio boards esp32dev
```

## Package Management

```bash
# List installed packages
pio package list

# Show package information
pio package show

# Update packages
pio package update

# Search packages
pio package search
```

## Account & Cloud

```bash
# Login to PlatformIO account
pio account login

# Logout
pio account logout

# Show account information
pio account show

# Register new account
pio account register
```

## System Information

```bash
# Show system information
pio system info

# Check PlatformIO Core version
pio --version

# Show help
pio --help

# Show help for specific command
pio run --help
```

## Advanced Commands

```bash
# Run custom script
pio run --target custom_script

# Execute command in PlatformIO environment
pio exec --command "command here"

# Run with environment variables
pio run -e esp32dev -- -e CUSTOM_VAR=value

# Build with specific build type
pio run --target debug

# Show build environment
pio run --target envdump

# Show build flags
pio run --target flags
```

## Common Workflows

```bash
# Complete workflow: build, upload, monitor
pio run --target upload && pio device monitor

# Clean build and upload
pio run --target clean && pio run --target upload

# Build and show size
pio run && pio run --target size

# Update everything
pio platform update && pio lib update && pio package update
```

## Environment Variables

```bash
# Set environment variable for build
pio run -e esp32dev -- -e CUSTOM_VAR=value

# Use in platformio.ini
# [env:esp32dev]
# build_flags = -DCUSTOM_VAR=$CUSTOM_VAR
```

## Debugging

```bash
# Build with debug symbols
pio run --target debug

# Upload and start debugger
pio debug

# Debug with specific environment
pio debug -e esp32dev
```

## Useful Flags

```bash
# Verbose output
-v, --verbose

# Silent mode
-s, --silent

# Environment selection
-e, --environment

# Project directory
-d, --project-dir

# Configuration file
-c, --config-file

# Disable auto-upload
--no-upload

# Disable auto-monitor
--no-monitor
```

## Tips

- Use `pio run --target upload` to build and upload in one command
- Use `pio device monitor` after upload to see serial output
- Check `platformio.ini` for project-specific configuration
- Use `pio run --list-targets` to see all available build targets
- Libraries are automatically downloaded based on `platformio.ini`
- Use `pio run -v` for detailed build output when troubleshooting
