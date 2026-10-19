// ps2gs.cpp — everything in the PS2 backend that touches gsKit/ps2sdk.
//
// Draws are submitted to gsKit's queue and flushed once per frame, which is
// what the GS wants: TEX0/ALPHA/TEST/SCISSOR travel with the primitives that
// need them. Textures are handed to gsKit's texture manager, which owns VRAM
// placement and the upload.
//
// See ps2gs.h for why this file is separate from ps2render.cpp.
#include "ps2gs.h"

#include <gsKit.h>
#include <libpad.h>
#include <malloc.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {

// One console, one display: the texture manager and the pad port are shared by
// the textures and the display object.
GSGLOBAL* g_global = nullptr;
unsigned char* g_padArea = nullptr;

// The EE console is not visible in PCSX2's log file, so init writes breadcrumbs
// next to the application log. A bring-up aid; harmless once the port works.
FILE* g_trace = nullptr;

void Trace(const char* fmt, ...) {
    if (g_trace == nullptr) {
        g_trace = fopen("host:/rechan-ps2-gs.log", "w");
        if (g_trace == nullptr) {
            return;
        }
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_trace, fmt, ap);
    va_end(ap);
    fflush(g_trace);
}

// GS alpha runs 0..0x80 (0x80 = 1.0); the bridge carries 0..255.
inline unsigned char GsAlpha(unsigned char a) { return (unsigned char)(a >> 1); }

inline u64 GsRgbaq(unsigned int rgba) {
    return GS_SETREG_RGBAQ(rgba & 0xffu, (rgba >> 8) & 0xffu, (rgba >> 16) & 0xffu,
                           GsAlpha((unsigned char)((rgba >> 24) & 0xffu)), 0x00);
}

// In GS texture modulation (TFX=0 / MODULATE), output is (F_in * C_tex) >> 7,
// where 0x80 is 1.0 (identity). Direct 0..255 vertex colours double brightness
// and saturate/blow out channels (e.g. orange becomes yellow, purple becomes
// magenta). Scale 0..255 -> 0..0x80 for textured draws.
inline u64 GsTexturedRgbaq(unsigned int rgba) {
    const unsigned int r = ((rgba & 0xffu) + 1) >> 1;
    const unsigned int g = (((rgba >> 8) & 0xffu) + 1) >> 1;
    const unsigned int b = (((rgba >> 16) & 0xffu) + 1) >> 1;
    return GS_SETREG_RGBAQ(r > 0x80 ? 0x80 : r,
                           g > 0x80 ? 0x80 : g,
                           b > 0x80 ? 0x80 : b,
                           GsAlpha((unsigned char)((rgba >> 24) & 0xffu)), 0x00);
}

inline unsigned int PackRgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return (unsigned int)r | ((unsigned int)g << 8) | ((unsigned int)b << 16) | ((unsigned int)a << 24);
}

inline unsigned short PackRgba5551(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return (unsigned short)(((unsigned short)(r >> 3) << 0) | ((unsigned short)(g >> 3) << 5)
                            | ((unsigned short)(b >> 3) << 10) | ((a >= 128 ? 1u : 0u) << 15));
}

} // namespace

// A texture keeps both the gsKit object (the texture manager stores this exact
// pointer in its VRAM block, so it must be stable) and the EE-side storage that
// gsKit uploads from.
struct Ps2GsTexture {
    GSTEXTURE tex;
    int width = 0;
    int height = 0;
    int format = PS2GS_FORMAT_RGBA32;
    void* mem = nullptr;
    void* clut = nullptr;
    int valid = 0;

    Ps2GsTexture() { memset(&tex, 0, sizeof(tex)); }
};

struct Ps2Gs {
    GSGLOBAL* global = nullptr;
};

namespace {

void ReleaseTextureStorage(Ps2GsTexture* texture) {
    if (texture == nullptr) {
        return;
    }
    if (texture->valid && g_global != nullptr) {
        gsKit_TexManager_free(g_global, &texture->tex);
    }
    texture->valid = 0;
    free(texture->mem);
    free(texture->clut);
    texture->mem = nullptr;
    texture->clut = nullptr;
    texture->width = 0;
    texture->height = 0;
    memset(&texture->tex, 0, sizeof(texture->tex));
}

// Palettised storage: the image becomes one byte (or half a byte) per texel
// plus a CLUT. Colours are ordered by usage so that a truncation keeps the ones
// that dominate the picture.
int BuildPalette(const unsigned char* src, int texels, int maxEntries,
                 unsigned int* palette, unsigned char* indices) {
    struct Entry {
        unsigned int key;
        int count;
    };

    Entry entries[256];
    int entryCount = 0;

    for (int i = 0; i < texels; ++i) {
        const unsigned int key = PackRgba(src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2],
                                          src[i * 4 + 3]);
        int found = -1;
        for (int e = 0; e < entryCount; ++e) {
            if (entries[e].key == key) {
                found = e;
                break;
            }
        }
        if (found >= 0) {
            entries[found].count++;
        }
        else if (entryCount < 256) {
            entries[entryCount].key = key;
            entries[entryCount].count = 1;
            entryCount++;
        }
    }

