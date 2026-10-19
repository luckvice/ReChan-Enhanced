// ps2render.cpp — the pddi side of the PS2 backend.
//
// This file implements the pddi contracts and knows nothing about ps2sdk or
// gsKit: every GS operation goes through ps2gs.h. That keeps the platform
// details in one place and lets this translation unit include libp3d's headers
// without colliding with ps2sdk's types (see ps2gs.h).
#include "ps2render.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// Colours cross into ps2gs packed as r | g<<8 | b<<16 | a<<24.
inline unsigned int Pack(u8 r, u8 g, u8 b, u8 a) {
    return (unsigned int)r | ((unsigned int)g << 8) | ((unsigned int)b << 16)
         | ((unsigned int)a << 24);
}

inline unsigned int Pack(pddiColour c) { return Pack(c.r, c.g, c.b, c.a); }

inline u8 FloatToByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return (u8)(v * 255.0f + 0.5f);
}

// Log goes to the EE TTY (PCSX2 shows it with its console) and, when the app
// opened it, the host: device.
void Ps2Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

} // namespace

// ps2Texture

ps2Texture::~ps2Texture() {
    ReleaseStorage();
}

void ps2Texture::ReleaseStorage() {
    if (native != nullptr) {
        ps2gs_texture_destroy(native);
        native = nullptr;
    }
    valid = false;
}

void ps2Texture::SetFilterMode(pddiFilterMode mode) {
    filterLinear = (mode != PDDI_FILTER_NONE);
    if (native != nullptr) {
        ps2gs_texture_set_filter(native, filterLinear ? 1 : 0);
    }
}

void ps2Texture::SetData(int w, int h, int bppIn, int alphaDepthIn, const void* rgba) {
    ReleaseStorage();

    if (w <= 0 || h <= 0 || rgba == nullptr) {
        return;
    }

    width = w;
    height = h;
    bpp = bppIn > 0 ? bppIn : 32;
    alphaDepth = alphaDepthIn;

    // Storage format from the pddi bpp. The palettised formats are what the
    // PSX-era art of the game arrives as.
    int format = PS2GS_FORMAT_RGBA32;
    if (bpp <= 4) {
        format = PS2GS_FORMAT_CLUT4;
    }
    else if (bpp <= 8) {
        format = PS2GS_FORMAT_CLUT8;
    }
    else if (bpp <= 16 || w >= 512 || h >= 360) {
        format = PS2GS_FORMAT_RGBA16;
    }

    if (native == nullptr) {
        native = ps2gs_texture_create();
    }
    if (native == nullptr || !ps2gs_texture_upload(native, w, h, format, rgba)) {
        ReleaseStorage();
        return;
    }

    ps2gs_texture_set_filter(native, filterLinear ? 1 : 0);
    valid = true;
}

// ps2Shader

ps2Shader::ps2Shader(const char* type_) : type(type_ != nullptr ? type_ : "simple") {}

void ps2Shader::SetInt(u32 param, int value) {
    if (param == PDDI_SP::ALPHATEST) {
        alphaTest = value != 0;
    }
}

void ps2Shader::SetColour(u32 param, pddiColour c) {
    if (param == PDDI_SP::DIFFUSE || param == PDDI_SP::AMBIENT || param == PDDI_SP::EMISSIVE) {
        diffuse = c;
    }
}

// ps2PrimBuffer

ps2PrimBuffer::ps2PrimBuffer(const pddiPrimBufferDesc& desc)
    : primType(desc.primType), vertexFormat(desc.vertexFormat) {
    vertices.reserve(desc.vertexCount);
    indices.reserve(desc.indexCount);
}

void ps2PrimBuffer::SetVertexData(const void* data, u32 count) {
    vertices.clear();
    if (data == nullptr || count == 0) {
        return;
    }

    const float* src = (const float*)data;
    const int stride = ((vertexFormat & PDDI_V_POSITION) ? 3 : 0)
                     + ((vertexFormat & PDDI_V_COLOUR) ? 3 : 0)
                     + ((vertexFormat & PDDI_V_UV) ? 2 : 0)
                     + ((vertexFormat & PDDI_V_TEXINFO) ? 2 : 0);
    if (stride == 0) {
        return;
    }

    for (u32 i = 0; i < count; ++i) {
        const float* v = src + (size_t)i * stride;
        Ps2Vertex out;
        int lane = 0;
        if (vertexFormat & PDDI_V_POSITION) {
            out.x = v[lane + 0];
            out.y = v[lane + 1];
            out.z = v[lane + 2];
            lane += 3;
        }
        if (vertexFormat & PDDI_V_COLOUR) {
            out.r = v[lane + 0];
            out.g = v[lane + 1];
            out.b = v[lane + 2];
            lane += 3;
        }
        if (vertexFormat & PDDI_V_UV) {
            out.u = v[lane + 0];
            out.v = v[lane + 1];
            lane += 2;
        }
        if (vertexFormat & PDDI_V_TEXINFO) {
            out.texInfo[0] = v[lane + 0];
            out.texInfo[1] = v[lane + 1];
        }
        vertices.push_back(out);
    }
}

