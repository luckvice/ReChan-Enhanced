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
    else if (bpp <= 16) {
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

// ps2Context

ps2Context::ps2Context(ps2Display* display) : gs(display != nullptr ? display->Gs() : nullptr) {}

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

    pddiColour colour;
    Ps2GsTexture* tex = ResolveTexture(nullptr, &colour);
    if (textured && tex != nullptr) {
        ps2gs_set_clamp(gs, 1);
        ps2gs_bind_texture(gs, tex);
    }

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

        if (textured && tex != nullptr) {
            const float uv0[2] = {v[0]->u, v[0]->v};
            const float uv1[2] = {v[1]->u, v[1]->v};
            const float uv2[2] = {v[2]->u, v[2]->v};
            ps2gs_draw_triangle_textured(gs, tex, xy0, uv0, c0, xy1, uv1, c1, xy2, uv2, c2);
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
