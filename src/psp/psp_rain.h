#pragma once

#include "gen/common.h"

struct Mat4;

// Rain in phase 1 (Chinatown): drops and splashes on the streets, none
// indoors. The light reflections and puddle layers are reserved for the
// benchmark configurations and draw nothing yet. Each layer switches on its
// own, so the rain benchmark (PSP_BENCH_RAIN) measures what each one costs.
namespace PspRain {

enum Layer : u32 {
    LayerDrops = 1u << 0,
    LayerSplashes = 1u << 1,
    LayerLightReflections = 1u << 2,
    LayerPuddles = 1u << 3,
};

// Benchmark configurations C0..C4: each one adds a layer to the previous one.
static constexpr u32 kConfigLayers[] = {
    0u,
    LayerDrops,
    LayerDrops | LayerSplashes,
    LayerDrops | LayerSplashes | LayerLightReflections,
    LayerDrops | LayerSplashes | LayerLightReflections | LayerPuddles,
};
static constexpr u32 kConfigCount = static_cast<u32>(sizeof(kConfigLayers) / sizeof(kConfigLayers[0]));

void SetLayers(u32 layers);
u32 GetLayers();

// Draws the active layers once per frame, after the world's effects, with the
// world pass matrix. Only phase 1 (level ID 1) has rain.
void Draw(s32 levelId, const Mat4& passWorld);

} // namespace PspRain