    // Insertion sort by descending usage: the entry count is small.
    for (int a = 1; a < entryCount; ++a) {
        Entry cur = entries[a];
        int b = a;
        while (b > 0 && entries[b - 1].count < cur.count) {
            entries[b] = entries[b - 1];
            --b;
        }
        entries[b] = cur;
    }
    if (entryCount > maxEntries) {
        entryCount = maxEntries;
    }

    for (int i = 0; i < maxEntries; ++i) {
        palette[i] = (i < entryCount) ? entries[i].key : 0u;
    }

    for (int i = 0; i < texels; ++i) {
        const unsigned int key = PackRgba(src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2],
                                          src[i * 4 + 3]);
        int best = 0;
        int bestDist = 0x7fffffff;
        for (int e = 0; e < entryCount; ++e) {
            const unsigned int p = entries[e].key;
            const int dr = (int)((p >> 0) & 0xffu) - (int)((key >> 0) & 0xffu);
            const int dg = (int)((p >> 8) & 0xffu) - (int)((key >> 8) & 0xffu);
            const int db = (int)((p >> 16) & 0xffu) - (int)((key >> 16) & 0xffu);
            const int da = (int)((p >> 24) & 0xffu) - (int)((key >> 24) & 0xffu);
            const int dist = dr * dr + dg * dg + db * db + da * da;
            if (dist < bestDist) {
                bestDist = dist;
                best = e;
            }
        }
        indices[i] = (unsigned char)best;
    }

    return entryCount;
}

} // namespace

Ps2Gs* ps2gs_init(int width, int height) {
    if (g_global != nullptr) {
        return nullptr;
    }

    Trace("gs: init %dx%d\n", width, height);

    GSGLOBAL* global = gsKit_init_global();
    if (global == nullptr) {
        printf("[RECHAN-PS2] gsKit_init_global failed\n");
        return nullptr;
    }
    Trace("gs: init_global ok\n");

    // Native mode: the console's own 640x448 NTSC interlaced signal, the same
    // format as the original disc. GS_FIELD (not GS_FRAME) keeps gsKit from
    // applying a vertical magnification that collapses the display.
    global->Mode = GS_MODE_NTSC;
    global->Interlace = GS_INTERLACED;
    global->Field = GS_FIELD;
    global->Width = width;
    global->Height = height;
    global->Aspect = GS_ASPECT_4_3;
    global->PrimAlphaEnable = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    Trace("gs: dmaKit ok\n");

    gsKit_init_screen(global);
    Trace("gs: init_screen ok\n");
    gsKit_TexManager_init(global);
    Trace("gs: texmanager ok\n");
    gsKit_mode_switch(global, GS_ONESHOT);
    gsKit_set_test(global, GS_ZTEST_OFF);
    gsKit_set_clamp(global, GS_CMODE_CLAMP);
    Trace("gs: mode/state ok\n");

    g_global = global;

    Ps2Gs* gs = new Ps2Gs();
    gs->global = global;
    Trace("gs: init done\n");
    return gs;
}

void ps2gs_shutdown(Ps2Gs* gs) {
    if (gs == nullptr) {
        return;
    }
    free(g_padArea);
    g_padArea = nullptr;
    if (gs->global != nullptr) {
        g_global = nullptr;
        gsKit_deinit_global(gs->global);
        gs->global = nullptr;
    }
    delete gs;
}

Ps2GsModeInfo ps2gs_mode_info(const Ps2Gs* gs) {
    Ps2GsModeInfo info = {0, 0, 0, 0, 0, 0, 0, 0};
    if (gs == nullptr || gs->global == nullptr) {
        return info;
    }
    info.width = gs->global->Width;
    info.height = gs->global->Height;
    info.interlace = gs->global->Interlace;
    info.field = gs->global->Field;
    info.dw = gs->global->DW;
    info.dh = gs->global->DH;
    info.magH = gs->global->MagH;
    info.magV = gs->global->MagV;
    return info;
}

