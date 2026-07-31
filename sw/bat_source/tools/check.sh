#!/usr/bin/env bash
#
# Compile gate for the BatSource firmware.
#
# Syntax-compiles every application source file in Core/Src with -Wall -Wextra
# using the ARM toolchain bundled with STM32CubeIDE, and compares the resulting
# warning list against tools/warnings.baseline.
#
# This is deliberately -fsyntax-only: it needs no build directory, no linker
# script and no CubeIDE workspace, so it runs in a second and is safe to use as
# a pre-commit / per-change gate. It does NOT verify link-time properties
# (section sizes, stack headroom) -- use a full CubeIDE headless build for
# those.
#
# Usage:
#   tools/check.sh              compare against the baseline, fail if it grew
#   tools/check.sh --update     regenerate the baseline from the current tree
#
# Exit status: 0 if no new warnings (or --update), 1 otherwise.

set -uo pipefail

# sort and comm must agree on collation, otherwise comm reports "file is not in
# sorted order" and its set difference is unreliable. Pin both to byte order.
export LC_ALL=C

cd "$(dirname "$0")/.."
PROJECT_ROOT="$PWD"
BASELINE="tools/warnings.baseline"
CURRENT="$(mktemp)"
trap 'rm -f "$CURRENT"' EXIT

# Locate the CubeIDE-bundled cross compiler. Pinning the exact plugin directory
# would break on every IDE update, so glob for it and take the newest match.
# CC can be overridden from the environment for CI or a system toolchain.
if [ -z "${CC:-}" ]; then
    CC=$(ls -d /opt/st/stm32cubeide_*/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin/arm-none-eabi-gcc 2>/dev/null | sort -V | tail -1)
fi
if [ -z "$CC" ] || [ ! -x "$CC" ]; then
    echo "check.sh: arm-none-eabi-gcc not found." >&2
    echo "  Looked under /opt/st/stm32cubeide_*/plugins/...gnu-tools-for-stm32.*/tools/bin/" >&2
    echo "  Set CC=/path/to/arm-none-eabi-gcc to override." >&2
    exit 2
fi

# Must match the flags in .cproject, so the warning set here is the same one the
# real IDE build produces.
CFLAGS=(
    -mcpu=cortex-m4
    -std=gnu11
    -DUSE_HAL_DRIVER
    -DSTM32G474xx
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -mthumb
    -Wall
    -Wextra
    -fsyntax-only
)

INCLUDES=()
for dir in \
    Core/Inc \
    Drivers/STM32G4xx_HAL_Driver/Inc \
    Drivers/STM32G4xx_HAL_Driver/Inc/Legacy \
    Drivers/CMSIS/Device/ST/STM32G4xx/Include \
    Drivers/CMSIS/Include \
    Library/UGUI \
    Library/LCD
do
    [ -d "$dir" ] && INCLUDES+=("-I$dir")
done

failed=0
for src in Core/Src/*.c; do
    # Diagnostics are emitted with paths relative to PROJECT_ROOT so the
    # baseline stays stable regardless of where the script is invoked from.
    if ! "$CC" "${CFLAGS[@]}" "${INCLUDES[@]}" "$src" 2>>"$CURRENT"; then
        echo "check.sh: COMPILE ERROR in $src" >&2
        failed=1
    fi
done

# Keep only the diagnostic headline lines (file:line:col: warning/error: text),
# dropping the source-echo and caret lines GCC interleaves.
#
# Line and column numbers are deliberately STRIPPED. Comparing them would make
# the baseline shift whenever unrelated code above a warning grows or shrinks,
# which pressures anyone (human or agent) editing this tree to reshape source
# purely to keep line numbers stable -- exactly the wrong incentive. Comparing
# on (file, diagnostic text) instead is stable under insertions and still
# catches real regressions.
#
# Duplicates are preserved rather than collapsed, and comm() below does a
# multiset difference on sorted input, so adding a SECOND instance of an
# already-known warning in the same file is still reported as new.
normalize() {
    grep -E '^[^ ].*:[0-9]+:[0-9]+: (warning|error):' "$1" \
        | sed -E "s|^${PROJECT_ROOT}/||" \
        | sed -E 's|^([^:]+):[0-9]+:[0-9]+: |\1: |' \
        | sort
}

if [ "${1:-}" = "--update" ]; then
    normalize "$CURRENT" > "$BASELINE"
    echo "check.sh: baseline updated -- $(wc -l < "$BASELINE") diagnostics recorded."
    exit 0
fi

if [ ! -f "$BASELINE" ]; then
    echo "check.sh: no baseline at $BASELINE. Run 'tools/check.sh --update' first." >&2
    exit 2
fi

normalize "$CURRENT" > "$CURRENT.norm"

# Only *new* diagnostics fail the gate. Fixing warnings (lines present in the
# baseline but gone now) is always allowed and is reported as progress.
new=$(comm -13 "$BASELINE" "$CURRENT.norm")
fixed=$(comm -23 "$BASELINE" "$CURRENT.norm")

if [ -n "$fixed" ]; then
    echo "check.sh: $(echo "$fixed" | wc -l) diagnostic(s) resolved since the baseline:"
    echo "$fixed" | sed 's/^/  - /'
fi

if [ -n "$new" ]; then
    echo "check.sh: FAIL -- $(echo "$new" | wc -l) NEW diagnostic(s):" >&2
    echo "$new" | sed 's/^/  + /' >&2
    rm -f "$CURRENT.norm"
    exit 1
fi

rm -f "$CURRENT.norm"

if [ "$failed" -ne 0 ]; then
    echo "check.sh: FAIL -- compile errors above." >&2
    exit 1
fi

echo "check.sh: OK -- $(wc -l < "$BASELINE") known diagnostic(s), no new ones."
exit 0
