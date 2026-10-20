#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=$(mktemp -d "${TMPDIR:-/tmp}/rechan-psp-hub-trigger.XXXXXX")
trap 'rm -rf "$output"' EXIT HUP INT TERM
case "$(uname -s)" in
    Darwin) gc_sections="-Wl,-dead_strip" ;;
    *) gc_sections="-Wl,--gc-sections" ;;
esac

"${CXX:-c++}" -std=c++17 -O1 -g -DNDEBUG -DRC_PLATFORM_PSP \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra \
    -Wno-unused-parameter -Wno-nontrivial-memcall \
    -ffunction-sections -fdata-sections "$gc_sections" \
    -I"$root/src" -I"$root/vendor/libp3d" -I"$root/vendor/cgltf" -I"$root/vendor/stb" \
    "$root/tests/psp/hub_sewer_trigger_test.cpp" \
    "$root/src/gen/blockmgr.cpp" \
    "$root/src/gen/block.cpp" \
    "$root/src/gen/colvol.cpp" \
    "$root/src/ai/fevolume.cpp" \
    "$root/src/ai/teleporter.cpp" \
    "$root/src/ai/ladder.cpp" \
    "$root/src/gen/database.cpp" \
    -o "$output/hub_sewer_trigger"

"$output/hub_sewer_trigger"
