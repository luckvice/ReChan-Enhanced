#pragma once

#include "gen/common.h"

// Rain in phase 1 (Chinatown): drops, splashes, the level's lights reflected
// on the wet ground and puddles. Each layer switches on its own, so the rain
// benchmark (PSP_BENCH_RAIN) can measure what every one of them costs.
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

} // namespace PspRain
