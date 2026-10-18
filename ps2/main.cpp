// ReChan — PlayStation 2 (Emotion Engine + Graphics Synthesizer)
//
// Step 1 of the port: prove the toolchain -> boot -> first render cycle in
// PCSX2, at the console's native resolution (640x448 NTSC, the same format as
// the NTSC-U disc SLUS-00684).
//
// The sequence follows gsKit's canonical example (examples/basic/basic.c):
//   - static content goes into the PERSISTENT queue (replayed every frame);
//   - Z test is turned off after the clear (gsKit_clear draws a sprite at Z=0;
//     without this, primitives with Z>0 are rejected);
//   - the frame loop uses the ONESHOT queue, which is reset on every
//     gsKit_queue_exec.
//
// Two log channels: stdout (EE TTY) and host:/rechan-ps2.log (a PCSX2 device
// that maps to dist/ps2).

#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>

#include <stdarg.h>
#include <stdio.h>

static const int kWidth = 640;
static const int kHeight = 448;
static const int kFrames = 600;  // ~10 s at 60 Hz: long enough to screenshot.

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

static void ReportGlobal(const char* tag, GSGLOBAL* gs) {
    Log("[RECHAN-PS2] %s mode=%d %dx%d interlace=%d field=%d aspect=%d DW=%d DH=%d mag=(%d,%d) psm=%d zbuf=%d dbuf=%d\n",
        tag, (int)gs->Mode, gs->Width, gs->Height, (int)gs->Interlace, (int)gs->Field,
        gs->Aspect, gs->DW, gs->DH, gs->MagH, gs->MagV, gs->PSM,
        (int)gs->ZBuffering, (int)gs->DoubleBuffering);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);

    Log("[RECHAN-PS2] boot: PS2 target (EE/GS), native resolution %dx%d\n", kWidth, kHeight);

    g_log = fopen("host:/rechan-ps2.log", "w");
    Log("[RECHAN-PS2] file log: %s\n", g_log ? "host:/rechan-ps2.log" : "unavailable");

    GSGLOBAL* gs = gsKit_init_global();
    if (gs == 0) {
        Log("[RECHAN-PS2] ERROR: gsKit_init_global failed\n");
        return 1;
    }
    ReportGlobal("defaults", gs);

    // Native NTSC resolution (the NTSC block documented in gsKit's basic.c).
    // Field = GS_FIELD: interlaced field mode, like real PS2 games. With
    // Interlace + GS_FRAME, gsKit ends up with MagV = -1 and a broken display.
    gs->Mode = GS_MODE_NTSC;
    gs->Interlace = GS_INTERLACED;
    gs->Field = GS_FIELD;
    gs->Width = kWidth;
    gs->Height = kHeight;
    gs->Aspect = GS_ASPECT_4_3;
    gs->PrimAlphaEnable = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    Log("[RECHAN-PS2] gsKit_init_screen...\n");
    gsKit_init_screen(gs);
    ReportGlobal("screen", gs);
    Log("[RECHAN-PS2] gsKit_init_screen ok\n");

    // Alpha 0x00 is opaque in gsKit colours (the clear itself already proved it).
    const u64 kBg = GS_SETREG_RGBAQ(0x10, 0x20, 0x60, 0x00, 0x00);
    const u64 kYellow = GS_SETREG_RGBAQ(0xf0, 0xc0, 0x20, 0x00, 0x00);
    const u64 kRed = GS_SETREG_RGBAQ(0xd0, 0x30, 0x20, 0x00, 0x00);
    const u64 kGreen = GS_SETREG_RGBAQ(0x20, 0xd0, 0x50, 0x00, 0x00);
    const u64 kWhite = GS_SETREG_RGBAQ(0xff, 0xff, 0xff, 0x00, 0x00);

    // Static content in the persistent queue (survives oneshot resets).
    gsKit_mode_switch(gs, GS_PERSISTENT);
    gsKit_clear(gs, kBg);
    gsKit_set_test(gs, GS_ZTEST_OFF);
    gsKit_prim_sprite(gs, 40.0f, 60.0f, 300.0f, 220.0f, 4, kYellow);
    gsKit_prim_sprite(gs, 340.0f, 240.0f, 600.0f, 400.0f, 4, kRed);
    gsKit_prim_sprite(gs, 40.0f, 380.0f, 300.0f, 420.0f, 4, kGreen);
    gsKit_mode_switch(gs, GS_ONESHOT);
    Log("[RECHAN-PS2] static pattern queued (persistent)\n");

    for (int frame = 0; frame < kFrames; ++frame) {
        // A moving bar proves the loop advances frame by frame.
        float x = (float)((frame * 3) % (kWidth + 80)) - 40.0f;
        gsKit_prim_sprite(gs, x, 200.0f, x + 60.0f, 260.0f, 5, kWhite);

        if (frame == 0) {
            Log("[RECHAN-PS2] frame 0: queue_exec...\n");
        }
        gsKit_queue_exec(gs);
        if (frame == 0) {
            Log("[RECHAN-PS2] frame 0: queue_exec ok, sync_flip...\n");
        }
        gsKit_sync_flip(gs);
        if (frame == 0) {
            Log("[RECHAN-PS2] frame 0: sync_flip ok\n");
        }

        if ((frame % 60) == 0) {
            Log("[RECHAN-PS2] frame %d ok\n", frame);
        }
    }

    Log("[RECHAN-PS2] done: %d frames drawn at %dx%d\n", kFrames, kWidth, kHeight);
    if (g_log != 0) {
        fclose(g_log);
        g_log = 0;
    }
    return 0;
}