void ps2gs_frame_end(Ps2Gs* gs) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }
    static int tracedFrames = 0;
    if (tracedFrames < 2) {
        Trace("gs: frame_end #%d\n", tracedFrames + 1);
        ++tracedFrames;
    }
    gsKit_queue_exec(gs->global);
    // The GS flips in step with the display, so vsync is inherent here.
    gsKit_sync_flip(gs->global);
    gsKit_TexManager_nextFrame(gs->global);
}

void ps2gs_clear(Ps2Gs* gs, unsigned char r, unsigned char g, unsigned char b) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }
    gsKit_clear(gs->global, GS_SETREG_RGBAQ(r, g, b, 0x00, 0x00));

    // gsKit_clear submits a full-screen sprite at Z=0. Without a depth buffer
    // that Z would reject every primitive drawn at Z>0, so the test is left off
    // until the 3D milestone brings a real Z buffer.
    gsKit_set_test(gs->global, GS_ZTEST_OFF);
}

void ps2gs_set_ztest(Ps2Gs* gs, int enable) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }
    gsKit_set_test(gs->global, enable ? GS_ZTEST_ON : GS_ZTEST_OFF);
}

void ps2gs_set_blend(Ps2Gs* gs, int blend) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }

    // GS alpha register: (A - B) * FIX + D, plus a per-pixel alpha switch.
    //   0 = source colour, 1 = destination colour, 2 = zero.
    switch (blend) {
        case PS2GS_BLEND_ALPHA:
            gsKit_set_primalpha(gs->global, GS_SETREG_ALPHA(0, 1, 0, 1, 0x80), GS_SETTING_ON);
            break;
        case PS2GS_BLEND_ADD:
            gsKit_set_primalpha(gs->global, GS_SETREG_ALPHA(0, 2, 0, 1, 0x80), GS_SETTING_ON);
            break;
        case PS2GS_BLEND_SUBTRACT:
            gsKit_set_primalpha(gs->global, GS_SETREG_ALPHA(1, 0, 0, 1, 0x80), GS_SETTING_ON);
            break;
        case PS2GS_BLEND_HALF:
            gsKit_set_primalpha(gs->global, GS_SETREG_ALPHA(0, 1, 0, 1, 0x40), GS_SETTING_OFF);
            break;
        case PS2GS_BLEND_NONE:
        default:
            // FIX at 1.0 with no per-pixel alpha: the primitive is opaque and
            // the vertex alpha in RGBAQ is ignored.
            gsKit_set_primalpha(gs->global, GS_SETREG_ALPHA(0, 1, 0, 1, 0x80), GS_SETTING_OFF);
            break;
    }
}

void ps2gs_set_scissor(Ps2Gs* gs, int x, int y, int w, int h) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }
    gsKit_set_scissor(gs->global, GS_SETREG_SCISSOR(x, x + w - 1, y, y + h - 1));
}

Ps2GsTexture* ps2gs_texture_create(void) {
    return new Ps2GsTexture();
}

void ps2gs_texture_destroy(Ps2GsTexture* texture) {
    if (texture == nullptr) {
        return;
    }
    ReleaseTextureStorage(texture);
    delete texture;
}

