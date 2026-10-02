#!/bin/sh
# Builds the firmware and merges it into ONE image that flashes at 0x0:
#
#   tools/release/build-bin.sh [version]      # from the repo root; default version: git describe
#
# Output: dist/morse-tree-<version>-esp32c6-16mb.bin (+ .sha256). The image holds the bootloader
# and the partition table too, so it works on a board that ran anything before. Flash it from
# a browser (README, "Install") or with
#   esptool.py --chip esp32c6 write_flash 0x0 dist/morse-tree-<version>-esp32c6-16mb.bin
set -eu
cd "$(dirname "$0")/../.."
ROOT=$(pwd)
VER=${1:-$(git describe --tags --always --dirty)}
OUT="$ROOT/dist/morse-tree-$VER-esp32c6-16mb.bin"
B="$ROOT/firmware/.pio/build/morse"
PIO=${PIO:-pio}

(cd firmware && "$PIO" run -e morse)
BOOT_APP0=$(find "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages/framework-arduinoespressif32" -name boot_app0.bin | head -1)
[ -n "$BOOT_APP0" ] || { echo "boot_app0.bin not found in the Arduino framework package" >&2; exit 1; }

mkdir -p "$ROOT/dist"
# Offsets as PlatformIO flashes them for this board (firmware/no_ota_16mb.csv: app at 0x10000).
"$PIO" pkg exec -p tool-esptoolpy -- esptool.py --chip esp32c6 merge_bin -o "$OUT" \
  --flash_mode keep --flash_freq keep --flash_size 16MB \
  0x0     "$B/bootloader.bin" \
  0x8000  "$B/partitions.bin" \
  0xe000  "$BOOT_APP0" \
  0x10000 "$B/firmware.bin"
SUM="shasum -a 256"; command -v shasum >/dev/null 2>&1 || SUM=sha256sum
(cd "$ROOT/dist" && $SUM "$(basename "$OUT")" > "$(basename "$OUT").sha256")
echo "$OUT"
