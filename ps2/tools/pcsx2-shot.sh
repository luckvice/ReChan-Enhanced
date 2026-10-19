#!/usr/bin/env bash
# Boots an ELF in PCSX2 (visible window), waits until the ELF is actually
# executing (not the BIOS boot screen), settles a few seconds so the game can
# draw its first frames, captures the window twice as BMP and shuts PCSX2 down.
#
# Usage: bash ps2/tools/pcsx2-shot.sh <elf> <output-prefix>
#   writes <prefix>1.bmp and <prefix>2.bmp
#
# Env:
#   RECHAN_PS2_SETTLE   seconds to wait after the ELF starts (default 5)
#   RECHAN_PS2_TIMEOUT  seconds to wait for the BIOS+ELF boot (default 60)
#   PCSX2_BIN           path to the PCSX2 binary
#
# Needs Docker only for building (see ps2/build-remote.sh); here PCSX2 runs
# locally. The window is found through pcsx2-window-id.swift.
set -u

ELF="$1"
PREFIX="${2:-/tmp/pcsx2-shot}"
SETTLE="${RECHAN_PS2_SETTLE:-5}"
BOOT_TIMEOUT="${RECHAN_PS2_TIMEOUT:-60}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PCSX2="${PCSX2_BIN:-/Applications/PCSX2.app/Contents/MacOS/PCSX2}"
LOG="/tmp/pcsx2-$(basename "$PREFIX").log"

rm -f "$LOG" "${PREFIX}1.bmp" "${PREFIX}2.bmp"
"$PCSX2" -elf "$ELF" -logfile "$LOG" > "${LOG%.log}.out" 2>&1 &
PID=$!

# The PCSX2 window exists from the first BIOS frame: waiting for it alone
# captures the memory card browser instead of the game. Wait for the emulator
# log to report that our ELF is executing.
ELF_RUNNING=""
for i in $(seq 1 "$BOOT_TIMEOUT"); do
    sleep 1
    if grep -q "is executing" "$LOG" 2>/dev/null; then
        ELF_RUNNING="yes"
        echo "ELF executing after ~${i}s"
        break
    fi
    if ! kill -0 $PID 2>/dev/null; then
        echo "ERROR: PCSX2 exited before the ELF started"
        exit 1
    fi
done

if [ -z "$ELF_RUNNING" ]; then
    echo "ERROR: ELF did not start within ${BOOT_TIMEOUT}s (still in BIOS?)"
    kill $PID 2>/dev/null
    exit 1
fi

WIN=""
for i in $(seq 1 12); do
    WIN=$(swift "$HERE/pcsx2-window-id.swift" pcsx2 2>/dev/null \
          | awk -F'\t' '$2=="layer=0" && $3=="onscreen=true" { split($6, s, "x"); if (s[1] >= 600) { print $1; exit } }')
    if [ -n "$WIN" ]; then
        echo "PCSX2 window found: id=$WIN"
        break
    fi
    sleep 2
done

if [ -z "$WIN" ]; then
    echo "ERROR: PCSX2 game window not found"
    kill $PID 2>/dev/null
    exit 1
fi

# Give the engine time to run Game::Open() and draw real frames.
sleep "$SETTLE"
screencapture -x -l "$WIN" -t bmp "${PREFIX}1.bmp" && echo "capture 1 ok"
sleep 3
screencapture -x -l "$WIN" -t bmp "${PREFIX}2.bmp" && echo "capture 2 ok"
sleep 1
kill $PID 2>/dev/null; sleep 1; kill -9 $PID 2>/dev/null
echo "emulator log: $LOG"
