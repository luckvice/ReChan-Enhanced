// ps2render.h — PlayStation 2 (Emotion Engine + Graphics Synthesizer) backend.
//
// The GS rasterizes everything: no shaders, no VFPU. Textures live in GS VRAM
// (with CLUT palettes for 4/8-bit art), blending is the fixed-function GS alpha
// unit and Z is a per-primitive register value. This file is the foundation of
// the port: display, render context, 2D/textured draws, gamepad and the boot
// path. The retained-mode mesh path, the PSX VRAM lookup and render targets
// arrive with the following tasks of the epic.
//
// Everything GS-specific sits behind ps2gs.h, so this file is free of ps2sdk and
// gsKit (see that header for why that separation is required).
#pragma once

#include "pddi/pddi.h"
#include "pddi/pdditex.h"
#include "pddi/pddishad.h"
#include "pddi/pddidev.h"

#include "ps2gs.h"

#include <string>
#include <vector>

// Native video mode: 640x480 progressive (480p) at 60 Hz, the port's target.
// The original NTSC-U disc ran 640x448 interlaced; the backend keeps the same
// 4:3 aspect and the full width while gaining the progressive scan.
constexpr int PS2_NATIVE_WIDTH = 640;
constexpr int PS2_NATIVE_HEIGHT = 480;

// One retained-mode vertex, unpacked from the pddi vertex format. The layout the
// other backends consume is position (3 floats), colour (3 floats), uv
// (2 floats) and the PSX texinfo pair (2 floats); colours stay normalised here.
struct Ps2Vertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    float u = 0.0f, v = 0.0f;
    float texInfo[2] = {0.0f, 0.0f};
};

class ps2Texture : public pddiTexture {
public:
    ps2Texture() = default;
    ~ps2Texture() override;

    int GetWidth() override { return width; }
    int GetHeight() override { return height; }
    int GetBpp() override { return bpp; }
    int GetAlphaDepth() override { return alphaDepth; }

    // Input is always RGBA8; the requested bpp selects the GS storage format:
    // 32 -> RGBA32, 16 -> RGBA5551, 8/4 -> CLUT'd indices with a generated
    // palette. The gsKit texture manager owns VRAM placement and the upload.
    void SetData(int w, int h, int bppIn, int alphaDepthIn, const void* rgba) override;
    void SetFilterMode(pddiFilterMode mode) override;
    void Bind(int unit) override { (void)unit; }

    // GS side, used by ps2Context.
    Ps2GsTexture* Native() { return native; }
    bool IsValid() const { return valid; }

private:
    void ReleaseStorage();

    int width = 0;
    int height = 0;
    int bpp = 0;
    int alphaDepth = 0;
    bool filterLinear = false;
    Ps2GsTexture* native = nullptr;
    bool valid = false;
};

class ps2Shader : public pddiBaseShader {
public:
    explicit ps2Shader(const char* type);

    const char* GetType() override { return type.c_str(); }

    void SetTexture(u32 param, pddiTexture* tex) override { (void)param; texture = tex; }
    void SetInt(u32 param, int value) override;
    void SetFloat(u32 param, float value) override { (void)param; (void)value; }
    void SetColour(u32 param, pddiColour c) override;
    void SetInt(const char*, int) override {}
    void SetFloat(const char*, float) override {}
    void SetVector(const char*, float, float, float, float) override {}
    void SetMatrix(const char*, const float*) override {}
    void PreRender() override {}
    void PostRender() override {}

    pddiTexture* Texture() const { return texture; }
    pddiColour Diffuse() const { return diffuse; }
    bool AlphaTest() const { return alphaTest; }

private:
    std::string type;
    pddiTexture* texture = nullptr;
    pddiColour diffuse{255, 255, 255, 255};
    bool alphaTest = false;
};

class ps2PrimBuffer : public pddiPrimBuffer {
public:
    explicit ps2PrimBuffer(const pddiPrimBufferDesc& desc);

    void SetVertexData(const void* data, u32 count) override;
    void SetIndices(const u16* indices, u32 count) override;
    u32 GetIndexCount() const override { return (u32)indices.size(); }
    u32 GetVertexCount() const override { return (u32)vertices.size(); }
    pddiPrimType GetPrimType() const override { return primType; }

    const std::vector<Ps2Vertex>& Vertices() const { return vertices; }
    const std::vector<u16>& Indices() const { return indices; }
    u32 VertexFormat() const { return vertexFormat; }

protected:
    ~ps2PrimBuffer() override = default;

private:
    pddiPrimType primType;
    u32 vertexFormat = 0;
    std::vector<Ps2Vertex> vertices;
    std::vector<u16> indices;
};

// DualShock through libpad, polled by the backend and read through the pddi
// gamepad interface. Buttons are active low in the pad state.
class ps2Gamepad : public pddiGamepad {
public:
    ps2Gamepad() = default;
    ~ps2Gamepad() override = default;