void ps2PrimBuffer::SetIndices(const u16* data, u32 count) {
    indices.clear();
    if (data == nullptr || count == 0) {
        return;
    }
    indices.assign(data, data + count);
}

// ps2Gamepad

void ps2Gamepad::Poll() {
    ps2gs_pad_poll(&pad);
}

bool ps2Gamepad::IsButtonDown(int button) const {
    // libpad reports buttons active low.
    unsigned short mask = 0;
    switch (button) {
        case GamepadButton::A: mask = PS2GS_PAD_CROSS; break;
        case GamepadButton::B: mask = PS2GS_PAD_CIRCLE; break;
        case GamepadButton::X: mask = PS2GS_PAD_SQUARE; break;
        case GamepadButton::Y: mask = PS2GS_PAD_TRIANGLE; break;
        case GamepadButton::LeftBumper: mask = PS2GS_PAD_L1; break;
        case GamepadButton::RightBumper: mask = PS2GS_PAD_R1; break;
        case GamepadButton::Back: mask = PS2GS_PAD_SELECT; break;
        case GamepadButton::Start: mask = PS2GS_PAD_START; break;
        case GamepadButton::LeftThumb: mask = PS2GS_PAD_L3; break;
        case GamepadButton::RightThumb: mask = PS2GS_PAD_R3; break;
        case GamepadButton::DpadUp: mask = PS2GS_PAD_UP; break;
        case GamepadButton::DpadRight: mask = PS2GS_PAD_RIGHT; break;
        case GamepadButton::DpadDown: mask = PS2GS_PAD_DOWN; break;
        case GamepadButton::DpadLeft: mask = PS2GS_PAD_LEFT; break;
        default: return false;
    }
    return (pad.buttons & mask) == 0;
}

float ps2Gamepad::GetAxis(int axis) const {
    // libpad centres the sticks at 128 and reports them with high values up.
    switch (axis) {
        case GamepadAxis::LeftX: return ((float)pad.leftX - 128.0f) / 127.0f;
        case GamepadAxis::LeftY: return ((float)pad.leftY - 128.0f) / 127.0f;
        case GamepadAxis::RightX: return ((float)pad.rightX - 128.0f) / 127.0f;
        case GamepadAxis::RightY: return ((float)pad.rightY - 128.0f) / 127.0f;
        case GamepadAxis::LeftTrigger: return (pad.buttons & PS2GS_PAD_L2) == 0 ? 1.0f : 0.0f;
        case GamepadAxis::RightTrigger: return (pad.buttons & PS2GS_PAD_R2) == 0 ? 1.0f : 0.0f;
        default: return 0.0f;
    }
}

// ps2RenderTarget

ps2RenderTarget::ps2RenderTarget(int w, int h) : width(w), height(h), texture(new ps2Texture()) {}

ps2RenderTarget::~ps2RenderTarget() {
    delete texture;
}

// ps2Display

ps2Display::ps2Display() {
    if (const char* frames = getenv("RECHAN_PS2_MAX_FRAMES")) {
        const long value = strtol(frames, nullptr, 10);
        if (value > 0) {
            maxFrames = (u64)value;
        }
    }
}

ps2Display::~ps2Display() {
    if (gs != nullptr) {
        ps2gs_shutdown(gs);
        gs = nullptr;
    }
}

bool ps2Display::InitDisplay(const pddiDisplayInit& init) {
    if (gs != nullptr) {
        return true;
    }

    if (init.xSize != PS2_NATIVE_WIDTH || init.ySize != PS2_NATIVE_HEIGHT) {
        Ps2Log("[RECHAN-PS2] requested %dx%d; the PS2 target renders at the native %dx%d\n",
               init.xSize, init.ySize, PS2_NATIVE_WIDTH, PS2_NATIVE_HEIGHT);
    }

    gs = ps2gs_init(PS2_NATIVE_WIDTH, PS2_NATIVE_HEIGHT);
    if (gs == nullptr) {
        return false;
    }

    const Ps2GsModeInfo mode = ps2gs_mode_info(gs);
    width = mode.width;
    height = mode.height;

    Ps2Log("[RECHAN-PS2] display: %dx%d interlace=%d field=%d DW=%d DH=%d mag=(%d,%d)\n",
           mode.width, mode.height, mode.interlace, mode.field, mode.dw, mode.dh,
           mode.magH, mode.magV);
    return true;
}

void ps2Display::SwapBuffers() {
    if (gs == nullptr) {
        return;
    }
    ps2gs_frame_end(gs);
    ++frameCount;
}

bool ps2Display::ShouldClose() {
    if (closeRequested) {
        return true;
    }
    if (maxFrames != 0 && frameCount >= maxFrames) {
        return true;
    }
    return false;
}

void ps2Display::PollEvents() {
    Ps2GsPadState pad;
    ps2gs_pad_poll(&pad);
    padButtons = pad.buttons;
}

