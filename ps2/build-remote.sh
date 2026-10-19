#!/usr/bin/env bash
# Builds the PS2 target inside the official ps2dev container on a remote Docker
# host and brings the ELF back to dist/ps2/.
#
# Remote because the EE toolchain is not available on the macOS host and the
# local disk is tight. Same host the PSP build uses.
#
# Usage:
#   bash ps2/build-remote.sh
# Variables: RECHAN_PS2_HOST, RECHAN_PS2_IMAGE, RECHAN_PS2_DIR,
#            RECHAN_PS2_TARGET.

set -euo pipefail

HOST="${RECHAN_PS2_HOST:-dev@192.168.18.24}"
IMAGE="${RECHAN_PS2_IMAGE:-ghcr.io/ps2dev/ps2dev:latest}"
REMOTE_DIR="${RECHAN_PS2_DIR:-/tmp/rechan-ps2}"
TARGET="${RECHAN_PS2_TARGET:-rechan}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/dist/ps2"
TARBALL="/tmp/rechan-ps2-src.$$.tgz"

mkdir -p "$OUT"

# The target plus the libp3d sources it compiles: the pddi interface headers,
# the ps2 backend and the maths headers core.h pulls in. vendor/libp3d/vendor
# (79 MB of third-party trees) is not needed to build this target and is left
# out on purpose.
tar --exclude='*.o' -C "$ROOT" -czf "$TARBALL" \
    ps2 \
    src \
    vendor/libp3d/core.h \
    vendor/libp3d/p3d \
    vendor/libp3d/pddi \
    vendor/libp3d/vendor/stb \
    vendor/stb
trap 'rm -f "$TARBALL"' EXIT

echo "==> sending sources to $HOST:$REMOTE_DIR"
ssh "$HOST" "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR"
scp -q "$TARBALL" "$HOST:$REMOTE_DIR/src.tgz"

echo "==> building in container $IMAGE"
# The ps2dev image is Alpine and ships no make; install it in the container.
ssh "$HOST" "cd $REMOTE_DIR && tar xzf src.tgz && rm -f src.tgz && docker run --rm -v $REMOTE_DIR:/work -w /work/ps2 $IMAGE sh -c 'apk add --no-cache make >/dev/null && make TARGET=$TARGET' 2>&1"

echo "==> fetching $TARGET.elf into dist/ps2/"
scp -q "$HOST:$REMOTE_DIR/ps2/$TARGET.elf" "$OUT/$TARGET.elf"
ls -l "$OUT/$TARGET.elf"

echo "==> linking the game data the engine reads through host:"
# PCSX2 maps host: to the ELF's own directory, so the assets the engine opens
# as "pc/..." have to be reachable from there. The HD assets live in res/pc;
# the original PSX data (SOUND/, XC/, ...) is not distributed with the engine
# and is picked up from the same place when the user extracted it there.
ln -sfn "$ROOT/res/pc" "$OUT/pc"
