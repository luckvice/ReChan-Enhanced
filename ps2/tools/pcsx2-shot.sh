#!/usr/bin/env bash
# Boots an ELF in PCSX2 (visible window), waits for the game window and
# captures it twice as BMP, then shuts the emulator down.
#
# Usage: bash ps2/tools/pcsx2-shot.sh <elf> <output-prefix>
#   writes <prefix>1.bmp and <prefix>2.bmp
#
# Needs Docker only for building (see ps2/build-remote.sh); here PCSX2 runs
# locally. The window is found through pcsx2-window-id.swift.
set -u

ELF="$1"
PREFIX="${2:-/tmp/pcsx2-shot}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PCSX2="${PCSX2_BIN:-/Applications/PCSX2.app/Contents/MacOS/PCSX2}"
LOG="/tmp/pcsx2-$(basename "$PREFIX").log"

rm -f "$LOG" "${PREFIX}1.bmp" "${PREFIX}2.bmp"
"$PCSX2" -elf "$ELF" -logfile "$LOG" > "${LOG%.log}.out" 2>&1 &
PID=$!

WIN=""
for i in $(seq 1 24); do
    sleep 2
    WIN=$(swift "$HERE/pcsx2-window-id.swift" pcsx2 2>/dev/null \
          | awk -F'\t' '$2=="layer=0" && $3=="onscreen=true" { split($6, s, "x"); if (s[1] >= 600) { print $1; exit } }')
    if [ -n "$WIN" ]; then
        echo "PCSX2 window found after ~$((i*2))s: id=$WIN"
        break
    fi
done

if [ -z "$WIN" ]; then
    echo "ERROR: PCSX2 game window not found"
    kill $PID 2>/dev/null
    exit 1
fi

sleep 2
screencapture -x -l "$WIN" -t bmp "${PREFIX}1.bmp" && echo "capture 1 ok"
sleep 3
screencapture -x -l "$WIN" -t bmp "${PREFIX}2.bmp" && echo "capture 2 ok"
sleep 1
kill $PID 2>/dev/null; sleep 1; kill -9 $PID 2>/dev/null
echo "emulator log: $LOG"
