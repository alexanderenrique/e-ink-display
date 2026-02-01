from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
try:
    import env
    build_dir = Path(env.subst("$BUILD_DIR"))
except ImportError:
    # Run standalone: use project root relative to this script
    build_dir = ROOT / ".pio" / "build" / "seeed_xiao_esp32c3"

firmware = build_dir / "firmware.bin"

# Where you want OTA artifacts
ota_dir = ROOT / "ota"
ota_dir.mkdir(exist_ok=True)

dest = ota_dir / "firmware.bin"

version = (ROOT / "version.txt").read_text().strip()
print(f"post_build.py (version: {version}): Copying {firmware} → {dest}")
shutil.copy2(firmware, dest)