bool ps2Display::IsKeyDown(int key) {
    // Start asks the app to close; the rest of the pad feeds the key state the
    // game queries.
    if ((padButtons & PS2GS_PAD_START) == 0) {
        closeRequested = true;
    }

    switch (key) {
        case 'W': return (padButtons & PS2GS_PAD_UP) == 0;
        case 'S': return (padButtons & PS2GS_PAD_DOWN) == 0;
        case 'A': return (padButtons & PS2GS_PAD_LEFT) == 0;
        case 'D': return (padButtons & PS2GS_PAD_RIGHT) == 0;
        case 32: return (padButtons & PS2GS_PAD_CROSS) == 0;   // space: confirm
        case 27: return (padButtons & PS2GS_PAD_CIRCLE) == 0;  // escape: cancel
        case 13: return (padButtons & PS2GS_PAD_SELECT) == 0;  // enter: menus
        default: return false;
    }
}

void ps2Display::GetVideoMode(int index, pddiVideoMode& mode) {
    (void)index;
    mode.width = PS2_NATIVE_WIDTH;
    mode.height = PS2_NATIVE_HEIGHT;
    mode.refreshRate = 60;
}

ps2Context::ps2Context(ps2Display* display) : gs(display != nullptr ? display->Gs() : nullptr) {
    decodeScratch.resize(256 * 256 * 4);
    pageCache.resize(kPageCacheCapacity);
    for (auto& entry : pageCache) {
        entry.texture = ps2gs_texture_create();
    }
}

ps2Context::~ps2Context() {
    for (auto& entry : pageCache) {
        if (entry.texture != nullptr) {
            ps2gs_texture_destroy(entry.texture);
            entry.texture = nullptr;
        }
    }
    pageCache.clear();
    for (auto& img : vramImages) {
        if (img.pixels != nullptr) {
            std::free(img.pixels);
            img.pixels = nullptr;
        }
    }
    vramImages.clear();
}

void ps2Context::Clear(int flags) {
    if (gs == nullptr) {
        return;
    }
    if (flags & PDDI_BUFFER_COLOUR) {
        ps2gs_clear(gs, clearColour.r, clearColour.g, clearColour.b);
    }
    else {
        // Even without a colour clear the Z test has to be left off (see
        // ps2gs_clear): there is no depth buffer to test against yet.
        ps2gs_set_ztest(gs, 0);
    }
}

void ps2Context::SetCullMode(pddiCullMode mode) {
    // The GS cull field is per-primitive; until the 3D milestone emits triangles
    // with a winding, the mode is only stored.
    cullMode = mode;
}

void ps2Context::EnableZBuffer(bool enable) {
    if (enable && !zBufferRequested) {
        Ps2Log("[RECHAN-PS2] depth test requested; the Z buffer arrives with the 3D milestone\n");
    }
    zBufferRequested = enable;
    if (gs != nullptr) {
        ps2gs_set_ztest(gs, 0);
    }
}

void ps2Context::SetBlendMode(pddiBlendMode mode) {
    blendMode = mode;
    if (gs == nullptr) {
        return;
    }

    int blend = PS2GS_BLEND_NONE;
    switch (mode) {
        case PDDI_BLEND_ALPHA: blend = PS2GS_BLEND_ALPHA; break;
        case PDDI_BLEND_ADD: blend = PS2GS_BLEND_ADD; break;
        case PDDI_BLEND_SUBTRACT: blend = PS2GS_BLEND_SUBTRACT; break;
        case PDDI_BLEND_PSX_QUARTER: blend = PS2GS_BLEND_HALF; break;
        default: blend = PS2GS_BLEND_NONE; break;
    }
    ps2gs_set_blend(gs, blend);
}

void ps2Context::SetPolygonOffset(bool enable, f32 factor, f32 units) {
    (void)factor;
    (void)units;
    polygonOffset = enable;
}

void ps2Context::SetScissor(int x, int y, int w, int h) {
    if (gs == nullptr) {
        return;
    }
    ps2gs_set_scissor(gs, x, y, w, h);
}

pddiRenderTarget* ps2Context::CreateRenderTarget(int width, int height,
                                                 pddiRenderTargetFormat format,
                                                 bool withInstanceId) {
    (void)format;
    (void)withInstanceId;
    return new ps2RenderTarget(width, height);
}

bool ps2Context::SetRenderTarget(pddiRenderTarget* target) {
    (void)target;
    return false;
}

Ps2GsTexture* ps2Context::ResolveTexture(const pddiBaseShader* shader, pddiColour* colour) {
    const ps2Shader* ps2shader = dynamic_cast<const ps2Shader*>(shader);

    if (colour != nullptr) {
        *colour = ps2shader != nullptr ? ps2shader->Diffuse() : pddiColour(255, 255, 255, 255);
    }

    pddiTexture* tex = (ps2shader != nullptr && ps2shader->Texture() != nullptr)
        ? ps2shader->Texture()
        : boundTexture;
    if (tex == nullptr) {
        return nullptr;
    }

    ps2Texture* ps2tex = dynamic_cast<ps2Texture*>(tex);
    return (ps2tex != nullptr && ps2tex->IsValid()) ? ps2tex->Native() : nullptr;
}

