#!/bin/sh
# Runs the rain benchmark on a PSP connected through PSPLink.
#
#   sh scripts/run_psp_rain_bench.sh NAME
#
# Expects dist/psp-debug/host/NAME.prx, built in usb-debug mode with
# PSP_AUTO_START_PHASE1 PSP_BENCH_FIXED_STEP PSP_BENCH_COMBAT PSP_BENCH_RAIN
# PSP_TRIPLE_BUFFER, and usbhostfs_pc serving dist/psp-debug/host
# (sh scripts/psp_usb.sh usb). The game data must be installed in
# ms0:/PSP/GAME/RECHAN. Reports and captures land in dist/psp-debug/host/NAME/;
# read them with scripts/psp_rain_bench_report.mjs.
#
# PSPLink is only ever reset or closed here: "exit" would end the session.
set -eu
name="${1:?usage: sh scripts/run_psp_rain_bench.sh NAME}"
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
pspsh="$root/build/psp-usb-tools/pspdev/bin/pspsh"
host="$root/dist/psp-debug/host"
out="$host/$name"
data=ms0:/PSP/GAME/RECHAN/data
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

[ -f "$host/$name.prx" ] || { echo "Missing $host/$name.prx" >&2; exit 1; }

# One command script per call: pspsh stops a script at the first failing
# command (a missing file), and the alarm keeps a stuck shell from hanging.
psh() {
    printf '%s\nclose\n' "$2" > "$work/cmd.psh"
    perl -e "alarm $1; exec @ARGV" "$pspsh" -i 127.0.0.1 -h "$work/hist" "$work/cmd.psh" 2>&1 \
        | sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' || true
}
reset_psplink() {
    printf 'reset\n' > "$work/cmd.psh"
    perl -e 'alarm 15; exec @ARGV' "$pspsh" -i 127.0.0.1 -h "$work/hist" "$work/cmd.psh" >/dev/null 2>&1 || true
    for _ in $(seq 1 20); do
        sleep 6
        psh 15 "ls $data" | grep -q 'Listing directory' && return 0
    done
    echo "PSPLink did not come back after reset" >&2
    return 1
}

files=""
for petal in 0 1 2; do
    for config in 0 1 2 3 4; do
        files="$files PSP_RAIN_P${petal}_C${config}.TXT SHOT_P${petal}C${config}.BMP"
    done
done

# Start from a clean PSPLink and no reports from a previous run.
reset_psplink
for file in $files; do psh 20 "rm $data/$file" >/dev/null; done
if psh 30 "ls $data" | grep -q ' PSP_RAIN_P'; then
    echo "Stale rain reports are still on the Memory Stick" >&2
    exit 1
fi

start=$(psh 60 "ldstart host0:/$name.prx" | grep -E 'Load/Start|Failed' || true)
echo "$(date +%H:%M:%S) $name start: $start"
echo "$start" | grep -q 'UID: 0x' || { echo "$name: load failed" >&2; exit 1; }

# 15 scenarios: a petal load plus 780 frames each, about 8 minutes in all.
mkdir -p "$out"
finished=0
t0=$(date +%s)
while [ $(( $(date +%s) - t0 )) -lt 1500 ]; do
    sleep 30
    listing=$(psh 30 "ls $data")
    count=$(echo "$listing" | grep -cE ' PSP_RAIN_P[0-2]_C[0-4].TXT' || true)
    echo "$(date +%H:%M:%S) $name reports=$count/15"
    if echo "$listing" | grep -q ' PSP_RAIN_P2_C4.TXT'; then finished=1; break; fi
done

if [ "$finished" = 0 ]; then
    # Hang: keep every thread's state before the reset.
    psh 60 "thlist" > "$out/hang-thlist.txt"
    echo "$(date +%H:%M:%S) $name did not finish; thread list in $out/hang-thlist.txt"
fi
for file in $files; do
    psh 30 "cp $data/$file host0:/$name/$file" >/dev/null
done
psh 30 "scrshot host0:/$name/end.bmp" >/dev/null
reset_psplink
echo "$(date +%H:%M:%S) $name finished=$finished; files in $out"
[ "$finished" = 1 ]
