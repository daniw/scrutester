#!/usr/bin/env bash
#
# Host build and run of the input wrap unit test (no target toolchain needed).
# Usage: tools/test_input_wrap.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -Itools/stub -ICore/Inc \
    tools/test_input_wrap.c Core/Src/input.c -o "$out/test_input_wrap"
"$out/test_input_wrap"
