#!/usr/bin/env bash
#
# Host build and run of the config_store test (fake EEPROM, no target toolchain needed).
# Usage: tools/test_config_store.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -Itools/stub -ICore/Inc -ICore/Src \
    tools/test_config_store.c -o "$out/test_config_store"
"$out/test_config_store"
