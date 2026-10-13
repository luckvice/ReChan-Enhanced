#include "psp/psp_rain.h"

#include "ai/player.h"
#include "gen/camera.h"
#include "gen/colsect.h"
#include "gen/display.h"
#include "gen/model.h"
#include "gen/shadows.h"
#include "gen/time.h"
#include "p3d/context.h"
#include "p3d/matrix.h"
#include "pddi/psp/psprender.h"
#include "psp/psp_perf.h"

#include <pspkernel.h>

#include "snd/rsevent.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <malloc.h>

namespace PspRain {

namespace {

// Every layer by default; the benchmark picks its own per scenario.
u32 s_layers = kConfigLayers[kConfigCount - 1];

// The rain lives in a block of 2x2 tiles of kTile around the point ahead of
// the camera, split into 8x8 columns of kCell. The vertex arrays hold the
// whole block, column after column, so a frame draws only the columns that
// are in view and not under a roof as a few contiguous runs: the CPU never
// touches a drop, only one matrix and the run list. The pattern repeats every
// tile, so moving the block by a tile is seamless. World units have Y up;
// Jackie is about 600 units tall.
constexpr s32 kTile = 2048;
constexpr s32 kCell = 512;
constexpr s32 kBlockCells = 8;
constexpr u32 kColumns = kBlockCells * kBlockCells;
constexpr s32 kTileCells = kTile / kCell;

// Drops: line streaks, two layers of kTile so the fall can wrap.
constexpr u32 kDropsPerCell = 20;
constexpr u32 kDropColumnVertices = kDropsPerCell * 2u * 2u;
constexpr f32 kDropLength = 90.0f;
// About 9 m/s at roughly 250 units per metre, slanted a little by the wind.
constexpr f32 kFallSpeed = 2300.0f;
constexpr f32 kWindX = 0.14f;
constexpr f32 kWindZ = 0.06f;

// Splashes: small three-line crowns on Jackie's floor (the streets of the
// phase are flat; on a higher pavement the depth test hides them). Each
// 30 Hz tick shows one of kSplashGroups groups, so every splash flashes once
// at its own place. Stored group after group, column after column.
constexpr u32 kSplashGroups = 8;
constexpr u32 kSplashLines = 3;
constexpr u32 kSplashColumnVertices = kSplashLines * 2u;
constexpr f32 kSplashSize = 14.0f;

// The block is centred this far ahead of the camera, along its view, and
// this high above the point the camera looks at.
constexpr f32 kAheadOfCamera = 1024.0f;
constexpr s32 kAboveTarget = 600;
// Columns farther than this, or outside a 90-degree horizontal view (wider
// than the game camera's), are not drawn.
constexpr f32 kMaxDistance = 3600.0f;
constexpr f32 kColumnRadius = 362.0f;  // half the cell diagonal

// Indoors: the level's SoundAmbiantSpace switches put Jackie in ambience
// space 0 on the streets of phase 1 and in another space inside buildings
// (the kitchen of petal 2 starts in one). Indoors there is no rain at all.
constexpr s32 kStreetAmbienceSpace = 0;

// Roofs: a column is covered when the collision has a ceiling above the
// floor level (Jackie's floor) at its centre. A few columns are asked per
// frame; until a column has an answer it stays dry.
constexpr u32 kCeilingQueriesPerFrame = 4;
constexpr s32 kCeilingProbeAboveFloor = 160;
constexpr s32 kMaxCeilingHeight = 6000;
constexpr s32 kRefloorThreshold = 384;
constexpr s32 kNoCeiling = 0x7FFFFFFF;
enum CoverState : u8 { CoverUnknown, CoverOpen, CoverRoofed };

// Puddles: about kPuddleChance of the 512-unit cells hold one, its place,
// size and outline taken from a hash of the cell, so it never moves. A puddle
// only lies on flat ground at Jackie's level (the collision floor at its
// centre within kPuddleLevelTolerance of his) and out in the open.
//
// Reflection: the nearest blocks' geometry is drawn a second time, mirrored
// in Jackie's floor, into a quarter-screen target, and the puddles show it:
// their UVs are their own screen position, since a point on the mirror plane
// projects to the same place in both views. The oblique projection's clip
// plane sits kReflectionClearance above the floor, so the floor itself (and
// anything below it) never enters the reflection.
constexpr u32 kPuddleChance = 100;        // percent of the cells
constexpr s32 kPuddleMinRadius = 140;
constexpr s32 kPuddleRadiusRange = 140;
// Centres stay this far inside the cell; a large puddle may spill over.
constexpr s32 kPuddleMargin = 100;
constexpr s32 kPuddleLevelTolerance = 48;
constexpr u32 kPuddleRim = 8;              // outline vertices
constexpr u32 kPuddleVertices = 1 + 2 * kPuddleRim;
constexpr u32 kPuddleIndices = kPuddleRim * 3 + kPuddleRim * 6;
// Inside, the reflection covers this much of the ground; the rim fades out.
constexpr u32 kPuddleCoreAlpha = 0x9C;
// The reflection seen in a puddle is a little darker than the scene.
constexpr u32 kPuddleTint = 0xC8;
constexpr f32 kReflectionClearance = 8.0f;
// Scales the pass's depth: heights up to 1/kReflectionDepthScale times their
// distance stay inside the depth range.
constexpr f32 kReflectionDepthScale = 0.5f;
// Only the nearest visible blocks are mirrored: they are what the puddles
// near the camera show.
constexpr u32 kReflectionBlocks = 6;

struct Puddle {
    s32 centreX, centreZ;
    s32 radius;
    u32 shapeSeed;
};

// The cell's puddle, if it has one.
bool CellPuddle(s32 cellX, s32 cellZ, Puddle* puddle) {
    u32 hash = static_cast<u32>(cellX) * 73856093u ^ static_cast<u32>(cellZ) * 19349663u ^ 0x50554444u;
    hash = (hash ^ (hash >> 13)) * 0x5bd1e995u;
    hash ^= hash >> 15;
    if (hash % 100u >= kPuddleChance) return false;
    const s32 span = kCell - 2 * kPuddleMargin;
    puddle->centreX = cellX * kCell + kPuddleMargin + static_cast<s32>((hash >> 8) % static_cast<u32>(span));
    puddle->centreZ = cellZ * kCell + kPuddleMargin + static_cast<s32>((hash >> 16) % static_cast<u32>(span));
    puddle->radius = kPuddleMinRadius + static_cast<s32>((hash >> 4) % static_cast<u32>(kPuddleRadiusRange));
    puddle->shapeSeed = hash;
    return true;
}

// Lightning: a strike every kStrikeMin..kStrikeMin + kStrikeRange seconds
// (the first one sooner) flashes the scene, under the HUD, in a cold white:
// a bright flicker, a short gap, a second flash and a fade. Only on the
// street; the effect's own random sequence picks the times.
constexpr f32 kStrikeMin = 6.0f;
constexpr f32 kStrikeRange = 10.0f;
constexpr f32 kFirstStrikeMin = 2.0f;
constexpr f32 kFirstStrikeRange = 4.0f;
constexpr f32 kFlashR = 0.55f, kFlashG = 0.60f, kFlashB = 0.72f;

// Flash strength `seconds` into a strike (0 once it is over).
f32 StrikeIntensity(f32 seconds) {
    if (seconds < 0.0f) return 0.0f;
    if (seconds < 0.06f) return 1.0f;
    if (seconds < 0.12f) return 0.12f;
    if (seconds < 0.22f) return 0.75f;
    const f32 fade = 1.0f - (seconds - 0.22f) / 0.5f;
    return fade > 0.0f ? 0.75f * fade * fade : 0.0f;
}

constexpr u32 Colour(f32 r, f32 g, f32 b) {
    return 0xFF000000u | (static_cast<u32>(b * 255.0f) << 16)
        | (static_cast<u32>(g * 255.0f) << 8) | static_cast<u32>(r * 255.0f);
}
// Additive: the streak head is black (fades into the scene), the tail a dim
// blue-grey, so the rain reads on the night street without washing it out.
constexpr u32 kDropTail = Colour(0.20f, 0.23f, 0.29f);
constexpr u32 kSplashColour = Colour(0.26f, 0.29f, 0.35f);

PspRainVertex* s_drops = nullptr;
PspRainVertex* s_splashes = nullptr;
f32 s_fall = 0.0f;
f32 s_splashClock = 0.0f;

u8 s_cover[kColumns] = {};
// Collision floor under the column's puddle (kNoPuddleFloor: none, or not
// asked yet); a puddle shows while it is at Jackie's level.
constexpr s32 kNoPuddleFloor = static_cast<s32>(0x80000001);
s32 s_puddleFloor[kColumns] = {};
// Puddles drawn last frame: the mirrored pass only runs when there were some.
u32 s_lastPuddles = 0;
s32 s_coverCellX = 0;
s32 s_coverCellZ = 0;
s32 s_coverFloor = 0;
bool s_coverValid = false;
u32 s_coverCursor = 0;
bool s_reflectionDrawn = false;
// Floor of the image in the reflection target, or kNoReflectionImage.
constexpr s32 kNoReflectionImage = static_cast<s32>(0x80000000);
s32 s_reflectionImageFloor = kNoReflectionImage;
u32 s_reflectionFrame = 0;
f32 s_nextStrike = 4.0f;   // seconds until the next strike
f32 s_strike = -1.0f;      // seconds into the current strike (< 0: none)
u32 s_lightningSeed = 0x544855u;

// The effect's own generator: the game's random sequences stay untouched.
u32 NextRandom(u32& state) {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

PspRainVertex Vertex(f32 x, f32 y, f32 z, u32 colour) {
    return { colour, static_cast<s16>(std::lround(x)), static_cast<s16>(std::lround(y)),
             static_cast<s16>(std::lround(z)), 0 };
}

void ReleaseArrays() {
    std::free(s_drops);
    std::free(s_splashes);
    s_drops = nullptr;
    s_splashes = nullptr;
    s_coverValid = false;
}

bool EnsureArrays() {
    if (s_drops && s_splashes) return true;
    ReleaseArrays();
    const std::size_t dropBytes = sizeof(PspRainVertex) * kColumns * kDropColumnVertices;
    const std::size_t splashBytes = sizeof(PspRainVertex) * kSplashGroups * kColumns * kSplashColumnVertices;
    s_drops = static_cast<PspRainVertex*>(memalign(16, dropBytes));
    s_splashes = static_cast<PspRainVertex*>(memalign(16, splashBytes));
    if (!s_drops || !s_splashes) {
        ReleaseArrays();
        return false;
    }

    // One pattern per cell of a tile, repeated in the four tiles of the
    // block (and in both drop layers), so the block can move by whole tiles.
    static constexpr f32 kCrown[kSplashLines][2] = { { 1.0f, 0.2f }, { -0.6f, 0.8f }, { -0.4f, -1.0f } };
    for (s32 cz = 0; cz < kBlockCells; ++cz) {
        for (s32 cx = 0; cx < kBlockCells; ++cx) {
            const u32 column = static_cast<u32>(cz * kBlockCells + cx);
            const u32 patternCell = static_cast<u32>((cz % kTileCells) * kTileCells + (cx % kTileCells));
            const f32 baseX = static_cast<f32>(cx * kCell);
            const f32 baseZ = static_cast<f32>(cz * kCell);

            u32 seed = 0x5241494Eu + patternCell * 7919u;  // "RAIN"
            PspRainVertex* drop = s_drops + column * kDropColumnVertices;
            for (u32 i = 0; i < kDropsPerCell; ++i) {
                const f32 x = baseX + static_cast<f32>(NextRandom(seed) % kCell);
                const f32 y = static_cast<f32>(NextRandom(seed) % kTile);
                const f32 z = baseZ + static_cast<f32>(NextRandom(seed) % kCell);
                for (s32 layer = 0; layer < 2; ++layer) {
                    const f32 ly = y + static_cast<f32>(layer * kTile);
                    // The streak points back up the way the drop came.
                    *drop++ = Vertex(x - kWindX * kDropLength, ly + kDropLength,
                                     z - kWindZ * kDropLength, 0xFF000000u);
                    *drop++ = Vertex(x, ly, z, kDropTail);
                }
            }

            u32 splashSeed = 0x53504C53u + patternCell * 104729u;  // "SPLS"
            for (u32 group = 0; group < kSplashGroups; ++group) {
                PspRainVertex* splash = s_splashes + (group * kColumns + column) * kSplashColumnVertices;
                const f32 x = baseX + static_cast<f32>(NextRandom(splashSeed) % kCell);
                const f32 z = baseZ + static_cast<f32>(NextRandom(splashSeed) % kCell);
                const f32 scale = 0.7f + static_cast<f32>(NextRandom(splashSeed) % 64) / 106.0f;
                for (u32 line = 0; line < kSplashLines; ++line) {
                    *splash++ = Vertex(x, 0.0f, z, kSplashColour);
                    *splash++ = Vertex(x + kCrown[line][0] * kSplashSize * scale, kSplashSize * scale,
                                       z + kCrown[line][1] * kSplashSize * scale, 0xFF000000u);
                }
            }
        }
    }
    // The GE reads these arrays straight from RAM.
    sceKernelDcacheWritebackRange(s_drops, dropBytes);
    sceKernelDcacheWritebackRange(s_splashes, splashBytes);
    return true;
}

// Jackie's floor: the last valid floor of his model (as his shadow uses),
// else his feet.
bool PlayerFloor(s32* outFloor) {
    const Player* player = Player::s_player;
    if (!player) return false;
    const ModelFloorHeightState* state = player->model
        ? GetModelFloorHeightState(static_cast<const Model*>(player->model)) : nullptr;
    constexpr s32 kNoFloor = static_cast<s32>(0x80000001);
    if (state && state->previous > kNoFloor) *outFloor = state->previous;
    else if (state && state->current > kNoFloor) *outFloor = state->current;
    else *outFloor = player->pos.y;
    return true;
}

s32 FloorDiv(s32 value, s32 divisor) {
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

// Keeps the cover answers of the columns the block shares with the previous
// frame; columns new to the block (or a new floor level) start unknown.
void MoveCoverWindow(s32 cellX, s32 cellZ, s32 floor) {
    if (!s_coverValid || std::abs(floor - s_coverFloor) > kRefloorThreshold) {
        std::fill(std::begin(s_cover), std::end(s_cover), static_cast<u8>(CoverUnknown));
        std::fill(std::begin(s_puddleFloor), std::end(s_puddleFloor), kNoPuddleFloor);
        s_coverCellX = cellX;
        s_coverCellZ = cellZ;
        s_coverFloor = floor;
        s_coverValid = true;
        return;
    }
    if (cellX == s_coverCellX && cellZ == s_coverCellZ) return;
    u8 moved[kColumns];
    s32 movedFloor[kColumns];
    for (s32 cz = 0; cz < kBlockCells; ++cz) {
        for (s32 cx = 0; cx < kBlockCells; ++cx) {
            const s32 oldX = cellX + cx - s_coverCellX;
            const s32 oldZ = cellZ + cz - s_coverCellZ;
            const bool shared = oldX >= 0 && oldX < kBlockCells && oldZ >= 0 && oldZ < kBlockCells;
            moved[cz * kBlockCells + cx] = shared ? s_cover[oldZ * kBlockCells + oldX]
                                                  : static_cast<u8>(CoverUnknown);
            movedFloor[cz * kBlockCells + cx] = shared ? s_puddleFloor[oldZ * kBlockCells + oldX]
                                                       : kNoPuddleFloor;
        }
    }
    std::copy(std::begin(moved), std::end(moved), std::begin(s_cover));
    std::copy(std::begin(movedFloor), std::end(movedFloor), std::begin(s_puddleFloor));
    s_coverCellX = cellX;
    s_coverCellZ = cellZ;
}

// Asks the collision about a column: true when a ceiling covers it. The
// probe stands on the cell's puddle when it has one, and *puddleFloor gets
// the floor there (kNoPuddleFloor without a puddle).
bool ColumnRoofed(s32 cellX, s32 cellZ, s32 floor, s32* puddleFloor) {
    LVector probe = {};
    probe.x = cellX * kCell + kCell / 2;
    probe.y = floor + kCeilingProbeAboveFloor;
    probe.z = cellZ * kCell + kCell / 2;
    Puddle puddle = {};
    const bool hasPuddle = CellPuddle(cellX, cellZ, &puddle);
    if (hasPuddle) {
        probe.x = puddle.centreX;
        probe.z = puddle.centreZ;
    }
    s32 floorHeight = 0;
    s32 ceilingHeight = kNoCeiling;
    LVector floorNormal = {};
    LVector ceilingNormal = {};
    CollisionSector::GetWorldFloorAndCeilingHeight(floorHeight, ceilingHeight,
        floorNormal, ceilingNormal, probe, 64);
    *puddleFloor = hasPuddle ? floorHeight : kNoPuddleFloor;
    return ceilingHeight != kNoCeiling && ceilingHeight > probe.y
        && ceilingHeight - probe.y < kMaxCeilingHeight;
}

// Asks the collision about a few columns: unknown visible ones first (a
// frame or two decides them all), then one known column per frame is
// re-checked, so the map follows the level as its blocks stream in.
void UpdateCover(const bool* visible) {
    u32 queries = 0;
    for (u32 i = 0; i < kColumns && queries < kCeilingQueriesPerFrame; ++i) {
        if (s_cover[i] != CoverUnknown || !visible[i]) continue;
        const s32 cellX = s_coverCellX + static_cast<s32>(i % kBlockCells);
        const s32 cellZ = s_coverCellZ + static_cast<s32>(i / kBlockCells);
        s_cover[i] = ColumnRoofed(cellX, cellZ, s_coverFloor, &s_puddleFloor[i]) ? CoverRoofed : CoverOpen;
        ++queries;
    }
    if (queries < kCeilingQueriesPerFrame) {
        for (u32 i = 0; i < kColumns; ++i) {
            const u32 column = (s_coverCursor + i) % kColumns;
            if (s_cover[column] == CoverUnknown) continue;
            const s32 cellX = s_coverCellX + static_cast<s32>(column % kBlockCells);
            const s32 cellZ = s_coverCellZ + static_cast<s32>(column / kBlockCells);
            s_cover[column] = ColumnRoofed(cellX, cellZ, s_coverFloor, &s_puddleFloor[column])
                ? CoverRoofed : CoverOpen;
            s_coverCursor = (column + 1u) % kColumns;
            ++queries;
            break;
        }
    }
    PspPerf::CountRenderer(PspPerf::RendererCounter::RainCeilingQueries, queries);
}

// Turns the per-column flags into contiguous runs of `stride` vertices.
u32 BuildRuns(const bool* draw, u32 firstVertex, u32 stride, u32 (*runs)[2]) {
    u32 count = 0;
    for (u32 column = 0; column < kColumns; ++column) {
        if (!draw[column]) continue;
        const u32 start = firstVertex + column * stride;
        if (count > 0 && runs[count - 1][0] + runs[count - 1][1] == start) {
            runs[count - 1][1] += stride;
        }
        else {
            runs[count][0] = start;
            runs[count][1] = stride;
            ++count;
        }
    }
    return count;
}

Mat4 BlockWorld(const Mat4& passWorld, f32 x, f32 y, f32 z) {
    // The GE reads 16-bit positions as value / 32768.
    Mat4 local;
    local.m[0] = local.m[5] = local.m[10] = 32768.0f;
    local.SetTranslation(x, y, z);
    return passWorld * local;
}

// The puddles of the open columns, textured with this frame's reflection;
// without an image (draw false) they are only counted, which decides whether
// the next frame renders one.
void DrawPuddles(const bool* open, s32 originX, s32 originZ, s32 floor, const Mat4& passWorld,
                 bool draw) {
    static PspReflectVertex vertices[kColumns * kPuddleVertices];
    static u16 indices[kColumns * kPuddleIndices];
    const Mat4 viewProjection = p3d::context->GetProjectionMatrix()
        * p3d::context->GetViewMatrix() * passWorld;
    const f32* m = viewProjection.m;
    const f32 floorY = static_cast<f32>(floor);
    u32 vertexCount = 0;
    u32 indexCount = 0;
    const s32 originCellX = originX / kCell;
    const s32 originCellZ = originZ / kCell;
    for (u32 column = 0; column < kColumns; ++column) {
        if (!open[column] || s_puddleFloor[column] == kNoPuddleFloor
            || std::abs(s_puddleFloor[column] - floor) > kPuddleLevelTolerance) {
            continue;
        }
        Puddle puddle = {};
        if (!CellPuddle(originCellX + static_cast<s32>(column % kBlockCells),
                        originCellZ + static_cast<s32>(column / kBlockCells), &puddle)) {
            continue;
        }
        ++s_lastPuddles;
        if (!draw) continue;
        // Centre, then the core ring, then the rim, around an uneven outline.
        f32 ringX[kPuddleRim];
        f32 ringZ[kPuddleRim];
        u32 seed = puddle.shapeSeed;
        for (u32 k = 0; k < kPuddleRim; ++k) {
            const f32 angle = 6.2831853f * (static_cast<f32>(k) + 0.3f * static_cast<f32>(NextRandom(seed) % 100) / 100.0f)
                / static_cast<f32>(kPuddleRim);
            const f32 reach = static_cast<f32>(puddle.radius) * (0.7f + static_cast<f32>(NextRandom(seed) % 45) / 100.0f);
            ringX[k] = std::cos(angle) * reach;
            ringZ[k] = std::sin(angle) * 0.75f * reach;
        }
        const u32 base = vertexCount;
        bool usable = true;
        for (u32 v = 0; v < kPuddleVertices && usable; ++v) {
            f32 dx = 0.0f, dz = 0.0f;
            u32 alpha = kPuddleCoreAlpha;
            if (v >= 1 && v <= kPuddleRim) {
                dx = ringX[v - 1] * 0.6f;
                dz = ringZ[v - 1] * 0.6f;
            }
            else if (v > kPuddleRim) {
                dx = ringX[v - 1 - kPuddleRim];
                dz = ringZ[v - 1 - kPuddleRim];
                alpha = 0;
            }
            const f32 x = static_cast<f32>(puddle.centreX) + dx;
            const f32 z = static_cast<f32>(puddle.centreZ) + dz;
            // On the puddle's own floor, a little off Jackie's at most.
            const f32 y = static_cast<f32>(s_puddleFloor[column]);
            const f32 cx = m[0] * x + m[4] * y + m[8] * z + m[12];
            const f32 cy = m[1] * x + m[5] * y + m[9] * z + m[13];
            const f32 cw = m[3] * x + m[7] * y + m[11] * z + m[15];
            if (cw <= 1.0f) { usable = false; break; }
            PspReflectVertex& vertex = vertices[vertexCount++];
            // Screen position in the half-size target of a 256x256 texture.
            vertex.u = (120.0f + 120.0f * cx / cw) / 256.0f;
            vertex.v = (68.0f - 68.0f * cy / cw) / 256.0f;
            vertex.color = (alpha << 24) | (kPuddleTint << 16) | (kPuddleTint << 8) | kPuddleTint;
            vertex.x = static_cast<s16>(static_cast<s32>(x) - originX);
            vertex.y = static_cast<s16>(s_puddleFloor[column] - floor);
            vertex.z = static_cast<s16>(static_cast<s32>(z) - originZ);
            vertex.pad = 0;
        }
        if (!usable) {
            vertexCount = base;
            continue;
        }
        for (u32 k = 0; k < kPuddleRim; ++k) {
            const u16 next = static_cast<u16>((k + 1) % kPuddleRim);
            const u16 core = static_cast<u16>(base + 1 + k);
            const u16 coreNext = static_cast<u16>(base + 1 + next);
            const u16 rim = static_cast<u16>(base + 1 + kPuddleRim + k);
            const u16 rimNext = static_cast<u16>(base + 1 + kPuddleRim + next);
            const u16 tri[9] = { static_cast<u16>(base), core, coreNext,
                                 core, rim, coreNext, coreNext, rim, rimNext };
            std::copy(tri, tri + 9, indices + indexCount);
            indexCount += 9;
        }
    }
    PspPerf::CountRenderer(PspPerf::RendererCounter::RainPuddles, indexCount / kPuddleIndices);
    if (indexCount == 0) return;
    DrawPspReflectionPatches(BlockWorld(passWorld, static_cast<f32>(originX), floorY + 1.0f,
                                        static_cast<f32>(originZ)),
                             vertices, vertexCount, indices, indexCount);
}

void UpdateLightning(f32 dt) {
    if (s_strike >= 0.0f) {
        s_strike += dt;
        if (s_strike > 0.22f && StrikeIntensity(s_strike) <= 0.0f) s_strike = -1.0f;
        return;
    }
    s_nextStrike -= dt;
    if (s_nextStrike > 0.0f) return;
    s_strike = 0.0f;
    s_nextStrike = kStrikeMin
        + kStrikeRange * static_cast<f32>(NextRandom(s_lightningSeed) % 1000u) / 1000.0f;
}

void DrawLightning() {
    if (!(s_layers & LayerLightning)) return;
    const f32 intensity = StrikeIntensity(s_strike);
    if (intensity <= 0.0f) return;
    DrawPspScreenFlash(Colour(kFlashR * intensity, kFlashG * intensity, kFlashB * intensity));
}

} // namespace

bool BeginReflection(s32 levelId) {
    s_reflectionDrawn = false;
    if (levelId != 1 || !(s_layers & LayerPuddles) || !p3d::context) {
        s_reflectionImageFloor = kNoReflectionImage;
        return false;
    }
    if (rsGetAmbienceSpace() != kStreetAmbienceSpace) {
        s_reflectionImageFloor = kNoReflectionImage;
        return false;
    }
    s32 floor = 0;
    if (!PlayerFloor(&floor)) return false;
    // An image is only shown the frame it is drawn. No puddle in view last
    // frame: skip the pass (a puddle coming into view shows from the next).
    s_reflectionImageFloor = kNoReflectionImage;
    if (s_lastPuddles == 0) return false;
#if defined(PSP_RAIN_REFLECTION_30HZ)
    // The mirrored pass every other frame; the frame between shows the last
    // image. Measured slower: the page and CLUT caches thrash between the
    // two views (palette uploads doubled).
    s_reflectionFrame ^= 1u;
    if (s_reflectionFrame != 0 && s_reflectionImageFloor == floor) return false;
#endif
    const Mat4& view = p3d::context->GetViewMatrix();
    const Mat4& projection = p3d::context->GetProjectionMatrix();

    // Mirror in the floor plane y = floor: y' = 2 * floor - y.
    Mat4 mirror;
    mirror.m[5] = -1.0f;
    mirror.m[13] = 2.0f * static_cast<f32>(floor);
    const Mat4 mirroredView = view * mirror;

    // Keep the mirror images of what stands above the floor: in the real
    // camera's eye space they lie beyond the plane y = floor - clearance,
    // whose normal is the view's image of world -Y. (A point on the plane has
    // the same eye coordinates in both views.)
    const f32 nx = -view.m[4];
    const f32 ny = -view.m[5];
    const f32 nz = -view.m[6];
    const f32 nw = view.m[4] * view.m[12] + view.m[5] * view.m[13] + view.m[6] * view.m[14]
        + static_cast<f32>(floor) - kReflectionClearance;
    // The PSP GE drops what falls behind z = 0 in clip space (measured on a
    // PSP-3000: a z = -w clip plane kept nothing). With the depth row a * C
    // the clip plane is the floor, and depth grows with distance.
    Mat4 oblique = projection;
    oblique.m[2] = kReflectionDepthScale * nx;
    oblique.m[6] = kReflectionDepthScale * ny;
    oblique.m[10] = kReflectionDepthScale * nz;
    oblique.m[14] = kReflectionDepthScale * nw;
    s_reflectionDrawn = BeginPspReflectionPass(mirroredView, oblique);
    if (s_reflectionDrawn) s_reflectionImageFloor = floor;
    return s_reflectionDrawn;
}

void EndReflection() {
    if (s_reflectionDrawn) EndPspReflectionPass();
}

u32 ReflectionBlockLimit() {
    return kReflectionBlocks;
}

void SetLayers(u32 layers) {
    s_layers = layers;
    // A new start (a benchmark scenario, the option) restarts the rain, so
    // the same simulated frame always shows the same drops.
    s_fall = 0.0f;
    s_splashClock = 0.0f;
    s_coverValid = false;
    s_reflectionImageFloor = kNoReflectionImage;
    s_reflectionFrame = 0;
    s_lastPuddles = 0;
    s_lightningSeed = 0x544855u;  // "THU"
    s_nextStrike = kFirstStrikeMin
        + kFirstStrikeRange * static_cast<f32>(NextRandom(s_lightningSeed) % 1000u) / 1000.0f;
    s_strike = -1.0f;
}

u32 GetLayers() {
    return s_layers;
}

void Draw(s32 levelId, const Mat4& passWorld) {
    s_lastPuddles = 0;
    if (levelId != 1 || s_layers == 0) {
        // Outside phase 1 the arrays are not needed.
        if (s_drops || s_splashes) ReleaseArrays();
        return;
    }
    if (!p3d::context || !(s_layers & (LayerDrops | LayerSplashes | LayerPuddles | LayerLightning))) {
        return;
    }
    PspPerf::ScopeTimer timer(PspPerf::RendererCounter::RainUs);
    Camera* camera = g_display ? g_display->GetCamera() : nullptr;
    s32 floor = 0;
    if (!camera || !PlayerFloor(&floor) || !EnsureArrays()) return;

    const f32 dt = g_time ? g_time->GetDeltaTime() : (1.0f / 60.0f);
    s_fall = std::fmod(s_fall + dt * kFallSpeed, static_cast<f32>(kTile));
    s_splashClock = std::fmod(s_splashClock + dt, 1024.0f);
    if (rsGetAmbienceSpace() != kStreetAmbienceSpace) return;
    if (s_layers & LayerLightning) UpdateLightning(dt);

    const LVector& eye = camera->GetPosition();
    const LVector& target = camera->GetTargetPos();
    f32 forwardX = static_cast<f32>(target.x - eye.x);
    f32 forwardZ = static_cast<f32>(target.z - eye.z);
    const f32 forwardLength = std::sqrt(forwardX * forwardX + forwardZ * forwardZ);
    if (forwardLength > 1.0f) {
        forwardX /= forwardLength;
        forwardZ /= forwardLength;
    }
    const f32 centreX = static_cast<f32>(eye.x) + forwardX * kAheadOfCamera;
    const f32 centreZ = static_cast<f32>(eye.z) + forwardZ * kAheadOfCamera;
    const f32 centreY = static_cast<f32>(target.y + kAboveTarget);

    // The 2x2 tiles cover at least half a tile around the centre.
    const s32 originX = FloorDiv(static_cast<s32>(centreX) - kTile / 2, kTile) * kTile;
    const s32 originZ = FloorDiv(static_cast<s32>(centreZ) - kTile / 2, kTile) * kTile;
    MoveCoverWindow(originX / kCell, originZ / kCell, floor);

    bool visible[kColumns];
    for (u32 column = 0; column < kColumns; ++column) {
        const f32 x = static_cast<f32>(originX + static_cast<s32>(column % kBlockCells) * kCell + kCell / 2);
        const f32 z = static_cast<f32>(originZ + static_cast<s32>(column / kBlockCells) * kCell + kCell / 2);
        const f32 dx = x - static_cast<f32>(eye.x);
        const f32 dz = z - static_cast<f32>(eye.z);
        const f32 ahead = dx * forwardX + dz * forwardZ;
        const f32 side = std::fabs(dx * forwardZ - dz * forwardX);
        visible[column] = ahead > -kColumnRadius && side <= ahead + kColumnRadius * 1.5f
            && dx * dx + dz * dz < kMaxDistance * kMaxDistance;
    }
    UpdateCover(visible);
    bool open[kColumns];
    u32 drawn = 0;
    u32 roofed = 0;
    for (u32 column = 0; column < kColumns; ++column) {
        open[column] = visible[column] && s_cover[column] == CoverOpen;
        drawn += open[column] ? 1u : 0u;
        roofed += (visible[column] && s_cover[column] == CoverRoofed) ? 1u : 0u;
    }
    PspPerf::CountRenderer(PspPerf::RendererCounter::RainColumns, drawn);
    PspPerf::CountRenderer(PspPerf::RendererCounter::RainRoofedColumns, roofed);
    if (drawn == 0) {
        DrawLightning();
        return;
    }

    // The wet floor first: the splashes and drops are drawn over it.
    if (s_layers & LayerPuddles) {
        DrawPuddles(open, originX, originZ, floor, passWorld, s_reflectionImageFloor == floor);
#if defined(PSP_RAIN_REFLECTION_DEBUG)
        DrawPspReflectionDebug();
#endif
    }
    u32 runs[kColumns][2];
    // Splashes on the floor: the 30 Hz tick picks the group.
    if (s_layers & LayerSplashes) {
        const u32 group = static_cast<u32>(s_splashClock * 30.0f) % kSplashGroups;
        const u32 count = BuildRuns(open, group * kColumns * kSplashColumnVertices,
                                    kSplashColumnVertices, runs);
        DrawPspRainLines(BlockWorld(passWorld, static_cast<f32>(originX),
                                    static_cast<f32>(floor + 2), static_cast<f32>(originZ)),
                         s_splashes, runs, count);
    }
    if (s_layers & LayerDrops) {
        // The fall shifts the two layers down; the upper one refills the top.
        const s32 originY = FloorDiv(static_cast<s32>(centreY + s_fall) - kTile / 2, kTile) * kTile;
        const u32 count = BuildRuns(open, 0, kDropColumnVertices, runs);
        DrawPspRainLines(BlockWorld(passWorld, static_cast<f32>(originX),
                                    static_cast<f32>(originY) - s_fall, static_cast<f32>(originZ)),
                         s_drops, runs, count);
    }
    DrawLightning();
}

} // namespace PspRain
