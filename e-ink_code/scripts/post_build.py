from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
try:
    import env
    build_dir = Path(env.subst("$BUILD_DIR"))
except ImportError:
    # Run standalone: use project root relative to this script
    # Get environment name from command line argument or default
    env_name = sys.argv[1] if len(sys.argv) > 1 else "seeed_xiao_esp32c3"
    build_dir = ROOT / ".pio" / "build" / env_name

firmware = build_dir / "firmware.bin"

# Where you want OTA artifacts
ota_dir = ROOT / "ota"
ota_dir.mkdir(exist_ok=True)

dest = ota_dir / "firmware.bin"

version = (ROOT / "version.txt").read_text().strip()
print(f"post_build.py (version: {version}): Copying {firmware} → {dest}")
shutil.copy2(firmware, dest)
