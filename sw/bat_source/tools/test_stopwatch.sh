#!/usr/bin/env bash
#
# Host build and run of the stopwatch unit test (no target toolchain needed).
# Usage: tools/test_stopwatch.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc \
    tools/test_stopwatch.c Core/Src/stopwatch.c -o "$out/test_stopwatch"
"$out/test_stopwatch"
