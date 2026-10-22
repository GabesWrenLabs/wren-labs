"""Build the no-install flash kit: one merged .bin that flashes at address 0x0 on a Heltec WiFi LoRa 32 V3.

Run from tank/firmware after `pio run`:
    python tools/make_release.py [version]

Output: release/wren-range-<version>.bin plus the raw parts for esptool's four-address form.
Uses the esptool and boot_app0.bin that PlatformIO already installed.
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIO_PKGS = os.path.join(os.path.expanduser("~"), ".platformio", "packages")
ESPTOOL = os.path.join(PIO_PKGS, "tool-esptoolpy", "esptool.py")
BOOT_APP0 = os.path.join(PIO_PKGS, "framework-arduinoespressif32", "tools", "partitions", "boot_app0.bin")

version = sys.argv[1] if len(sys.argv) > 1 else "v0.1"
release = os.path.join(HERE, "release")
os.makedirs(release, exist_ok=True)
shutil.copy(BOOT_APP0, os.path.join(release, "boot_app0.bin"))

build = os.path.join(HERE, ".pio", "build", "heltec_v3")
for name in ("bootloader.bin", "partitions.bin", "firmware.bin"):
    shutil.copy(os.path.join(build, name), os.path.join(release, name))
out = os.path.join(release, f"wren-range-{version}.bin")
cmd = [
    sys.executable, ESPTOOL, "--chip", "esp32s3", "merge_bin", "-o", out,
    "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "8MB",
    "0x0", os.path.join(build, "bootloader.bin"),
    "0x8000", os.path.join(build, "partitions.bin"),
    "0xe000", os.path.join(release, "boot_app0.bin"),
    "0x10000", os.path.join(build, "firmware.bin"),
]
subprocess.run(cmd, check=True)
print(f"{out} ({os.path.getsize(out)} bytes)")
