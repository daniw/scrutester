"""
Builds a standalone binary image of the menu icon store, matching byte-for-
byte what the firmware's icon_store_flash_seed() (Core/Src/icon_store.c)
writes to the external W25N01GV QSPI NAND when the `flashIcons` CLI command
runs. Lets you provision/update the icon store directly with a hardware
programmer, without a firmware rebuild or the (currently disabled -
ICON_INCLUDE_SEED isn't defined in the shipped build) in-firmware path.

Reads icon_seed_data.c directly (the same file export_firmware.py writes)
so it can never drift from the actual compiled-in seed artwork - no pixel
data is duplicated into this script.

On-flash layout (see icon_store.c for the authoritative source):
    block 0            : icon_store_header_t (magic, version, icon_count,
                          9x {width, height, size, crc32}, header_crc32)
    block 1..9          : one icon's raw RGB565 pixel bytes each, padded
                          with 0xFF (NAND erased state) to the full block

All 10 blocks are emitted so the output is a complete, self-contained image
- writing it with a preceding full-range erase (or a programmer that erases
implicitly) reproduces exactly what icon_store_flash_seed() leaves behind.

Requires: Python 3 standard library only.

Usage:
    python3 flash_icon_seed.py                  # -> icon_seed.bin, 96x96 (matches the default firmware build)
    python3 flash_icon_seed.py --size 64 --out icon_seed_64.bin
    python3 flash_icon_seed.py --src /path/to/icon_seed_data.c

Flashing (STM32CubeProgrammer CLI, external loader already built in this
repo at sw/W25N01GV_Bat-Source.stldr - see sw/README.md for installing it):
    STM32_Programmer_CLI -c port=SWD -el sw/W25N01GV_Bat-Source.stldr \\
        -e all -w icon_seed.bin 0x90000000 -v
"""
import argparse
import os
import re
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = os.path.join(HERE, '..', '..', '..', '..', 'sw', 'bat_source',
                            'Core', 'Src', 'icon_seed_data.c')

ICON_STORE_MAGIC = 0x314E4349        # "ICN1", little-endian uint32
ICON_STORE_VERSION = 1
ICON_STORE_BASE_ADDR = 0x90000000    # QSPI memory-mapped base (Dev_Inf.c)
PAGE_SIZE = 2048
PAGES_PER_BLOCK = 64
BLOCK_SIZE = PAGE_SIZE * PAGES_PER_BLOCK  # 131072 bytes, one icon slot

# icon_store_crc32() in icon_store.c is the textbook reflected CRC-32
# (poly 0xEDB88320, init 0xFFFFFFFF, final ~crc) - exactly zlib.crc32().
def icon_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def extract_block(src: str, size: int) -> str:
    """Returns the #if ICON_STORE_ICON_SIZE == ICON_SIZE_<size> ... #endif
    block's contents. icon_seed_data.c has one such block per resolution,
    with identically-named arrays in each - so the whole file must never be
    regex-scanned directly, only this isolated slice."""
    marker = 'ICON_SIZE_64' if size == 64 else 'ICON_SIZE_96'
    pattern = re.compile(
        r'#if\s+ICON_STORE_ICON_SIZE\s*==\s*' + marker + r'\b(.*?)#endif',
        re.DOTALL)
    m = pattern.search(src)
    if not m:
        raise SystemExit(f"Couldn't find a '#if ICON_STORE_ICON_SIZE == {marker}' "
                          f"block in the source file - has icon_seed_data.c's "
                          f"structure changed?")
    return m.group(1)


def parse_icon_seeds(block: str):
    """Returns an ordered list of (array_name, pixel_bytes) matching the
    icon_seeds[] initializer's order - that order is exactly the icon_id
    each icon lands under in the store, so it must be preserved exactly."""
    seeds_match = re.search(
        r'icon_seeds\[ICON_STORE_ICON_COUNT\]\s*=\s*\{(.*?)\};',
        block, re.DOTALL)
    if not seeds_match:
        raise SystemExit("Couldn't find the icon_seeds[] initializer in the "
                          "selected block.")
    names = re.findall(r'icon_(\w+)_data', seeds_match.group(1))
    if not names:
        raise SystemExit("icon_seeds[] initializer matched but no "
                          "icon_<name>_data references were found in it.")

    arrays = {}
    for m in re.finditer(
            r'static const uint16_t icon_(\w+)_data\[(\d+)\]\s*=\s*\{(.*?)\};',
            block, re.DOTALL):
        name, count, body = m.group(1), int(m.group(2)), m.group(3)
        values = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+', body)]
        if len(values) != count:
            raise SystemExit(f"icon_{name}_data declares [{count}] but "
                              f"{len(values)} values were parsed - "
                              f"regex extraction is out of sync with the "
                              f"source format.")
        # Each uint16_t (RGB565 sample) as the STM32 (little-endian) would
        # lay it out in memory - this is exactly what
        # (uint8_t*)seed->pixels reinterprets when written to flash.
        arrays[name] = struct.pack(f'<{count}H', *values)

    missing = [n for n in names if n not in arrays]
    if missing:
        raise SystemExit(f"icon_seeds[] references arrays not found in this "
                          f"block: {missing}")
    return [(n, arrays[n]) for n in names]