void ps2Context::DrawQuad(pddiBaseShader* shader, float x, float y, float w, float h,
                          float u0, float v0, float u1, float v1) {
    if (gs == nullptr) {
        return;
    }

    pddiColour colour;
    Ps2GsTexture* tex = ResolveTexture(shader, &colour);

    if (tex != nullptr) {
        ps2gs_set_clamp(gs, 1);
        ps2gs_bind_texture(gs, tex);
        ps2gs_draw_sprite_textured(gs, tex, x, y, u0, v0, x + w, y + h, u1, v1, Pack(colour));
        return;
    }

    // Untextured immediate quad: a flat sprite in the shader's diffuse colour.
    ps2gs_set_clamp(gs, 0);
    ps2gs_draw_sprite_flat(gs, x, y, x + w, y + h, Pack(colour));
}

void ps2Context::DrawQuadBatch(pddiTexture* tex, pddiBlendMode blend,
                               const pddiBatchVertex* verts, s32 vertCount) {
    if (gs == nullptr || verts == nullptr || vertCount <= 0) {
        return;
    }

    SetBlendMode(blend);

    ps2Texture* ps2tex = dynamic_cast<ps2Texture*>(tex);
    Ps2GsTexture* gstex = (ps2tex != nullptr && ps2tex->IsValid()) ? ps2tex->Native() : nullptr;

    if (gstex != nullptr) {
        ps2gs_set_clamp(gs, 1);
        ps2gs_bind_texture(gs, gstex);
    }
    else {
        ps2gs_set_clamp(gs, 0);
    }

    // The batch arrives as triangles with per-vertex colour and UV.
    for (s32 i = 0; i + 2 < vertCount; i += 3) {
        const pddiBatchVertex& a = verts[i + 0];
        const pddiBatchVertex& b = verts[i + 1];
        const pddiBatchVertex& c = verts[i + 2];

        const unsigned int ca = Pack(a.r, a.g, a.b, a.a);
        const unsigned int cb = Pack(b.r, b.g, b.b, b.a);
        const unsigned int cc = Pack(c.r, c.g, c.b, c.a);

        const float xy1[2] = {a.x, a.y};
        const float xy2[2] = {b.x, b.y};
        const float xy3[2] = {c.x, c.y};

        if (gstex != nullptr) {
            const float uv1[2] = {a.u, a.v};
            const float uv2[2] = {b.u, b.v};
            const float uv3[2] = {c.u, c.v};
            ps2gs_draw_triangle_textured(gs, gstex, xy1, uv1, ca, xy2, uv2, cb, xy3, uv3, cc);
        }
        else {
            ps2gs_draw_triangle_gouraud(gs, xy1, ca, xy2, cb, xy3, cc);
        }
    }
}

void ps2Context::DrawGouraudQuad(float x0, float y0, float r0, float g0, float b0, float a0,
                                 float x1, float y1, float r1, float g1, float b1, float a1,
                                 float x2, float y2, float r2, float g2, float b2, float a2,
                                 float x3, float y3, float r3, float g3, float b3, float a3) {
    if (gs == nullptr) {
        return;
    }

    const unsigned int c0 = Pack(FloatToByte(r0), FloatToByte(g0), FloatToByte(b0), FloatToByte(a0));
    const unsigned int c1 = Pack(FloatToByte(r1), FloatToByte(g1), FloatToByte(b1), FloatToByte(a1));
    const unsigned int c2 = Pack(FloatToByte(r2), FloatToByte(g2), FloatToByte(b2), FloatToByte(a2));
    const unsigned int c3 = Pack(FloatToByte(r3), FloatToByte(g3), FloatToByte(b3), FloatToByte(a3));

    // Same triangulation as the GL backend: (v0, v1, v2) and (v1, v3, v2).
    const float p0[2] = {x0, y0};
    const float p1[2] = {x1, y1};
    const float p2[2] = {x2, y2};
    const float p3[2] = {x3, y3};

    ps2gs_draw_triangle_gouraud(gs, p0, c0, p1, c1, p2, c2);
    ps2gs_draw_triangle_gouraud(gs, p1, c1, p3, c3, p2, c2);
}

void ps2Context::DrawFilledCircle(pddiBaseShader* shader, float centerX, float centerY,
                                  float radiusX, float radiusY, float u0, float v0,
                                  float u1, float v1, int segments) {
    if (gs == nullptr || segments < 3) {
        return;
    }

    pddiColour colour;
    Ps2GsTexture* tex = ResolveTexture(shader, &colour);
    if (tex == nullptr) {
        return;
    }

    ps2gs_set_clamp(gs, 1);
    ps2gs_bind_texture(gs, tex);

    const unsigned int c = Pack(colour);
    const float midU = (u0 + u1) * 0.5f;
    const float midV = (v0 + v1) * 0.5f;
    const float halfU = (u1 - u0) * 0.5f;
    const float halfV = (v1 - v0) * 0.5f;

    for (int i = 0; i < segments; ++i) {
        const float a0 = (float)i / (float)segments * 6.2831853f;
        const float a1 = (float)(i + 1) / (float)segments * 6.2831853f;
        const float cos0 = std::cos(a0);
        const float sin0 = std::sin(a0);
        const float cos1 = std::cos(a1);
        const float sin1 = std::sin(a1);

        const float xy1[2] = {centerX, centerY};
        const float xy2[2] = {centerX + cos0 * radiusX, centerY + sin0 * radiusY};
        const float xy3[2] = {centerX + cos1 * radiusX, centerY + sin1 * radiusY};

        const float uv1[2] = {midU, midV};
        const float uv2[2] = {midU + cos0 * halfU, midV + sin0 * halfV};
        const float uv3[2] = {midU + cos1 * halfU, midV + sin1 * halfV};

        ps2gs_draw_triangle_textured(gs, tex, xy1, uv1, c, xy2, uv2, c, xy3, uv3, c);
    }
}

