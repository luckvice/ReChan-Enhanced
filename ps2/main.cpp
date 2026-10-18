// ReChan — PlayStation 2 (Emotion Engine + Graphics Synthesizer) boot target.
//
// Step 2 of the port: the app boots through the pddi device interface
// (vendor/libp3d/pddi/ps2), the same way the desktop and PSP builds do, and
// renders a verification pattern at the console's native 640x448 NTSC mode.
//
// The pattern exercises the paths the game will need:
//   - an RGBA32 texture (DrawQuad with a shader-bound texture),
//   - an 8-bit CLUT texture (the palette path PSX-era art uses),
//   - a gouraud quad (per-vertex colours),
//   - a triangle batch (DrawQuadBatch, the sprite/UI path).
//
// Two log channels: stdout (EE TTY) and host:/rechan-ps2.log, a PCSX2 device
// that maps to dist/ps2.

#include "pddi/pddidev.h"
#include "pddi/pddishad.h"
#include "pddi/pdditex.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const int kWidth = 640;
static const int kHeight = 448;

static FILE* g_log = 0;

static void Log(const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fputs(buf, stdout);
    fflush(stdout);
    if (g_log != 0) {
        fputs(buf, g_log);
        fflush(g_log);
    }
}

// Solid-colour quadrants, in the order top-left, top-right, bottom-left,
// bottom-right. `rgba` is always RGBA8; the backend converts it to whatever GS
// format the texture asks for.
static void MakeQuadrants(unsigned char* dst, int w, int h,
                          const unsigned char colours[4][3]) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int quadrant = (y < h / 2 ? 0 : 2) + (x < w / 2 ? 0 : 1);
            const unsigned char* c = colours[quadrant];
            unsigned char* px = dst + ((size_t)y * w + x) * 4;
            px[0] = c[0];
            px[1] = c[1];
            px[2] = c[2];
            px[3] = 0xff;
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);

    Log("[RECHAN-PS2] boot: pddi device (EE/GS), native resolution %dx%d\n", kWidth, kHeight);

    g_log = fopen("host:/rechan-ps2.log", "w");
    Log("[RECHAN-PS2] file log: %s\n", g_log ? "host:/rechan-ps2.log" : "unavailable");

    pddiDevice* device = pddiCreate();
    if (device == 0) {
        Log("[RECHAN-PS2] ERROR: pddiCreate failed\n");
        return 1;
    }
    Log("[RECHAN-PS2] device ok (%p)\n", (void*)device);

    pddiDisplay* display = device->NewDisplay();
    Log("[RECHAN-PS2] display object ok\n");
    pddiDisplayInit init;
    init.xSize = kWidth;
    init.ySize = kHeight;
    init.title = "ReChan";
    if (!display->InitDisplay(init)) {
        Log("[RECHAN-PS2] ERROR: InitDisplay failed\n");
        return 1;
    }
    Log("[RECHAN-PS2] display ready: %dx%d\n", display->GetWidth(), display->GetHeight());

    pddiRenderContext* ctx = device->NewRenderContext(display);
    pddiGamepad* pad = device->NewGamepad();

    pddiBaseShader* shader = device->NewShader("simple");
    shader->SetColour(PDDI_SP::DIFFUSE, pddiColour(255, 255, 255, 255));

    // Texture A: RGBA32, four quadrants.
    static const unsigned char kQuadsA[4][3] = {
        {0xf0, 0xc0, 0x20},  // yellow
        {0xd0, 0x30, 0x20},  // red
        {0x20, 0xd0, 0x50},  // green
        {0xd0, 0x20, 0xc0},  // magenta
    };
    unsigned char imageA[128 * 128 * 4];
    MakeQuadrants(imageA, 128, 128, kQuadsA);
    pddiTexture* textureA = device->NewTexture();
    textureA->SetData(128, 128, 32, 8, imageA);
    textureA->SetFilterMode(PDDI_FILTER_NONE);

    // Texture B: the same layout stored as 8-bit indices with a CLUT.
    static const unsigned char kQuadsB[4][3] = {
        {0x20, 0xd0, 0xe0},  // cyan
        {0xf0, 0x80, 0x20},  // orange
        {0x70, 0x20, 0xd0},  // purple
        {0xff, 0xff, 0xff},  // white
    };
    unsigned char imageB[64 * 64 * 4];
    MakeQuadrants(imageB, 64, 64, kQuadsB);
    pddiTexture* textureB = device->NewTexture();
    textureB->SetData(64, 64, 8, 8, imageB);
    textureB->SetFilterMode(PDDI_FILTER_NONE);

    // Triangle batch: two flat triangles covering a quad (per-vertex colours).
    float batchColour[3] = {0x80 / 255.0f, 0xff / 255.0f, 0x00 / 255.0f};
    const float bx0 = 352.0f, by0 = 64.0f, bx1 = 592.0f, by1 = 192.0f;
    float batchVerts[6 * 8];
    {
        const float positions[6][2] = {
            {bx0, by0}, {bx1, by0}, {bx1, by1},
            {bx0, by0}, {bx1, by1}, {bx0, by1},
        };
        for (int i = 0; i < 6; ++i) {
            float* v = batchVerts + i * 8;
            v[0] = positions[i][0];
            v[1] = positions[i][1];
            v[2] = 0.0f;
            v[3] = batchColour[0];
            v[4] = batchColour[1];
            v[5] = batchColour[2];
            v[6] = (positions[i][0] - bx0) / (bx1 - bx0);
            v[7] = (positions[i][1] - by0) / (by1 - by0);
        }
    }
    pddiPrimBuffer* batch = device->NewPrimBuffer(
        pddiPrimBufferDesc(PDDI_PRIM_TRIANGLES, PDDI_V_POSITION | PDDI_V_COLOUR | PDDI_V_UV, 6, 0));
    batch->SetVertexData(batchVerts, 6);

    const float teal[3] = {0x20 / 255.0f, 0xa0 / 255.0f, 0xa0 / 255.0f};
    ctx->SetClearColour(pddiColour(0x10, 0x20, 0x60));

    Log("[RECHAN-PS2] scene: textureA(RGBA32) textureB(CLUT) gouraud batch\n");

    u64 frames = 0;
    while (!display->ShouldClose()) {
        display->PollEvents();
        if (pad != 0) {
            pad->Poll();
        }

        ctx->BeginFrame();
        ctx->Clear(PDDI_BUFFER_ALL);
        ctx->SetBlendMode(PDDI_BLEND_NONE);

        shader->SetTexture(PDDI_SP::TEXTURE, textureA);
        ctx->DrawQuad(shader, 32.0f, 48.0f, 256.0f, 160.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        shader->SetTexture(PDDI_SP::TEXTURE, textureB);
        ctx->DrawQuad(shader, 352.0f, 240.0f, 240.0f, 160.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        ctx->DrawGouraudQuad(32.0f, 272.0f, teal[0], teal[1], teal[2], 1.0f,
                             288.0f, 272.0f, teal[0], teal[1], teal[2], 1.0f,
                             32.0f, 352.0f, teal[0], teal[1], teal[2], 1.0f,
                             288.0f, 352.0f, teal[0], teal[1], teal[2], 1.0f);

        ctx->SetTexture(0);
        ctx->DrawPrimBuffer(batch, 0, 0);

        ctx->EndFrame();
        display->SwapBuffers();

        ++frames;
        if ((frames % 60) == 0) {
            Log("[RECHAN-PS2] frame %llu ok\n", (unsigned long long)frames);
        }
    }

    Log("[RECHAN-PS2] done: %llu frames drawn at %dx%d\n",
        (unsigned long long)frames, kWidth, kHeight);

    if (g_log != 0) {
        fclose(g_log);
        g_log = 0;
    }
    return 0;
}