int ps2gs_texture_upload(Ps2GsTexture* texture, int width, int height, int format,
                         const void* rgba8) {
    if (texture == nullptr) {
        return 0;
    }

    ReleaseTextureStorage(texture);

    if (width <= 0 || height <= 0 || rgba8 == nullptr) {
        return 0;
    }

    // The GS cannot address more than 1024 texels in either direction, and the
    // 4 MiB of VRAM cannot hold a full 1920x1080 RGBA sheet anyway. Reject the
    // texture here, with a reason in the trace, instead of handing gsKit
    // something it cannot upload.
    if (width > 1024 || height > 1024) {
        Trace("tex: rejected %dx%d (GS limit is 1024x1024)\n", width, height);
        return 0;
    }

    const unsigned char* src = (const unsigned char*)rgba8;
    const int texels = width * height;

    int psm = GS_PSM_CT32;
    switch (format) {
        case PS2GS_FORMAT_RGBA16: psm = GS_PSM_CT16; break;
        case PS2GS_FORMAT_CLUT8: psm = GS_PSM_T8; break;
        case PS2GS_FORMAT_CLUT4: psm = GS_PSM_T4; break;
        case PS2GS_FORMAT_RGBA32:
        default: psm = GS_PSM_CT32; break;
    }

    void* mem = memalign(128, gsKit_texture_size_ee(width, height, psm));
    if (mem == nullptr) {
        Trace("tex: alloc failed %dx%d psm=%d\n", width, height, psm);
        printf("[RECHAN-PS2] texture alloc failed (%dx%d)\n", width, height);
        return 0;
    }

    void* clut = nullptr;

    if (psm == GS_PSM_CT32) {
        // The bridge already carries GS byte order, so this is a straight copy.
        memcpy(mem, src, (size_t)texels * 4);
    }
    else if (psm == GS_PSM_CT16) {
        unsigned short* dst = (unsigned short*)mem;
        for (int i = 0; i < texels; ++i) {
            dst[i] = PackRgba5551(src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2], src[i * 4 + 3]);
        }
    }
    else {
        const int maxEntries = (psm == GS_PSM_T4) ? 16 : 256;
        const int clutWidth = (psm == GS_PSM_T4) ? 8 : 16;
        const int clutHeight = (psm == GS_PSM_T4) ? 2 : 16;

        clut = memalign(128, gsKit_texture_size_ee(clutWidth, clutHeight, GS_PSM_CT32));
        unsigned char* indices = (unsigned char*)malloc((size_t)texels);
        if (clut == nullptr || indices == nullptr) {
            free(mem);
            free(clut);
            free(indices);
            printf("[RECHAN-PS2] clut alloc failed\n");
            return 0;
        }

        BuildPalette(src, texels, maxEntries, (unsigned int*)clut, indices);

        const unsigned int* pal = (const unsigned int*)clut;
        Trace("tex: built %dx%d psm=%d entries=%d mem=%p clut=%p\n", width, height, psm,
              maxEntries, mem, clut);
        Trace("tex: pal %08x %08x %08x %08x %08x %08x %08x %08x\n",
              pal[0], pal[1], pal[2], pal[3], pal[4], pal[5], pal[6], pal[7]);
        Trace("tex: idx %02x %02x %02x %02x\n", indices[0], indices[1], indices[2], indices[3]);

        if (psm == GS_PSM_T8) {
            memcpy(mem, indices, (size_t)texels);
        }
        else {
            // Two 4-bit indices per byte, low nibble first.
            unsigned char* dst = (unsigned char*)mem;
            for (int i = 0; i < texels; i += 2) {
                const unsigned char lo = indices[i] & 0x0f;
                const unsigned char hi = (i + 1 < texels) ? (unsigned char)(indices[i + 1] & 0x0f) : 0;
                dst[i >> 1] = (unsigned char)(lo | (hi << 4));
            }
        }
        free(indices);
    }

    texture->width = width;
    texture->height = height;
    texture->format = format;
    texture->mem = mem;
    texture->clut = clut;

    texture->tex.Width = (u32)width;
    texture->tex.Height = (u32)height;
    texture->tex.PSM = (u8)psm;
    texture->tex.Filter = GS_FILTER_NEAREST;
    texture->tex.Mem = (u32*)mem;
    texture->tex.Delayed = 1;
    if (clut != nullptr) {
        texture->tex.Clut = (u32*)clut;
        texture->tex.ClutPSM = GS_PSM_CT32;
        texture->tex.ClutStorageMode = GS_CLUT_STOREMODE_NOLOAD;
    }

    if (g_global != nullptr) {
        gsKit_TexManager_invalidate(g_global, &texture->tex);
    }
    texture->valid = 1;
    return 1;
}

void ps2gs_texture_set_filter(Ps2GsTexture* texture, int linear) {
    if (texture == nullptr) {
        return;
    }
    texture->tex.Filter = linear ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
}

int ps2gs_texture_width(const Ps2GsTexture* texture) {
    return texture != nullptr ? texture->width : 0;
}

int ps2gs_texture_height(const Ps2GsTexture* texture) {
    return texture != nullptr ? texture->height : 0;
}

void ps2gs_bind_texture(Ps2Gs* gs, Ps2GsTexture* texture) {
    if (gs == nullptr || gs->global == nullptr || texture == nullptr || !texture->valid) {
        return;
    }
    // The manager uploads the texture the first time it is bound (and again
    // after invalidate), allocating its VRAM block on the way.
    gsKit_TexManager_bind(gs->global, &texture->tex);
    gsKit_set_texfilter(gs->global, (u8)texture->tex.Filter);

    {
        const unsigned int* pal = (const unsigned int*)texture->clut;
        Trace("bind: %dx%d psm=%u tbw=%d vram=%u vramclut=%u cpsm=%u csm=%u\n",
              texture->width, texture->height, texture->tex.PSM, texture->tex.TBW,
              texture->tex.Vram, texture->tex.VramClut, texture->tex.ClutPSM,
              texture->tex.ClutStorageMode);
        if (pal != nullptr) {
            Trace("bind: pal %08x %08x %08x %08x\n", pal[0], pal[1], pal[2], pal[3]);
        }
    }
}