void ps2Context::DrawCircle(pddiBaseShader* shader, float centerX, float centerY,
                            float radiusX, float radiusY, float thickness,
                            float u0, float v0, float u1, float v1, int segments) {
    if (gs == nullptr || segments < 3) {
        return;
    }

    pddiColour colour;
    Ps2GsTexture* tex = ResolveTexture(shader, &colour);
    if (tex == nullptr) {
        return;
    }

    ps2gs_set_clamp(gs, 1);
    ps2gs_bind_texture(gs, tex);

    const unsigned int c = Pack(colour);
    const float midU = (u0 + u1) * 0.5f;
    const float midV = (v0 + v1) * 0.5f;
    const float halfU = (u1 - u0) * 0.5f;
    const float halfV = (v1 - v0) * 0.5f;

    const float outerX = radiusX;
    const float outerY = radiusY;
    float innerX = radiusX - thickness;
    float innerY = radiusY - thickness;
    if (innerX < 0.0f) innerX = 0.0f;
    if (innerY < 0.0f) innerY = 0.0f;

    const float innerScaleX = (outerX > 0.0f) ? (innerX / outerX) : 0.0f;
    const float innerScaleY = (outerY > 0.0f) ? (innerY / outerY) : 0.0f;

    for (int i = 0; i < segments; ++i) {
        const float a0 = (float)i / (float)segments * 6.2831853f;
        const float a1 = (float)(i + 1) / (float)segments * 6.2831853f;
        const float cos0 = std::cos(a0);
        const float sin0 = std::sin(a0);
        const float cos1 = std::cos(a1);
        const float sin1 = std::sin(a1);

        // A quad between the inner and outer ring, as two triangles.
        const float ox0 = centerX + cos0 * outerX;
        const float oy0 = centerY + sin0 * outerY;
        const float ox1 = centerX + cos1 * outerX;
        const float oy1 = centerY + sin1 * outerY;
        const float ix0 = centerX + cos0 * innerX;
        const float iy0 = centerY + sin0 * innerY;
        const float ix1 = centerX + cos1 * innerX;
        const float iy1 = centerY + sin1 * innerY;

        const float ou0 = midU + cos0 * halfU;
        const float ov0 = midV + sin0 * halfV;
        const float ou1 = midU + cos1 * halfU;
        const float ov1 = midV + sin1 * halfV;
        const float iu0 = midU + cos0 * halfU * innerScaleX;
        const float iv0 = midV + sin0 * halfV * innerScaleY;
        const float iu1 = midU + cos1 * halfU * innerScaleX;
        const float iv1 = midV + sin1 * halfV * innerScaleY;

        const float o0[2] = {ox0, oy0};
        const float o1[2] = {ox1, oy1};
        const float i1[2] = {ix1, iy1};
        const float i0[2] = {ix0, iy0};

        const float ouv0[2] = {ou0, ov0};
        const float ouv1[2] = {ou1, ov1};
        const float iuv1[2] = {iu1, iv1};
        const float iuv0[2] = {iu0, iv0};

        ps2gs_draw_triangle_textured(gs, tex, o0, ouv0, c, o1, ouv1, c, i1, iuv1, c);
        ps2gs_draw_triangle_textured(gs, tex, o0, ouv0, c, i1, iuv1, c, i0, iuv0, c);
    }
}

ps2Context::VramImage* ps2Context::GetVram(u32 handle) {
    if (handle == 0 || handle > vramImages.size()) {
        return nullptr;
    }
    VramImage& img = vramImages[handle - 1];
    return img.pixels != nullptr ? &img : nullptr;
}

void ps2Context::InvalidatePages(u32 handle) {
    for (auto& entry : pageCache) {
        if (entry.handle == handle) {
            entry.handle = 0;
        }
    }
}

void ps2Context::InvalidatePagesRect(u32 handle, int minX, int minY, int maxX, int maxY) {
    auto overlaps = [](int aMin, int aMax, int bMin, int bMax) {
        return aMin < bMax && bMin < aMax;
    };

    for (auto& entry : pageCache) {
        if (entry.handle != handle) {
            continue;
        }
        const u32 depth = (entry.tpage >> 7) & 3u;
        const int pageX = static_cast<int>(entry.tpage & 0x0fu) * 64;
        const int pageY = static_cast<int>((entry.tpage >> 4) & 1u) * 256;
        const int pageWordsWide = depth == 0 ? 64 : (depth == 1 ? 128 : 256);

        const int clutX = static_cast<int>(entry.cba & 0x3fu) * 16;
        const int clutY = static_cast<int>((entry.cba >> 6) & 0x1ffu);
        const int clutWordsWide = (depth == 0) ? 16 : 256;

        const bool pageHit = overlaps(pageX, pageX + pageWordsWide, minX, maxX)
                          && overlaps(pageY, pageY + 256, minY, maxY);
        const bool clutHit = overlaps(clutX, clutX + clutWordsWide, minX, maxX)
                          && (clutY >= minY && clutY < maxY);

        if (pageHit || clutHit) {
            entry.handle = 0;
        }
    }
}

