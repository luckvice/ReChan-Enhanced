#!/usr/bin/env bash
# Prepares the host: tree PCSX2 exposes to the ELF so the engine can open its
# assets as pc/... .
#
# The desktop art is authored at 1920x1080 and stb_image needs a full RGBA
# buffer (~8 MiB per sheet) before it can be downscaled. The EE only has 32 MiB
# and the engine has already claimed most of it by the time the frontend loads,
# so the copy that reaches the console is downscaled once, here:
#
#   * fullscreen 16:9 art (>= 960x540)  -> fits 512x288  (16:9, 576 KiB of VRAM)
#   * everything else                   -> fits 512x512  (CT32, 1 MiB of VRAM)
#
# The 512x288 ceiling comes from gsKit: its two framebuffers and the z-buffer
# already claim 3.28 MiB of the 4 MiB of VRAM, and a 640x360 sheet (900 KiB) does
# not fit in what is left — gsKit hangs trying to allocate it. The 512x512 cap
# only covers art that fits the GS addressable limit; those sheets are still too
# large to be uploaded next to the framebuffers today (see the README).
#
# The runtime keeps the same caps in tTexture::LoadFromImagePath as a safety
# net, so pointing host: at the unscaled res/pc still works on the desktop.
#
# Usage: bash ps2/tools/prepare-host-data.sh [dest]   (default: dist/ps2)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$ROOT/res/pc"
DEST="${1:-$ROOT/dist/ps2}"

if [ ! -d "$SRC" ]; then
    echo "ERROR: $SRC not found" >&2
    exit 1
fi

# A previous run may have left dist/ps2/pc as a symlink to res/pc; writing
# through it would resize the repository assets in place.
if [ -L "$DEST/pc" ]; then
    rm -f "$DEST/pc"
fi
mkdir -p "$DEST/pc"

if ! command -v sips >/dev/null 2>&1; then
    echo "WARNING: sips not found; copying the art unscaled (the EE may run out" >&2
    echo "         of memory decoding 1920x1080 sheets)" >&2
fi

copy_plain() {
    local source="$1" target="$2"
    mkdir -p "$(dirname "$target")"
    if [ -f "$target" ] && [ "$target" -nt "$source" ]; then
        return 0
    fi
    cp -p "$source" "$target"
}

resize_png() {
    local source="$1" target="$2"
    local width height box_w box_h target_w target_h
    read -r width height <<< "$(sips -g pixelWidth -g pixelHeight "$source" 2>/dev/null \
        | awk '/pixelWidth/ { w = $2 } /pixelHeight/ { h = $2 } END { print w, h }')"

    if [ -z "${width:-}" ] || [ -z "${height:-}" ]; then
        copy_plain "$source" "$target"
        return 0
    fi

    if [ "$width" -le 512 ] && [ "$height" -le 512 ]; then
        copy_plain "$source" "$target"
        return 0
    fi

    if [ "$width" -ge 960 ] && [ "$height" -ge 540 ] && [ $((width * 2)) -ge $((height * 3)) ]; then
        box_w=512; box_h=288
    else
        box_w=512; box_h=512
    fi

    # sips -Z takes a single (square) box, so the aspect-preserving target is
    # computed here and applied with the exact -z.
    read -r target_w target_h <<< "$(awk -v w="$width" -v h="$height" -v bw="$box_w" -v bh="$box_h" 'BEGIN {
        scale = bw / w; if (bh / h < scale) scale = bh / h;
        tw = int(w * scale + 0.5); th = int(h * scale + 0.5);
        if (tw < 1) tw = 1; if (th < 1) th = 1;
        print tw, th;
    }')"

    mkdir -p "$(dirname "$target")"
    if [ -f "$target" ] && [ "$target" -nt "$source" ]; then
        return 0
    fi
    sips -z "$target_h" "$target_w" "$source" --out "$target" >/dev/null
}

while IFS= read -r -d '' source; do
    rel="${source#"$SRC"/}"
    target="$DEST/pc/$rel"
    case "$rel" in
        *.png|*.PNG)
            if command -v sips >/dev/null 2>&1; then
                resize_png "$source" "$target"
            else
                copy_plain "$source" "$target"
            fi
            ;;
        *)
            copy_plain "$source" "$target"
            ;;
    esac
done < <(find "$SRC" -type f -print0)

echo "host data ready in $DEST/pc"