void ps2gs_set_clamp(Ps2Gs* gs, int clamp) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }
    gsKit_set_clamp(gs->global, clamp ? GS_CMODE_CLAMP : GS_CMODE_REPEAT);
}

void ps2gs_draw_sprite_flat(Ps2Gs* gs, float x1, float y1, float x2, float y2,
                            unsigned int rgba) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }
    gsKit_prim_sprite(gs->global, x1, y1, x2, y2, 1, GsRgbaq(rgba));
}

void ps2gs_draw_sprite_textured(Ps2Gs* gs, Ps2GsTexture* texture,
                                float x1, float y1, float u1, float v1,
                                float x2, float y2, float u2, float v2,
                                unsigned int rgba) {
    if (gs == nullptr || gs->global == nullptr || texture == nullptr || !texture->valid) {
        return;
    }
    const float tw = (float)texture->width;
    const float th = (float)texture->height;
    gsKit_prim_sprite_texture_3d(gs->global, &texture->tex,
                                 x1, y1, 1, u1 * tw, v1 * th,
                                 x2, y2, 1, u2 * tw, v2 * th,
                                 GsTexturedRgbaq(rgba));
}

void ps2gs_draw_triangle_gouraud(Ps2Gs* gs, const float* xy1, unsigned int c1,
                                 const float* xy2, unsigned int c2,
                                 const float* xy3, unsigned int c3) {
    if (gs == nullptr || gs->global == nullptr) {
        return;
    }
    gsKit_prim_triangle_gouraud_3d(gs->global,
                                   xy1[0], xy1[1], 1, xy2[0], xy2[1], 1, xy3[0], xy3[1], 1,
                                   GsRgbaq(c1), GsRgbaq(c2), GsRgbaq(c3));
}

void ps2gs_draw_triangle_textured(Ps2Gs* gs, Ps2GsTexture* texture,
                                  const float* xy1, const float* uv1, unsigned int c1,
                                  const float* xy2, const float* uv2, unsigned int c2,
                                  const float* xy3, const float* uv3, unsigned int c3) {
    if (gs == nullptr || gs->global == nullptr || texture == nullptr || !texture->valid) {
        return;
    }
    const float tw = (float)texture->width;
    const float th = (float)texture->height;
    gsKit_prim_triangle_goraud_texture_3d(gs->global, &texture->tex,
                                          xy1[0], xy1[1], 1, uv1[0] * tw, uv1[1] * th,
                                          xy2[0], xy2[1], 1, uv2[0] * tw, uv2[1] * th,
                                          xy3[0], xy3[1], 1, uv3[0] * tw, uv3[1] * th,
                                          GsTexturedRgbaq(c1), GsTexturedRgbaq(c2), GsTexturedRgbaq(c3));
}

namespace {

// libpad talks to the IOP's sio2man/padman drivers; the platform layer loads
// those once the file system is up and then calls ps2gs_pad_open(), so the pad
// is only brought up when the drivers can actually answer. A failed attempt is
// not retried.
int g_padEnabled = 0;

void EnsurePad() {
    static int attempted = 0;
    if (attempted || !g_padEnabled) {
        return;
    }
    attempted = 1;

    g_padArea = (unsigned char*)memalign(64, 256);
    if (g_padArea == nullptr) {
        return;
    }
    padInit(0);
    Trace("gs: padInit ok\n");
    padPortOpen(0, 0, g_padArea);
    Trace("gs: padPortOpen ok\n");
}

} // namespace

void ps2gs_pad_open(void) { g_padEnabled = 1; }

void ps2gs_pad_poll(Ps2GsPadState* state) {
    if (state == nullptr) {
        return;
    }

    state->connected = 0;
    state->buttons = 0xffff;
    state->leftX = 128;
    state->leftY = 128;
    state->rightX = 128;
    state->rightY = 128;

    EnsurePad();

    if (g_padArea == nullptr) {
        return;
    }

    const int padState = padGetState(0, 0);
    if (padState != PAD_STATE_STABLE && padState != PAD_STATE_FINDCTP1) {
        return;
    }

    struct padButtonStatus status;
    if (padRead(0, 0, &status) == 0) {
        return;
    }

    state->connected = 1;
    state->buttons = status.btns;
    state->leftX = status.ljoy_h;
    state->leftY = status.ljoy_v;
    state->rightX = status.rjoy_h;
    state->rightY = status.rjoy_v;
}