void ps2Context::SetVRAMHandle(u32 handle) {
    vramHandle = handle;
}

void ps2Context::SetTexInfoOverride(bool enabled, u32 texInfoWord) {
    texInfoOverrideEnabled = enabled;
    texInfoOverrideWord = texInfoWord;
}

u32 ps2Context::CreateVRAMTexture(int w, int h, const u16* data) {
    if (w <= 0 || h <= 0) {
        return 0;
    }
    for (size_t i = 0; i < vramImages.size(); ++i) {
        VramImage& img = vramImages[i];
        if (img.pixels != nullptr) {
            if (i == 0 && img.width == w && img.height == h) {
                InvalidatePages(static_cast<u32>(i + 1));
                if (data != nullptr) {
                    std::memcpy(img.pixels, data, static_cast<size_t>(w) * h * sizeof(u16));
                }
                return static_cast<u32>(i + 1);
            }
            continue;
        }
        img.width = w;
        img.height = h;
        img.pixels = static_cast<u16*>(std::malloc(static_cast<size_t>(w) * h * sizeof(u16)));
        if (img.pixels == nullptr) {
            return 0;
        }
        if (data != nullptr) {
            std::memcpy(img.pixels, data, static_cast<size_t>(w) * h * sizeof(u16));
        }
        InvalidatePages(static_cast<u32>(i + 1));
        return static_cast<u32>(i + 1);
    }

    VramImage img;
    img.width = w;
    img.height = h;
    img.pixels = static_cast<u16*>(std::malloc(static_cast<size_t>(w) * h * sizeof(u16)));
    if (img.pixels == nullptr) {
        return 0;
    }
    if (data != nullptr) {
        std::memcpy(img.pixels, data, static_cast<size_t>(w) * h * sizeof(u16));
    }
    vramImages.push_back(img);
    const u32 newHandle = static_cast<u32>(vramImages.size());
    InvalidatePages(newHandle);
    return newHandle;
}

void ps2Context::DestroyVRAMTexture(u32 handle) {
    VramImage* img = GetVram(handle);
    if (!img) {
        return;
    }
    InvalidatePages(handle);
    const bool isReservedPsxVram = (handle == 1 && img->width == 1024 && img->height == 512);
    if (!isReservedPsxVram) {
        std::free(img->pixels);
        *img = {};
    }
}

void ps2Context::UpdateVRAMTexture(u32 handle, int w, int h, const u16* data) {
    VramImage* img = GetVram(handle);
    if (!img || !data || img->width != w || img->height != h) {
        return;
    }
    std::memcpy(img->pixels, data, static_cast<size_t>(w) * h * sizeof(u16));
    InvalidatePages(handle);
}

void ps2Context::UpdateVRAMTextureRegion(u32 handle, int textureW, int textureH,
                                         const u16* data, int x, int y,
                                         int w, int h) {
    VramImage* img = GetVram(handle);
    if (!img || !data || textureW != img->width || textureH != img->height ||
        w <= 0 || h <= 0 || x >= textureW || y >= textureH) {
        return;
    }
    const int startX = std::max(0, x);
    const int startY = std::max(0, y);
    UpdateVRAMTextureRect(handle, startX, startY, (x + w) - startX, (y + h) - startY,
                          textureW,
                          data + static_cast<size_t>(startY) * textureW + startX);
}

void ps2Context::UpdateVRAMTextureRect(u32 handle, int x, int y, int w, int h,
                                       int srcPitchTexels, const u16* data) {
    VramImage* img = GetVram(handle);
    if (!img || !data || w <= 0 || h <= 0 || srcPitchTexels < w) {
        return;
    }
    const int minX = std::max(0, x);
    const int minY = std::max(0, y);
    const int maxX = std::min(img->width, x + w);
    const int maxY = std::min(img->height, y + h);
    if (minX >= maxX || minY >= maxY) {
        return;
    }

    const size_t rowBytes = static_cast<size_t>(maxX - minX) * sizeof(u16);
    for (int row = minY; row < maxY; ++row) {
        u16* dst = img->pixels + row * img->width + minX;
        const u16* src = data + static_cast<size_t>(row - y) * srcPitchTexels + (minX - x);
        std::memcpy(dst, src, rowBytes);
    }

    InvalidatePagesRect(handle, minX, minY, maxX, maxY);
}

static inline u16 SampleVramSafe(const u16* pixels, int width, int height, int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return 0;
    }
    return pixels[y * width + x];
}