def build_image(icons, size: int) -> bytes:
    icon_count = len(icons)
    index_entries = []
    slot_bytes = []
    for name, pixels in icons:
        expected = size * size * 2
        if len(pixels) != expected:
            raise SystemExit(f"icon_{name}_data is {len(pixels)} bytes, "
                              f"expected {expected} for {size}x{size} "
                              f"RGB565 - width/height mismatch.")
        if len(pixels) > BLOCK_SIZE:
            raise SystemExit(f"icon_{name}_data ({len(pixels)} bytes) "
                              f"doesn't fit in one {BLOCK_SIZE}-byte slot "
                              f"(matches icon_store_flash_seed()'s own "
                              f"size > ICON_SLOT_SIZE check).")
        crc = icon_crc32(pixels)
        index_entries.append((size, size, len(pixels), crc))
        # 0xFF-pad to a full slot, matching NAND's erased state - the real
        # firmware's w25n01gv_write() only writes `size` bytes after an
        # erase, leaving the rest of the block at 0xFF.
        slot_bytes.append(pixels + b'\xff' * (BLOCK_SIZE - len(pixels)))

    header_no_crc = struct.pack('<IHH', ICON_STORE_MAGIC, ICON_STORE_VERSION,
                                 icon_count)
    for w, h, sz, crc in index_entries:
        header_no_crc += struct.pack('<HHII', w, h, sz, crc)
    header_crc = icon_crc32(header_no_crc)
    header = header_no_crc + struct.pack('<I', header_crc)

    if len(header) > BLOCK_SIZE:
        raise SystemExit("Header no longer fits in one block - "
                          "icon_store_header_t must have grown "
                          "(more icons?); this script needs updating to "
                          "match.")
    header_block = header + b'\xff' * (BLOCK_SIZE - len(header))

    image = header_block + b''.join(slot_bytes)
    return image, header, index_entries


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--src', default=DEFAULT_SRC,
                     help='path to icon_seed_data.c (default: repo-relative)')
    ap.add_argument('--size', type=int, choices=(64, 96), default=96,
                     help='icon resolution to package - must match '
                          'ICON_STORE_ICON_SIZE in the firmware build '
                          '(default: 96, the firmware default)')
    ap.add_argument('--out', default='icon_seed.bin',
                     help='output binary path (default: icon_seed.bin)')
    args = ap.parse_args()

    with open(args.src, 'r') as f:
        src = f.read()

    block = extract_block(src, args.size)
    icons = parse_icon_seeds(block)
    image, header, index_entries = build_image(icons, args.size)

    with open(args.out, 'wb') as f:
        f.write(image)

    print(f"Wrote {args.out}: {len(image)} bytes "
          f"({len(image) // BLOCK_SIZE} blocks x {BLOCK_SIZE} bytes)")
    print(f"Header: magic=0x{ICON_STORE_MAGIC:08X} version={ICON_STORE_VERSION} "
          f"icon_count={len(index_entries)} header_size={len(header)} bytes")
    for i, ((name, _), (w, h, sz, crc)) in enumerate(zip(icons, index_entries)):
        print(f"  [{i}] icon_{name}_data  {w}x{h}  {sz} bytes  "
              f"crc32=0x{crc:08X}  @ flash offset 0x{(i + 1) * BLOCK_SIZE:06X}")
    print()
    print("Flash it with (adjust -c port= for your programmer):")
    print(f"  STM32_Programmer_CLI -c port=SWD "
          f"-el sw/W25N01GV_Bat-Source.stldr "
          f"-e all -w {args.out} 0x{ICON_STORE_BASE_ADDR:08X} -v")


if __name__ == '__main__':
    main()
