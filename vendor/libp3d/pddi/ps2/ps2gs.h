// ps2gs.h — the GS boundary of the PS2 backend.
//
// ps2sdk's tamtypes.h (pulled in by gsKit) declares `u32` as `unsigned int`,
// while libp3d's core.h declares it as `uint32_t`, which on this target is
// `unsigned long`: two distinct types with the same width, and C++ rejects the
// redeclaration. The two header worlds therefore cannot meet inside a single
// translation unit, so the backend is split in two:
//
//   ps2render.cpp — the pddi implementation, no ps2sdk/gsKit at all;
//   ps2gs.cpp     — everything that touches gsKit, behind this bridge.
//
// The bridge deliberately uses plain types only. Colours cross it packed as
// r | g<<8 | b<<16 | a<<24, the byte order the GS itself uses.
#pragma once

struct Ps2Gs;         // display, queue and pad port
struct Ps2GsTexture;  // a texture known to gsKit's texture manager

// Storage format requested for an uploaded texture. The pddi backend picks one
// from the texture's bpp; gsKit/GS specifics stay on the other side.
enum Ps2GsFormat {
    PS2GS_FORMAT_RGBA32 = 0,
    PS2GS_FORMAT_RGBA16,
    PS2GS_FORMAT_CLUT8,
    PS2GS_FORMAT_CLUT4,
};

// GS alpha unit configurations, named after the pddi blend modes.
enum Ps2GsBlend {
    PS2GS_BLEND_NONE = 0,
    PS2GS_BLEND_ALPHA,
    PS2GS_BLEND_ADD,
    PS2GS_BLEND_SUBTRACT,
    PS2GS_BLEND_HALF,  // PSX "half transparent": (Cs - Cd) * 0.5 + Cd
};

// What gsKit ended up configuring, for the boot log.
struct Ps2GsModeInfo {
    int width;
    int height;
    int interlace;
    int field;
    int dw;
    int dh;
    int magH;
    int magV;
};

// Display, queue and pad.
Ps2Gs* ps2gs_init(int width, int height);
void ps2gs_shutdown(Ps2Gs* gs);
Ps2GsModeInfo ps2gs_mode_info(const Ps2Gs* gs);
void ps2gs_frame_end(Ps2Gs* gs);

// Frame state.
void ps2gs_clear(Ps2Gs* gs, unsigned char r, unsigned char g, unsigned char b);
void ps2gs_set_ztest(Ps2Gs* gs, int enable);
void ps2gs_set_atest(Ps2Gs* gs, int enable);
void ps2gs_set_blend(Ps2Gs* gs, int blend);
void ps2gs_set_scissor(Ps2Gs* gs, int x, int y, int w, int h);

// Textures.
Ps2GsTexture* ps2gs_texture_create(void);
void ps2gs_texture_destroy(Ps2GsTexture* texture);
// Uploads RGBA8 pixels, converting to the requested storage format. The
// palettised formats build a CLUT from the colours the image actually uses.
// Returns 1 on success.
int ps2gs_texture_upload(Ps2GsTexture* texture, int width, int height, int format,
                         const void* rgba8);
void ps2gs_texture_set_filter(Ps2GsTexture* texture, int linear);
int ps2gs_texture_width(const Ps2GsTexture* texture);
int ps2gs_texture_height(const Ps2GsTexture* texture);

// Drawing. UVs arrive normalised (0..1) and are converted to texels here, next
// to the texture that defines the scale.
void ps2gs_bind_texture(Ps2Gs* gs, Ps2GsTexture* texture);
void ps2gs_set_clamp(Ps2Gs* gs, int clamp);

void ps2gs_draw_sprite_flat(Ps2Gs* gs, float x1, float y1, float x2, float y2,
                            unsigned int rgba);
void ps2gs_draw_sprite_textured(Ps2Gs* gs, Ps2GsTexture* texture,
                                float x1, float y1, float u1, float v1,
                                float x2, float y2, float u2, float v2,
                                unsigned int rgba);
void ps2gs_draw_triangle_gouraud(Ps2Gs* gs, const float* xy1, unsigned int c1,
                                 const float* xy2, unsigned int c2,
                                 const float* xy3, unsigned int c3);
void ps2gs_draw_triangle_textured(Ps2Gs* gs, Ps2GsTexture* texture,
                                  const float* xy1, const float* uv1, unsigned int c1,
                                  const float* xy2, const float* uv2, unsigned int c2,
                                  const float* xy3, const float* uv3, unsigned int c3);

// The pad is the console's only input device. The button bits are the ones
// libpad reports (active low) and are mirrored here so the backend does not
// have to include libpad.h; the sticks come back centred at 128.
enum Ps2GsPadButton {
    PS2GS_PAD_SELECT = 0x0001,
    PS2GS_PAD_L3 = 0x0002,
    PS2GS_PAD_R3 = 0x0004,
    PS2GS_PAD_START = 0x0008,
    PS2GS_PAD_UP = 0x0010,
    PS2GS_PAD_RIGHT = 0x0020,
    PS2GS_PAD_DOWN = 0x0040,
    PS2GS_PAD_LEFT = 0x0080,
    PS2GS_PAD_L2 = 0x0100,
    PS2GS_PAD_R2 = 0x0200,
    PS2GS_PAD_L1 = 0x0400,
    PS2GS_PAD_R1 = 0x0800,
    PS2GS_PAD_TRIANGLE = 0x1000,
    PS2GS_PAD_CIRCLE = 0x2000,
    PS2GS_PAD_CROSS = 0x4000,
    PS2GS_PAD_SQUARE = 0x8000,
};

struct Ps2GsPadState {
    int connected = 0;
    unsigned short buttons = 0xffff;  // active low
    int leftX = 128;
    int leftY = 128;
    int rightX = 128;
    int rightY = 128;
};

// libpad reaches the pad through the IOP's sio2man/padman drivers, which the
// platform layer loads once the file system is up. Until ps2gs_pad_open() is
// called a poll just reports "disconnected".
void ps2gs_pad_open(void);
void ps2gs_pad_poll(Ps2GsPadState* state);