Ps2GsTexture* ps2Context::GetOrCreatePageTexture(u32 handle, u16 tpage, u16 cba) {
    VramImage* img = GetVram(handle);
    if (img == nullptr || img->pixels == nullptr) {
        return nullptr;
    }

    const u16 normTpage = tpage & 0x019Fu;

    for (auto& entry : pageCache) {
        if (entry.handle == handle && entry.tpage == normTpage && entry.cba == cba) {
            entry.lastUsedTick = ++pageCacheTick;
            return entry.texture;
        }
    }

    PageCacheEntry* target = nullptr;
    for (auto& entry : pageCache) {
        if (entry.handle == 0) {
            target = &entry;
            break;
        }
        if (target == nullptr || entry.lastUsedTick < target->lastUsedTick) {
            target = &entry;
        }
    }
    if (target == nullptr) {
        return nullptr;
    }

    if (decodeScratch.size() < 256 * 256 * 4) {
        decodeScratch.resize(256 * 256 * 4);
    }
    u8* out = decodeScratch.data();

    const int tx = normTpage & 0x0Fu;
    const int ty = (normTpage >> 4) & 1;
    const int depth = (normTpage >> 7) & 3;

    const int pageX = tx * 64;
    const int pageY = ty * 256;

    const int clutX = (cba & 0x3Fu) * 16;
    const int clutY = (cba >> 6) & 0x1FFu;

    const u16* vram = img->pixels;
    const int vw = img->width;
    const int vh = img->height;

    if (depth == 0) {
        u16 clut[16];
        for (int i = 0; i < 16; ++i) {
            clut[i] = SampleVramSafe(vram, vw, vh, clutX + i, clutY);
        }
        for (int y = 0; y < 256; ++y) {
            const int wordY = pageY + y;
            for (int x = 0; x < 256; ++x) {
                const int wordX = pageX + (x >> 2);
                const u16 word = SampleVramSafe(vram, vw, vh, wordX, wordY);
                const int nibble = (word >> ((x & 3) << 2)) & 0x0F;
                const u16 color = clut[nibble];

                const bool transparent = (nibble == 0) || (color == 0) || ((color & 0x7FFFu) == 0x7C1Fu);
                const u8 r5 = static_cast<u8>(color & 0x1F);
                const u8 g5 = static_cast<u8>((color >> 5) & 0x1F);
                const u8 b5 = static_cast<u8>((color >> 10) & 0x1F);

                const int idx = (y * 256 + x) * 4;
                out[idx + 0] = static_cast<u8>((r5 << 3) | (r5 >> 2));
                out[idx + 1] = static_cast<u8>((g5 << 3) | (g5 >> 2));
                out[idx + 2] = static_cast<u8>((b5 << 3) | (b5 >> 2));
                out[idx + 3] = transparent ? 0 : 255;
            }
        }
    }
    else if (depth == 1) {
        u16 clut[256];
        for (int i = 0; i < 256; ++i) {
            clut[i] = SampleVramSafe(vram, vw, vh, clutX + i, clutY);
        }
        for (int y = 0; y < 256; ++y) {
            const int wordY = pageY + y;
            for (int x = 0; x < 256; ++x) {
                const int wordX = pageX + (x >> 1);
                const u16 word = SampleVramSafe(vram, vw, vh, wordX, wordY);
                const int byteIdx = (x & 1) ? ((word >> 8) & 0xFF) : (word & 0xFF);
                const u16 color = clut[byteIdx];

                const bool transparent = (byteIdx == 0) || (color == 0) || ((color & 0x7FFFu) == 0x7C1Fu);
                const u8 r5 = static_cast<u8>(color & 0x1F);
                const u8 g5 = static_cast<u8>((color >> 5) & 0x1F);
                const u8 b5 = static_cast<u8>((color >> 10) & 0x1F);

                const int idx = (y * 256 + x) * 4;
                out[idx + 0] = static_cast<u8>((r5 << 3) | (r5 >> 2));
                out[idx + 1] = static_cast<u8>((g5 << 3) | (g5 >> 2));
                out[idx + 2] = static_cast<u8>((b5 << 3) | (b5 >> 2));
                out[idx + 3] = transparent ? 0 : 255;
            }
        }
    }
    else {
        for (int y = 0; y < 256; ++y) {
            const int wordY = pageY + y;
            for (int x = 0; x < 256; ++x) {
                const int wordX = pageX + x;
                const u16 color = SampleVramSafe(vram, vw, vh, wordX, wordY);

                const bool transparent = (color == 0) || ((color & 0x7FFFu) == 0x7C1Fu);
                const u8 r5 = static_cast<u8>(color & 0x1F);
                const u8 g5 = static_cast<u8>((color >> 5) & 0x1F);
                const u8 b5 = static_cast<u8>((color >> 10) & 0x1F);

                const int idx = (y * 256 + x) * 4;
                out[idx + 0] = static_cast<u8>((r5 << 3) | (r5 >> 2));
                out[idx + 1] = static_cast<u8>((g5 << 3) | (g5 >> 2));
                out[idx + 2] = static_cast<u8>((b5 << 3) | (b5 >> 2));
                out[idx + 3] = transparent ? 0 : 255;
            }
        }
    }

    if (!ps2gs_texture_upload(target->texture, 256, 256, PS2GS_FORMAT_RGBA16, out)) {
        return nullptr;
    }
    ps2gs_texture_set_filter(target->texture, 0);

    target->handle = handle;
    target->tpage = normTpage;
    target->cba = cba;
    target->lastUsedTick = ++pageCacheTick;
    return target->texture;
}

