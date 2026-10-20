#!/usr/bin/env bash
# Builds the PSP target inside the official pspdev container on the remote Docker host
# and brings EBOOT.PBP and rechan_strip.elf back to psp/.
# If the PSP Memory Stick is mounted at /Volumes/NO NAME/, it automatically syncs to it.

set -euo pipefail

HOST="${RECHAN_PSP_HOST:-dev@192.168.18.24}"
IMAGE="${RECHAN_PSP_IMAGE:-ghcr.io/pspdev/pspdev:latest}"
REMOTE_DIR="${RECHAN_PSP_DIR:-/tmp/rechan-psp}"
BUILD_MODE="${1:-release}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARBALL="/tmp/rechan-psp-src.$$.tgz"

echo "==> packaging PSP sources"
tar --exclude='*.o' --exclude='*.d' --exclude='*.elf' --exclude='*.prx' \
    --exclude='*.PBP' --exclude='*.SFO' --exclude='.git' \
    -C "$ROOT" -czf "$TARBALL" \
    psp \
    src \
    vendor/libp3d \
    vendor/cgltf \
    vendor/stb
trap 'rm -f "$TARBALL"' EXIT

echo "==> sending sources to $HOST:$REMOTE_DIR"
ssh "$HOST" "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR"
scp -q "$TARBALL" "$HOST:$REMOTE_DIR/src.tgz"

echo "==> building in container $IMAGE (BUILD_MODE=$BUILD_MODE)"
ssh "$HOST" "cd $REMOTE_DIR && tar xzf src.tgz && rm -f src.tgz && docker run --rm -v $REMOTE_DIR:/work -w /work/psp $IMAGE sh -c '
    export PSPDEV=/usr/local/pspdev PSPSDK=/usr/local/pspdev/psp/sdk
    export PATH=/usr/local/pspdev/bin:\$PATH
    make -B -j\$(nproc 2>/dev/null || echo 4) BUILD_MODE=$BUILD_MODE
' 2>&1"

echo "==> fetching built EBOOT.PBP and rechan.elf"
scp -q "$HOST:$REMOTE_DIR/psp/EBOOT.PBP" "$ROOT/psp/EBOOT.PBP"
if ssh "$HOST" "test -f $REMOTE_DIR/psp/rechan.elf"; then
    scp -q "$HOST:$REMOTE_DIR/psp/rechan.elf" "$ROOT/psp/rechan.elf" || true
fi

ls -lh "$ROOT/psp/EBOOT.PBP"

MEMSTICK_DIR="/Volumes/NO NAME/PSP/GAME/RECHAN"
if [ -d "$MEMSTICK_DIR" ]; then
    echo "==> copying to PSP Memory Stick at $MEMSTICK_DIR"
    cp -v "$ROOT/psp/EBOOT.PBP" "$MEMSTICK_DIR/EBOOT.PBP"
    sync
    echo "==> Memory Stick updated successfully!"
fi
