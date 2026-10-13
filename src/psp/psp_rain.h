#pragma once

#include "gen/common.h"

struct Mat4;

// Rain in phase 1 (Chinatown): drops and splashes on the streets (none
// indoors), puddles that mirror the scene, and lightning. Each layer switches on its
// own, so the rain benchmark (PSP_BENCH_RAIN) measures what each one costs.
namespace PspRain {

enum Layer : u32 {
    LayerDrops = 1u << 0,
    LayerSplashes = 1u << 1,
    // Puddles fixed in the world, showing the scene mirrored in the floor.
    LayerPuddles = 1u << 2,
    // Lightning: now and then the sky flashes over the street.
    LayerLightning = 1u << 3,
};

// Benchmark configurations C0..C4: each one adds a layer to the previous one.
static constexpr u32 kConfigLayers[] = {
    0u,
    LayerDrops,
    LayerDrops | LayerSplashes,
    LayerDrops | LayerSplashes | LayerPuddles,
    LayerDrops | LayerSplashes | LayerPuddles | LayerLightning,
};
static constexpr u32 kConfigCount = static_cast<u32>(sizeof(kConfigLayers) / sizeof(kConfigLayers[0]));

void SetLayers(u32 layers);
u32 GetLayers();

// Draws the active layers once per frame, after the world's effects, with the
// world pass matrix. Only phase 1 (level ID 1) has rain.
void Draw(s32 levelId, const Mat4& passWorld);

// Reflection layer: when BeginReflection returns true, the caller draws the
// nearest visible blocks' geometry (and nothing else), then calls
// EndReflection; Draw lays the mirrored image on the wet floor. The pass
// runs every other frame.
bool BeginReflection(s32 levelId);
void EndReflection();
// How many of the nearest visible blocks the reflection pass draws.
u32 ReflectionBlockLimit();

} // namespace PspRain