void ps2Context::DrawPrimBuffer(pddiPrimBuffer* buffer, u32 indexOffset, u32 indexCount) {
    if (gs == nullptr || buffer == nullptr) {
        return;
    }

    ps2PrimBuffer* ps2buffer = dynamic_cast<ps2PrimBuffer*>(buffer);
    if (ps2buffer == nullptr) {
        return;
    }

    const std::vector<Ps2Vertex>& vertices = ps2buffer->Vertices();
    const std::vector<u16>& indices = ps2buffer->Indices();
    if (vertices.empty()) {
        return;
    }

    const bool textured = (ps2buffer->VertexFormat() & PDDI_V_UV) != 0;
    const bool hasTexInfo = (ps2buffer->VertexFormat() & PDDI_V_TEXINFO) != 0;

    pddiColour colour;
    Ps2GsTexture* explicitTex = ResolveTexture(nullptr, &colour);
    if (textured && explicitTex != nullptr) {
        ps2gs_set_clamp(gs, 1);
        ps2gs_bind_texture(gs, explicitTex);
    }

    Ps2GsTexture* currentBoundTex = explicitTex;
    u16 lastTpage = 0xFFFF;
    u16 lastCba = 0xFFFF;

    const size_t start = indices.empty() ? 0 : (size_t)indexOffset;
    const size_t count = indices.empty()
        ? vertices.size()
        : (indexCount != 0 ? (size_t)indexCount : indices.size() - (size_t)indexOffset);

    for (size_t i = 0; i + 2 < count; i += 3) {
        const Ps2Vertex* v[3];
        for (int k = 0; k < 3; ++k) {
            const size_t slot = start + i + (size_t)k;
            const size_t index = indices.empty() ? slot : (size_t)indices[slot];
            if (index >= vertices.size()) {
                return;
            }
            v[k] = &vertices[index];
        }

        const unsigned int c0 = Pack(FloatToByte(v[0]->r), FloatToByte(v[0]->g), FloatToByte(v[0]->b), 0xff);
        const unsigned int c1 = Pack(FloatToByte(v[1]->r), FloatToByte(v[1]->g), FloatToByte(v[1]->b), 0xff);
        const unsigned int c2 = Pack(FloatToByte(v[2]->r), FloatToByte(v[2]->g), FloatToByte(v[2]->b), 0xff);

        const float xy0[2] = {v[0]->x, v[0]->y};
        const float xy1[2] = {v[1]->x, v[1]->y};
        const float xy2[2] = {v[2]->x, v[2]->y};

        Ps2GsTexture* triTex = explicitTex;
        bool triTextured = textured && (triTex != nullptr);
        float uvScale = 1.0f;

        if (explicitTex == nullptr && textured && hasTexInfo && vramHandle != 0) {
            s32 tpage = static_cast<s32>(v[0]->texInfo[0]);
            s32 cba = static_cast<s32>(v[0]->texInfo[1]);
            if (texInfoOverrideEnabled) {
                tpage = static_cast<s32>((texInfoOverrideWord >> 16) & 0xFFFFu);
                cba = static_cast<s32>(texInfoOverrideWord & 0xFFFFu);
            }

            if (tpage >= 0) {
                const u16 uTpage = static_cast<u16>(tpage);
                const u16 uCba = static_cast<u16>(cba);
                if (currentBoundTex == nullptr || uTpage != lastTpage || uCba != lastCba) {
                    triTex = GetOrCreatePageTexture(vramHandle, uTpage, uCba);
                    if (triTex != nullptr) {
                        ps2gs_set_clamp(gs, 1);
                        ps2gs_bind_texture(gs, triTex);
                        currentBoundTex = triTex;
                        lastTpage = uTpage;
                        lastCba = uCba;
                    }
                } else {
                    triTex = currentBoundTex;
                }
                if (triTex != nullptr) {
                    triTextured = true;
                    uvScale = 1.0f / 256.0f;
                }
            }
        }

        if (triTextured && triTex != nullptr) {
            const float uv0[2] = {v[0]->u * uvScale, v[0]->v * uvScale};
            const float uv1[2] = {v[1]->u * uvScale, v[1]->v * uvScale};
            const float uv2[2] = {v[2]->u * uvScale, v[2]->v * uvScale};
            ps2gs_draw_triangle_textured(gs, triTex, xy0, uv0, c0, xy1, uv1, c1, xy2, uv2, c2);
        }
        else {
            ps2gs_draw_triangle_gouraud(gs, xy0, c0, xy1, c1, xy2, c2);
        }
    }
}

void ps2Context::SetRealTextureRect(float offsetX, float offsetY, float sizeX, float sizeY) {
    (void)offsetX;
    (void)offsetY;
    (void)sizeX;
    (void)sizeY;
}

// ps2Device

pddiDisplay* ps2Device::NewDisplay() {
    return new ps2Display();
}

pddiRenderContext* ps2Device::NewRenderContext(pddiDisplay* display) {
    return new ps2Context(static_cast<ps2Display*>(display));
}

pddiPrimBuffer* ps2Device::NewPrimBuffer(const pddiPrimBufferDesc& desc) {
    return new ps2PrimBuffer(desc);
}

// The platform factory: the PS2 build links this backend instead of a desktop
// one, exactly like the PSP and null backends.
pddiDevice* pddiCreate() {
    return new ps2Device();
}
