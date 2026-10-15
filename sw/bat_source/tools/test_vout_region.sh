#!/usr/bin/env bash
#
# Host build and run of the vout_region unit test (no target toolchain needed).
# Usage: tools/test_vout_region.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc \
    tools/test_vout_region.c Core/Src/vout_region.c -lm -o "$out/test_vout_region"
"$out/test_vout_region"
