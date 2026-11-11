#!/usr/bin/env python3
"""
Flashes a complete device in one go via STM32CubeProgrammer CLI:
  1. icon store image -> external W25N01GV (0x90000000, external loader)
  2. firmware ELF     -> internal flash, then reset

The icon image is built on the fly by flash_icon_seed.py from
icon_seed_data.c. EMI icon is included when debug.h defines EMI_DEBUG
(must match ICON_STORE_ICON_COUNT of the firmware).

Requires: Python 3, STM32CubeProgrammer (STM32_Programmer_CLI), ST-LINK.

Usage:
    python3 flash_device.py                 # icons + firmware (Debug build)
    python3 flash_device.py --fw-only       # firmware only
    python3 flash_device.py --icons-only    # icons only
    python3 flash_device.py --dry-run       # print the commands only
"""
import argparse
import glob
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, '..'))          # sw/bat_source
REPO = os.path.normpath(os.path.join(PROJ, '..', '..'))
SEED_SCRIPT = os.path.join(REPO, 'doc', 'icons', 'improved_icons', 'scripts',
                           'flash_icon_seed.py')
DEFAULT_ELF = os.path.join(PROJ, 'Debug', 'bat_source.elf')
DEFAULT_LOADER = os.path.join(PROJ, '..', 'W25N01GV_Bat-Source.stldr')
DEBUG_H = os.path.join(PROJ, 'Core', 'Inc', 'debug.h')

CLI_NAMES = ('STM32_Programmer_CLI', 'STM32_Programmer_CLI.exe')
CLI_GLOBS = (
    r'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin',
    r'C:\ST\STM32CubeIDE_*\STM32CubeIDE\plugins\*cubeprogrammer*\tools\bin',
    r'C:\ST\STM32CubeIDE*\plugins\*cubeprogrammer*\tools\bin',
    '/opt/st/stm32cubeide_*/plugins/*cubeprogrammer*/tools/bin',
    os.path.expanduser('~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin'),
    '/Applications/STMicroelectronics/STM32CubeProgrammer.app/Contents/MacOs/bin',
)


def find_cli(explicit):
    """Returns the STM32_Programmer_CLI path or exits.
    @param explicit  user-given path or None"""
    if explicit:
        return explicit
    for name in CLI_NAMES:
        path = shutil.which(name)
        if path:
            return path
    for pattern in CLI_GLOBS:
        for d in sorted(glob.glob(pattern), reverse=True):
            for name in CLI_NAMES:
                path = os.path.join(d, name)
                if os.path.isfile(path):
                    return path
    sys.exit('STM32_Programmer_CLI not found: add it to PATH or pass --cli')


def emi_enabled():
    """True when debug.h defines EMI_DEBUG (uncommented)."""
    with open(DEBUG_H) as f:
        return re.search(r'^\s*#define\s+EMI_DEBUG\b', f.read(), re.M) is not None


def build_icon_image(path, size, emi):
    """Writes the icon store image to `path` using flash_icon_seed.py.
    @param size  icon resolution (64/96), must match the firmware
    @param emi   include the EMI icon"""
    spec = importlib.util.spec_from_file_location('flash_icon_seed', SEED_SCRIPT)
    seed = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(seed)
    with open(seed.DEFAULT_SRC) as f:
        src = f.read()
    undefined = set() if emi else {'EMI_DEBUG'}
    block = seed.strip_conditionals(seed.extract_block(src, size), undefined)
    icons = seed.parse_icon_seeds(block)
    image, _, _ = seed.build_image(icons, size)
    with open(path, 'wb') as f:
        f.write(image)
    print(f'Icon image: {len(icons)} icons {size}x{size}, {len(image)} bytes'
          f'{" (with EMI)" if emi else ""}')
    return seed.ICON_STORE_BASE_ADDR


def run(cmd, dry):
    """Prints and runs one CLI command, exits on failure."""
    print('\n> ' + ' '.join(f'"{c}"' if ' ' in c else c for c in cmd))
    if dry:
        return
    if subprocess.call(cmd) != 0:
        sys.exit('FAILED')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--elf', default=DEFAULT_ELF, help='firmware ELF (default: Debug build)')
    ap.add_argument('--loader', default=DEFAULT_LOADER, help='external loader .stldr')
    ap.add_argument('--cli', help='path to STM32_Programmer_CLI')
    ap.add_argument('--port', default='SWD', help='programmer port (default: SWD)')
    ap.add_argument('--size', type=int, choices=(64, 96), default=96,
                    help='icon size, must match ICON_STORE_ICON_SIZE (default: 96)')
    emi = ap.add_mutually_exclusive_group()
    emi.add_argument('--emi', dest='emi', action='store_true', default=None,
                     help='force EMI icon on (default: from debug.h)')
    emi.add_argument('--no-emi', dest='emi', action='store_false',
                     help='force EMI icon off')
    only = ap.add_mutually_exclusive_group()
    only.add_argument('--icons-only', action='store_true', help='skip the firmware')
    only.add_argument('--fw-only', action='store_true', help='skip the icons')
    ap.add_argument('--dry-run', action='store_true', help='print commands only')
    args = ap.parse_args()

    cli = find_cli(args.cli) if not args.dry_run else (args.cli or CLI_NAMES[0])
    connect = ['-c', f'port={args.port}', 'mode=UR']   # connect under reset

    if not args.icons_only:
        if not os.path.isfile(args.elf):
            sys.exit(f'ELF not found: {args.elf} (build in CubeIDE first)')
        age = time.time() - os.path.getmtime(args.elf)
        print(f'Firmware: {args.elf} (built {age / 60:.0f} min ago)')

    with tempfile.TemporaryDirectory() as tmp:
        if not args.fw_only:
            if not os.path.isfile(args.loader):
                sys.exit(f'External loader not found: {args.loader}')
            use_emi = emi_enabled() if args.emi is None else args.emi
            img = os.path.join(tmp, 'icon_seed.bin')
            base = build_icon_image(img, args.size, use_emi)
            # Download erases the touched blocks via the loader's SectorErase.
            run([cli] + connect + ['-el', os.path.abspath(args.loader),
                                   '-w', img, f'0x{base:08X}', '-v'], args.dry_run)
        if not args.icons_only:
            run([cli] + connect + ['-w', os.path.abspath(args.elf), '-v', '-rst'],
                args.dry_run)

    print('\nDone.')


if __name__ == '__main__':
    main()