    void Poll() override;
    bool IsConnected() const override { return pad.connected != 0; }
    bool IsButtonDown(int button) const override;
    float GetAxis(int axis) const override;
    bool SupportsVibration() const override { return false; }
    bool SetVibration(float, float) override { return false; }

private:
    Ps2GsPadState pad{};
};

class ps2RenderTarget : public pddiRenderTarget {
public:
    ps2RenderTarget(int w, int h);
    ~ps2RenderTarget() override;

    bool Resize(int w, int h) override { width = w; height = h; return true; }
    int GetWidth() const override { return width; }
    int GetHeight() const override { return height; }
    pddiTexture* GetTexture() const override { return texture; }
    // Off-screen rendering is not part of this increment: the caller is told the
    // target is unusable instead of silently drawing to the framebuffer.
    bool IsValid() const override { return false; }

private:
    int width;
    int height;
    ps2Texture* texture = nullptr;
};

class ps2Display : public pddiDisplay {
public:
    ps2Display();
    ~ps2Display() override;

    bool InitDisplay(const pddiDisplayInit& init) override;
    void SwapBuffers() override;
    int GetWidth() override { return width; }
    int GetHeight() override { return height; }
    bool ShouldClose() override;
    void PollEvents() override;

    bool IsKeyDown(int key) override;
    bool IsMouseButtonDown(int) override { return false; }
    void GetMousePosition(double& x, double& y) override { x = 0.0; y = 0.0; }

    void SetIcon(int, int, const unsigned char*) override {}

    int GetVideoModeCount() override { return 1; }
    void GetVideoMode(int index, pddiVideoMode& mode) override;
    void SetFullscreen(bool value) override { fullscreen = value; }
    bool IsFullscreen() override { return fullscreen; }
    void SetBorderless(bool value) override { borderless = value; }
    bool IsBorderless() override { return borderless; }
    void SetResolution(int w, int h) override { width = w; height = h; }
    void SetVSync(bool enabled) override { vsync = enabled; }
    void SetMSAA(int samples) override { msaa = samples; }
    int GetMSAA() override { return msaa; }
    void SetWindowPos(int, int) override {}

    void SetTitle(const char*) override {}

    void ShowCursor(bool) override {}
    void ClipCursor(bool) override {}

    void SetWndProc(pddiWndProc proc) override { wndProc = proc; }

    void AddOverlayCallback(OverlayCallback) override {}
    void RenderOverlay() override {}

    Ps2Gs* Gs() { return gs; }
    u64 FrameCount() const { return frameCount; }

private:
    Ps2Gs* gs = nullptr;
    // The pad is the console's only input device, so the display polls it and
    // exposes the buttons through IsKeyDown (the gamepad object is separate).
    u16 padButtons = 0xffff;
    int width = PS2_NATIVE_WIDTH;
    int height = PS2_NATIVE_HEIGHT;
    u64 frameCount = 0;
    // 0 = run until the app closes or Start is pressed; the QA harness bounds a
    // run with RECHAN_PS2_MAX_FRAMES.
    u64 maxFrames = 0;
    bool closeRequested = false;
    bool fullscreen = false;
    bool borderless = false;
    bool vsync = true;
    int msaa = 0;
    pddiWndProc wndProc;
};

// The context stores pddi state; everything that maps onto a GS register is
// applied when the next primitive is submitted.
class ps2Context : public pddiRenderContext {
public:
    explicit ps2Context(ps2Display* display);
    ~ps2Context() override;

    void BeginFrame() override {}
    void EndFrame() override {}

    void SetCameraAspect(float aspect) override { cameraAspect = aspect; }
    float GetCameraAspect() const override { return cameraAspect; }

    void SetClearColour(pddiColour c) override { clearColour = c; }
    void Clear(int flags) override;

    void SetProjectionMatrix(const Mat4& m) override { projection = m; }
    void SetViewMatrix(const Mat4& m) override { view = m; }
    void SetWorldMatrix(const Mat4& m) override { world = m; }
    const Mat4& GetWorldMatrix() const override { return world; }
    const Mat4& GetViewMatrix() const override { return view; }
    const Mat4& GetProjectionMatrix() const override { return projection; }

    void SetWorldMirror(bool enable) override { worldMirror = enable; }
    bool GetWorldMirror() const override { return worldMirror; }

    void SetCullMode(pddiCullMode mode) override;
    void EnableZBuffer(bool enable) override;
    void SetBlendMode(pddiBlendMode mode) override;
    void SetDepthClamp(bool enable) override { depthClamp = enable; }
    void SetPolygonOffset(bool enable, f32 factor = 0.0f, f32 units = 0.0f) override;
    void SetScissor(int x, int y, int w, int h) override;
    void SetMultisampleEnabled(bool enable) override { multisample = enable; }
    void ResolveForOverlayPass() override {}

    pddiRenderTarget* CreateRenderTarget(int width, int height, pddiRenderTargetFormat format,
                                         bool withInstanceId = false) override;
    bool SetRenderTarget(pddiRenderTarget* target) override;

