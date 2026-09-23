#!/usr/bin/env bash
#
# Host build and run of the charge_seq unit test (no target toolchain needed).
# Usage: tools/test_charge_seq.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc \
    tools/test_charge_seq.c Core/Src/charge_seq.c -lm -o "$out/test_charge_seq"
"$out/test_charge_seq"