    void DrawFilledCircle(pddiBaseShader* shader, float centerX, float centerY,
                          float radiusX, float radiusY, float u0, float v0, float u1, float v1,
                          int segments) override;

    void DrawCircle(pddiBaseShader* shader, float centerX, float centerY,
                    float radiusX, float radiusY, float thickness,
                    float u0, float v0, float u1, float v1, int segments) override;

    void DrawQuad(pddiBaseShader* shader, float x, float y, float w, float h,
                  float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) override;

    void DrawQuadBatch(pddiTexture* tex, pddiBlendMode blend,
                       const pddiBatchVertex* verts, s32 vertCount) override;

    void DrawPrimBuffer(pddiPrimBuffer* buffer, u32 indexOffset = 0, u32 indexCount = 0) override;

    void DrawGouraudQuad(float x0, float y0, float r0, float g0, float b0, float a0,
                         float x1, float y1, float r1, float g1, float b1, float a1,
                         float x2, float y2, float r2, float g2, float b2, float a2,
                         float x3, float y3, float r3, float g3, float b3, float a3) override;

    void SetTexture(pddiTexture* tex) override { boundTexture = tex; }

    // PSX VRAM texture management and decoded page caching.
    void SetVRAMHandle(u32 handle) override;
    void SetTexInfoOverride(bool enabled, u32 texInfoWord) override;
    u32 CreateVRAMTexture(int w, int h, const u16* data) override;
    void DestroyVRAMTexture(u32 handle) override;
    void UpdateVRAMTexture(u32 handle, int w, int h, const u16* data) override;
    void UpdateVRAMTextureRegion(u32 handle, int textureW, int textureH,
                                 const u16* data, int x, int y,
                                 int w, int h) override;
    void UpdateVRAMTextureRect(u32 handle, int x, int y, int w, int h,
                               int srcPitchTexels, const u16* data) override;

    void SetRealTextureMode(bool enabled) override { realTextureMode = enabled; }
    bool IsRealTextureModeEnabled() const override { return realTextureMode; }
    void SetRealTextureRect(float offsetX, float offsetY, float sizeX, float sizeY) override;

    // Shadow mapping is a MODERN_GRAPHICS feature with no PS2 pass yet: the
    // state is stored (or ignored) so callers keep working.
    void SetShadowCasterPass(bool, const Mat4&) override {}
    void SetReceiveShadows(bool) override {}
    void SetShadowCascades(pddiTexture* const*, const Mat4*, const float*, const float*,
                           pddiTexture* const*, int) override {}
    void SetCameraWorldPos(float, float, float) override {}
    void SetShadowLightDirection(float, float, float) override {}
    void SetShadowCasterInstanceId(u32) override {}
    void SetShadowReceiverInstanceId(u32) override {}
    void ClearShadowCasterIdTarget() override {}
    void SetShadowDebugMode(int) override {}

private:
    struct VramImage {
        int width = 0;
        int height = 0;
        u16* pixels = nullptr;
    };

    struct PageCacheEntry {
        u32 handle = 0;
        u16 tpage = 0;
        u16 cba = 0;
        u32 lastUsedTick = 0;
        Ps2GsTexture* texture = nullptr;
    };

    static constexpr size_t kPageCacheCapacity = 16;

    VramImage* GetVram(u32 handle);
    Ps2GsTexture* GetOrCreatePageTexture(u32 handle, u16 tpage, u16 cba);
    void InvalidatePages(u32 handle);
    void InvalidatePagesRect(u32 handle, int minX, int minY, int maxX, int maxY);

    Ps2GsTexture* ResolveTexture(const pddiBaseShader* shader, pddiColour* colour);

    Ps2Gs* gs = nullptr;

    pddiColour clearColour{0, 0, 0, 255};
    pddiTexture* boundTexture = nullptr;
    pddiBlendMode blendMode = PDDI_BLEND_NONE;
    pddiCullMode cullMode = PDDI_CULL_NONE;
    bool zBufferRequested = false;
    bool depthClamp = false;
    bool multisample = false;
    bool realTextureMode = false;
    bool polygonOffset = false;

    std::vector<VramImage> vramImages;
    std::vector<PageCacheEntry> pageCache;
    u32 pageCacheTick = 0;
    std::vector<u8> decodeScratch;

    u32 vramHandle = 0;
    bool texInfoOverrideEnabled = false;
    u32 texInfoOverrideWord = 0;

    Mat4 world, view, projection;
    float cameraAspect = 1.0f;
    bool worldMirror = false;
};

class ps2Device : public pddiDevice {
public:
    pddiDisplay* NewDisplay() override;
    pddiRenderContext* NewRenderContext(pddiDisplay* display) override;
    pddiGamepad* NewGamepad() override { return new ps2Gamepad(); }
    pddiTexture* NewTexture() override { return new ps2Texture(); }
    pddiPrimBuffer* NewPrimBuffer(const pddiPrimBufferDesc& desc) override;
    pddiBaseShader* NewShader(const char* type = "simple") override { return new ps2Shader(type); }
};
