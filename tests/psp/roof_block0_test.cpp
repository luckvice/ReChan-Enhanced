#include "core.h"
#include "p3d/lvector.h"
#include "p3d/p3dmath.h"
#include "p3d/context.h"
#include "gen/colvol.h"
#include "gen/database.h"
#define private public
#include "gen/blockmgr.h"
#include "gen/world.h"
#include "gen/game.h"
#undef private
#include "gen/camera.h"
#include "gen/colsect.h"
#include "gen/geometry.h"
#include "ai/thing.h"
#include "ai/obstacle.h"
#include "ai/humanoid.h"
#include "ai/player.h"
#include "ai/teleporter.h"
#include "gen/ai.h"
#include "pc/log.h"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>

namespace p3d {
pddiRenderContext* context = nullptr;
}

namespace PspPerf {
void Trace(const char*) {}
}

Log::Log() {}
Log::~Log() {}
Log& Log::Get() {
    alignas(Log) static u8 s_buf[sizeof(Log)] = {};
    return *reinterpret_cast<Log*>(s_buf);
}
void Log::LogMessage(const char*, ...) {}

void Camera::SetLookAtTarget(Thing*, u16) {}


void AI::KillThings(int) {}
void AI::PopulateBlock() {
    printf("[AI::PopulateBlock] called!\n");
}
void AI::UnpopulateBlock() {
    printf("[AI::UnpopulateBlock] called!\n");
}

class Display;



void WEffect_PopulateWEffects() {}
int RP_ZCullGClip(tGeometry*, const LVector*) { return 0; }
int RP_ZCullGMFog(tGeometry*, const LVector*, u16, u16, u32) { return 0; }
void UpdateUVPrimData(int, tPrimGeom*) {}
void UpdateCBVPrimData(int, tPrimGeom*) {}
void InvalidatePspPrimGeomCache(tPrimGeom*) {}
bool UpdatePspPrimGeomDynamicData(tPrimGeom*, bool, bool) { return true; }

const u8* tGeometry::GetVertexList() const { return vertexList; }
void tGeometry::SetVertexList(const u8* value) { vertexList = value; }
tPrimGeom::~tPrimGeom() { delete[] ownedRawData; }

CollisionSector::CollisionSector() { status = -1; }
void CollisionSector::Load(unsigned int* data) {
    if (!data) return;
    boundsMin.x = (s32)data[3];
    boundsMin.y = (s32)data[4];
    boundsMin.z = (s32)data[5];
    boundsMax.x = (s32)data[6];
    boundsMax.y = (s32)data[7];
    boundsMax.z = (s32)data[8];
}
void CollisionSector::Unload() { status = -1; }
CollisionSector g_collisionSectors[MAX_COLLISION_SECTORS];

Player* Player::s_player = nullptr;
BlockManager* g_blockManager = nullptr;
Game* g_game = nullptr;
AI* g_ai = nullptr;
Display* g_display = nullptr;

static World* s_mockWorld = nullptr;
World* Game::GetWorld() const { return s_mockWorld; }

Obstacle::Obstacle(const LVector* pos, u16 type) : Thing(pos, type) {}
Obstacle::~Obstacle() {}
void Obstacle::SetCollisionBox(const tagCollisionBox& box) { collBox = box; }
void Obstacle::HandleHumanoidCollision(Humanoid*) {}
void Obstacle::AnalyzeMesh(DBRoot*) {}
void Obstacle::UpdatePosition() {}
void Obstacle::ExplosiveTrigger(int, char const*) {}
void Obstacle::HandlePickupCollision(Thing*) {}
void Obstacle::Draw() {}
void Obstacle::Move() {}
void Obstacle::Reset() {}
void Obstacle::Think() {}
void Obstacle::Trigger() {}
void Obstacle::FillSphere(tSphere&) const {}
bool Obstacle::CareAboutAttack() const { return false; }
const LVector* Obstacle::GetDeltaVelocity() const { static LVector v{}; return &v; }
s32 Obstacle::GetFloorMaterial() const { return 0; }
s32 Obstacle::GetObstacleFloorHeight(LVector const&) const { return 0; }
void Obstacle::CreateModel(char const*) {}
void Obstacle::DeleteModel() {}
void Obstacle::HandleAttack(Humanoid*, int, int) {}
void Obstacle::HandleAttack(Humanoid*, int, int, int) {}
void Obstacle::TriggerByName(Thing*, char const*, char const*) {}

Thing::Thing(const LVector* initialPos, u16 type) {
    if (initialPos) pos = *initialPos;
    thingType = type;
    flags = TF_NEEDS_ACTIVATION;
}
Thing::~Thing() {}
void Thing::ClearFloorHeight() {}
void Thing::Deactivate() {}
void Thing::Activate() {
    if (g_blockManager && (blockNum == BLOCK_UNASSIGNED || !g_blockManager->IsValidBlockNumber(blockNum))) {
        u16 resolved = g_blockManager->GetBlockNumber(pos);
        if (resolved == BLOCK_UNASSIGNED) {
            resolved = g_blockManager->GetBlockNumberAllBlocks(pos);
        }
        if (resolved != BLOCK_UNASSIGNED) {
            blockNum = resolved;
        }
    }

    bool valid = false;
    if (g_blockManager) {
        if (g_blockManager->InActiveList(blockNum)) {
            if (g_blockManager->IsValidBlockNumber(blockNum)) {
                valid = true;
            }
        }
    }
    printf("[Thing::Activate] blockNum=%u InActiveList=%d IsValidBlockNumber=%d => valid=%d\n",
           blockNum,
           g_blockManager ? g_blockManager->InActiveList(blockNum) : 0,
           g_blockManager ? g_blockManager->IsValidBlockNumber(blockNum) : 0,
           valid);
    if (valid) {
        flags |= TF_ACTIVATED;
        CreateModel(nullptr);
    }
}
void Thing::AnalyzeMesh(DBRoot*) {}
void Thing::CreateModel(char const* name) {
    if (name) {
        modelHash = p3dHash(name);
    }
    if (modelHash) {
        flags |= TF_MODEL_CREATED;
    }
}
void Thing::DeleteModel() {}
void Thing::GetViewSpot(LVector*, LVector*) {}
const LVector* Thing::GetInitialPos() { return &pos; }
LVector* Thing::GetSoundPosPtr() { return &pos; }
void Thing::UpdatePosition() {}
void Thing::HandleCollision(Thing*, s32, ...) {}
void Thing::OnVisualModelDemoted() {}
void Thing::OnVisualModelPromoted() {}
void Thing::Draw() {}
void Thing::Kill() {}
void Thing::Move() {}
void Thing::Reset() {}
void Thing::Think() {}

void Humanoid::DeleteRightHandObj() {}
void Humanoid::DeleteLeftHandObj() {}
void Humanoid::FaceAngleY(s32, s32) {}
void Humanoid::SetActionState(u32 action, s32) { actionState = action; }
void Humanoid::PrepareLedgeLatch(const LVector&, const LVector&) {}
bool Humanoid::FacePointDesired(const LVector&) { return true; }
void Humanoid::FacePoint(const LVector&, s32) {}

static bool MySameBlockSet(const u16* a, u32 ac, const u16* b, u32 bc) {
    if (ac != bc) return false;
    for (u32 i = 0; i < ac; i++) {
        bool found = false;
        for (u32 j = 0; j < bc; j++) { if (a[i] == b[j]) { found = true; break; } }
        if (!found) return false;
    }
    return true;
}

int main() {
    printf("=== Testing Rooftops Block 0 Demand Loading & Activation ===\n");

    // 1. Read lev04.lcf WDB 58
    FILE* f = fopen("bin/rtarget/lev04.lcf", "rb");
    if (!f) {
        fprintf(stderr, "Cannot open bin/rtarget/lev04.lcf\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<u8> lcfData(fsize);
    fread(lcfData.data(), 1, fsize, f);
    fclose(f);

    u32 count = (lcfData[0] << 24) | (lcfData[1] << 16) | (lcfData[2] << 8) | lcfData[3];
    u32 pos = 4;
    struct Entry { char magic[5]; u32 size, offset; };
    std::vector<Entry> entries;
    for (u32 i = 0; i < count; i++) {
        Entry e = {};
        memcpy(e.magic, &lcfData[pos], 4);
        e.size = (lcfData[pos+4] << 24) | (lcfData[pos+5] << 16) | (lcfData[pos+6] << 8) | lcfData[pos+7];
        e.offset = (lcfData[pos+8] << 24) | (lcfData[pos+9] << 16) | (lcfData[pos+10] << 8) | lcfData[pos+11];
        u32 extraLen = (lcfData[pos+12] << 24) | (lcfData[pos+13] << 16) | (lcfData[pos+14] << 8) | lcfData[pos+15];
        pos += 16;
        if (extraLen > 0) pos += (extraLen + 3) & ~3;
        entries.push_back(e);
    }

    Database db;
    g_database = &db;
    db.Scan(&lcfData[entries[58].offset], entries[58].size);

    std::vector<DBVolume*> blockVolumes;
    for (DBRoot* v = db.GetFirstBlock(); v; v = static_cast<DBRoot*>(v->next)) {
        blockVolumes.push_back(static_cast<DBVolume*>(v));
    }
    printf("Loaded %zu block volumes from WDB 58\n", blockVolumes.size());

    printf("\n=== Scanning all Points in WDB 58 for Block 0 / Targets ===\n");
    for (DBPoint* pt = db.GetFirstPoint(); pt; pt = static_cast<DBPoint*>(pt->next)) {
        u16 blkNum = 9999;
        const DBAttrib* a15 = pt->FindAttrib(15);
        if (a15) blkNum = (u16)a15->value;
        const char* name = pt->GetName() ? pt->GetName() : "null";
        const DBAttrib* a5 = pt->FindAttrib(5);
        const char* modelName = a5 ? (a5->strValue ? a5->strValue : "val") : "none";
        if (pt->type == 6 && (u16)(pt->subType - 1) < 28u) {
            printf("HUMANOID POINT: name='%s' subType=%u blkAttr15=%u modelAttr5='%s'\n",
                   name, pt->subType, blkNum, modelName);
        }
        if (blkNum == 0 || strstr(name, "Lenny") || strstr(name, "stool") || strstr(name, "club") || strstr(name, "Tabl") || strstr(name, "drum") || strstr(name, "sldrs") || strstr(name, "tele")) {
            printf("POINT: name='%s' type=%u subType=%u pos=(%d,%d,%d) blkAttr15=%u modelAttr5='%s'\n",
                   name, pt->type, pt->subType, pt->pos.x, pt->pos.y, pt->pos.z, blkNum, modelName);
            for (u32 a = 0; a < pt->attribCount; a++) {
                const DBAttrib* at = pt->GetAttribByIndex(a);
                if (at) printf("   attr id=%u type=%u val=%u (0x%X) str='%s'\n", at->id, at->type, at->value, at->value, at->strValue ? at->strValue : "");
            }
        }
    }

    printf("\n=== Scanning all Meshes in WDB 58 for Block 0 / Targets ===\n");
    for (DBMesh* m = db.GetFirstMesh(); m; m = static_cast<DBMesh*>(m->next)) {
        u16 blkNum = 9999;
        const DBAttrib* a15 = m->FindAttrib(15);
        if (a15) blkNum = (u16)a15->value;
        const char* name = m->GetName() ? m->GetName() : "null";
        if (blkNum == 0 || strstr(name, "Lenny") || strstr(name, "stool") || strstr(name, "club") || strstr(name, "Tabl") || strstr(name, "drum") || strstr(name, "sldrs") || strstr(name, "tele")) {
            printf("MESH: name='%s' type=%u subType=%u pos=(%d,%d,%d) blkAttr15=%u fileName='%s'\n",
                   name, m->type, m->subType, m->pos.x, m->pos.y, m->pos.z, blkNum, m->fileName ? m->fileName : "null");
            for (u32 a = 0; a < m->attribCount; a++) {
                const DBAttrib* at = m->GetAttribByIndex(a);
                if (at) printf("   attr id=%u type=%u val=%u (0x%X) str='%s'\n", at->id, at->type, at->value, at->value, at->strValue ? at->strValue : "");
            }
        }
    }

    printf("\n=== Scanning all Entities in WDB 58 for Block 1 ===\n");
    for (DBPoint* pt = db.GetFirstPoint(); pt; pt = static_cast<DBPoint*>(pt->next)) {
        const DBAttrib* a15 = pt->FindAttrib(15);
        if (a15 && a15->value == 1) {
            printf("POINT (blk 1): name='%s' type=%u subType=%u pos=(%d,%d,%d)\n",
                   pt->GetName() ? pt->GetName() : "null", pt->type, pt->subType, pt->pos.x, pt->pos.y, pt->pos.z);
        }
    }
    for (DBMesh* m = db.GetFirstMesh(); m; m = static_cast<DBMesh*>(m->next)) {
        const DBAttrib* a15 = m->FindAttrib(15);
        if (a15 && a15->value == 1) {
            printf("MESH (blk 1): name='%s' type=%u subType=%u pos=(%d,%d,%d) fileName='%s'\n",
                   m->GetName() ? m->GetName() : "null", m->type, m->subType, m->pos.x, m->pos.y, m->pos.z, m->fileName ? m->fileName : "null");
        }
    }
    for (DBVolume* vol = db.GetFirstVolume(); vol; vol = static_cast<DBVolume*>(vol->next)) {
        const DBAttrib* a15 = vol->FindAttrib(15);
        if (a15 && a15->value == 1) {
            printf("VOL (blk 1): name='%s' type=%u subType=%u pos=(%d,%d,%d)\n",
                   vol->GetName() ? vol->GetName() : "null", vol->type, vol->subType, vol->pos.x, vol->pos.y, vol->pos.z);
            for (u32 a = 0; a < vol->attribCount; a++) {
                const DBAttrib* at = vol->GetAttribByIndex(a);
                if (at) printf("   attr id=%u type=%u val=%u (0x%X) str='%s'\n", at->id, at->type, at->value, at->value, at->strValue ? at->strValue : "");
            }
        }
    }
    for (DBVolume* vol = db.GetFirstVolume(); vol; vol = static_cast<DBVolume*>(vol->next)) {
        u16 blkNum = 9999;
        const DBAttrib* a15 = vol->FindAttrib(15);
        if (a15) blkNum = (u16)a15->value;
        const char* name = vol->GetName() ? vol->GetName() : "null";
        if (blkNum == 0 || strstr(name, "Lenny") || strstr(name, "stool") || strstr(name, "club") || strstr(name, "Tabl") || strstr(name, "drum") || strstr(name, "sldrs") || strstr(name, "az114") || strstr(name, "tele")) {
            printf("VOL: name='%s' type=%u subType=%u pos=(%d,%d,%d) blkAttr15=%u\n",
                   name, vol->type, vol->subType, vol->pos.x, vol->pos.y, vol->pos.z, blkNum);
            for (u32 a = 0; a < vol->attribCount; a++) {
                const DBAttrib* at = vol->GetAttribByIndex(a);
                if (at) printf("   attr id=%u type=%u val=%u (0x%X) str='%s'\n", at->id, at->type, at->value, at->value, at->strValue ? at->strValue : "");
            }
        }
    }

    g_display = nullptr;


    g_display = nullptr;

    BlockManager blockMgr;
    g_blockManager = &blockMgr;
    blockMgr.LoadBlocksFunc(blockVolumes);

    // Parse BLKs for petal 1
    std::vector<Entry> blkEntries;
    for (size_t i = 61; i < entries.size(); i++) {
        if (strcmp(entries[i].magic, ".BLK") == 0) {
            blkEntries.push_back(entries[i]);
        } else if (strcmp(entries[i].magic, ".WDB") == 0) {
            break; // next petal
        }
    }
    printf("Found %zu BLK entries in petal 1\n", blkEntries.size());
    std::vector<const u8*> blkPtrs(blkEntries.size());
    std::vector<u32> blkSizes(blkEntries.size());
    for (size_t i = 0; i < blkEntries.size(); i++) {
        blkPtrs[i] = &lcfData[blkEntries[i].offset];
        blkSizes[i] = blkEntries[i].size;
    }

    // Step A: Player spawns in Block 1
    printf("\n--- Step A: Jackie starts in Block 1 (PSP numBlocks=8) ---\n");
    blockMgr.LoadBlocks(1, blkPtrs.data(), blkSizes.data(), static_cast<u32>(blkEntries.size()));
    blockMgr.numBlocks = 8; // On PSP with 16:9 widescreen
    blockMgr.UpdateToBeLoadedList(1);
    blockMgr.UpdateAlreadyLoadedList();

    alignas(Player) static u8 s_mockPlayerBuf[sizeof(Player)] = {};
    Player* mockPlayer = reinterpret_cast<Player*>(s_mockPlayerBuf);
    mockPlayer->pos = { -39168, 8192, -30016 }; // Block 1 Jackie spawn
    mockPlayer->homePos = mockPlayer->pos;
    mockPlayer->blockNum = 1;
    Player::s_player = mockPlayer;

    alignas(AI) static u8 s_mockAIBuf[sizeof(AI)] = {};
    AI* mockAI = reinterpret_cast<AI*>(s_mockAIBuf);
    g_ai = mockAI;

    printf("\n=== Block Connections in Petal 1 ===\n");
    for (size_t i = 0; i < blockMgr.blocks.size(); i++) {
        const Block& b = blockMgr.blocks[i];
        printf("Block %zu (blockNum=%u): prev=%u next=%u branch=%d lodFar=(%d,%d,%d) lodNear=(%d,%d,%d)\n",
               i, b.blockNum, b.GetPrevBlockNumber(), b.GetNextBlockNumber(), (s16)b.unk38,
               b.lodFarX, b.lodFarY, b.lodFarZ, b.lodNearX, b.lodNearY, b.lodNearZ);
    }
    printf("  toBeLoadedCount=%u: ", blockMgr.toBeLoadedCount);
    for (u32 i = 0; i < blockMgr.toBeLoadedCount; i++) printf("%u ", blockMgr.toBeLoadedList[i]);
    printf("\n  drawListCount=%u: ", blockMgr.drawListCount);
    for (u32 i = 0; i < blockMgr.drawListCount; i++) printf("%u ", blockMgr.drawList[i]);
    printf("\n  activeCount=%u: ", blockMgr.activeCount);
    for (u32 i = 0; i < blockMgr.activeCount; i++) printf("%u ", blockMgr.activeList[i]);
    printf("\n");

    // Check Block 0 query from Block 1
    LVector testPos = { -52672, 6912, -26432 };
    printf("From Block 1: InActiveList(0)=%d IsValidBlockNumber(0)=%d\n",
           blockMgr.InActiveList(0), blockMgr.IsValidBlockNumber(0));
    printf("From Block 1: GetBlockNumber(testPos)=%u\n",
           blockMgr.GetBlockNumber(testPos));

    // Step B: Jackie moves to Block 2 (Rooftop)
    printf("\n--- Step B: Jackie moves to Block 2 (Rooftop) ---\n");
    mockPlayer->blockNum = 2;
    mockPlayer->pos = { -33280, 9728, -29632 };
    mockPlayer->homePos = mockPlayer->pos;
    blockMgr.DemandLoading();

    printf("BlockMgr state in Block 2: currentBlock=%u\n", blockMgr.currentBlockNum);
    printf("  toBeLoadedCount=%u: ", blockMgr.toBeLoadedCount);
    for (u32 i = 0; i < blockMgr.toBeLoadedCount; i++) printf("%u ", blockMgr.toBeLoadedList[i]);
    printf("\n  drawListCount=%u: ", blockMgr.drawListCount);
    for (u32 i = 0; i < blockMgr.drawListCount; i++) printf("%u ", blockMgr.drawList[i]);
    printf("\n  activeCount=%u: ", blockMgr.activeCount);
    for (u32 i = 0; i < blockMgr.activeCount; i++) printf("%u ", blockMgr.activeList[i]);
    printf("\n");
    printf("From Block 2: InActiveList(0)=%d IsValidBlockNumber(0)=%d\n",
           blockMgr.InActiveList(0), blockMgr.IsValidBlockNumber(0));

    // Step C: Jackie interacts with right door (teleport111 -> tele1 in Block 0)
    printf("\n--- Step C: Jackie enters teleport111 -> tele1 (Block 0) ---\n");
    LVector doorPos = { -33536, 10240, -28032 };
    Teleporter tele111(&doorPos, AITypes::TT_TELEPORTER);
    tele111.targetPos = { -53504, 6656, -28608 }; // tele1
    tele111.targetAngle = 0;
    tele111.blockNum = 2;

    u16 prevDraw[16] = {}, prevLoad[16] = {}, prevActive[16] = {};
    u32 prevDrawCnt = blockMgr.drawListCount, prevLoadCnt = blockMgr.toBeLoadedCount, prevActCnt = blockMgr.alreadyLoadedCount;
    std::copy_n(blockMgr.drawList, 16, prevDraw);
    std::copy_n(blockMgr.toBeLoadedList, 16, prevLoad);
    std::copy_n(blockMgr.alreadyLoadedList, 16, prevActive);

    tele111.HandleHumanoidCollision(mockPlayer);

    bool sameDraw = MySameBlockSet(prevDraw, prevDrawCnt, blockMgr.drawList, blockMgr.drawListCount);
    bool sameLoad = MySameBlockSet(prevLoad, prevLoadCnt, blockMgr.toBeLoadedList, blockMgr.toBeLoadedCount);
    bool sameAct = MySameBlockSet(prevActive, prevActCnt, blockMgr.alreadyLoadedList, blockMgr.alreadyLoadedCount);
    printf("DemandLoading comparison: sameDraw=%d sameLoad=%d sameAct=%d => windowsUnchanged=%d\n",
           sameDraw, sameLoad, sameAct, (sameDraw && sameLoad && sameAct));

    printf("After teleport: player pos=(%d,%d,%d) blockNum=%u\n",
           mockPlayer->pos.x, mockPlayer->pos.y, mockPlayer->pos.z, mockPlayer->blockNum);
    printf("BlockMgr state after teleport: currentBlock=%u\n", blockMgr.currentBlockNum);
    printf("  toBeLoadedCount=%u: ", blockMgr.toBeLoadedCount);
    for (u32 i = 0; i < blockMgr.toBeLoadedCount; i++) printf("%u ", blockMgr.toBeLoadedList[i]);
    printf("\n  drawListCount=%u: ", blockMgr.drawListCount);
    for (u32 i = 0; i < blockMgr.drawListCount; i++) printf("%u ", blockMgr.drawList[i]);
    printf("\n  activeCount=%u: ", blockMgr.activeCount);
    for (u32 i = 0; i < blockMgr.activeCount; i++) printf("%u ", blockMgr.activeList[i]);
    printf("\n");
    printf("In Block 0: InActiveList(0)=%d IsValidBlockNumber(0)=%d InDrawList(0)=%d\n",
           blockMgr.InActiveList(0), blockMgr.IsValidBlockNumber(0), blockMgr.InDrawList(0));
    printf("AlreadyLoadedBlocksCurrent() = %d, alreadyLoadedCount=%u\n",
           blockMgr.AlreadyLoadedBlocksCurrent(), blockMgr.alreadyLoadedCount);
    for (u32 i = 0; i < blockMgr.alreadyLoadedCount; i++) {
        const Block* b = blockMgr.alreadyLoadedBlocks[i];
        printf("  alreadyLoadedBlocks[%u] = %p (blockNum=%d)\n",
               i, (void*)b, b ? b->blockNum : -1);
        printf("  alreadyLoadedList[%u] = %u\n", i, blockMgr.alreadyLoadedList[i]);
    }
    const Block* b0 = nullptr;
    for (const auto& blk : blockMgr.blocks) {
        if (blk.blockNum == 0) { b0 = &blk; break; }
    }
    printf("Block 0 ptr: %p, bounds: pos=(%d,%d,%d) halfExtNeg=(%d,%d,%d) halfExtPos=(%d,%d,%d)\n",
           (void*)b0,
           b0 ? b0->posX : 0, b0 ? b0->posY : 0, b0 ? b0->posZ : 0,
           b0 ? b0->halfExtNegX : 0, b0 ? b0->halfExtNegY : 0, b0 ? b0->halfExtNegZ : 0,
           b0 ? b0->halfExtPosX : 0, b0 ? b0->halfExtPosY : 0, b0 ? b0->halfExtPosZ : 0);
    LVector posStool = { -52672, 6912, -26432 };
    if (b0) {
        printf("b0->collision = %p\n", (void*)b0->collision);
        if (b0->collision) {
            printf("  collision->boundsMin = (%d, %d, %d)\n",
                   b0->collision->boundsMin.x, b0->collision->boundsMin.y, b0->collision->boundsMin.z);
            printf("  collision->boundsMax = (%d, %d, %d)\n",
                   b0->collision->boundsMax.x, b0->collision->boundsMax.y, b0->collision->boundsMax.z);
        }
        printf("b0->PointInBlock(posStool): %d\n", b0->PointInBlock(&posStool));
    }

    // Step D: Activate Block 0 Things
    printf("\n--- Step D: Test Activate on all 7 Block 0 Entities ---\n");

    // 1. stool1 (Pickup, starts with blockNum=0, modelHash="stool1")
    LVector posStool1 = { -52672, 6912, -26432 };
    Thing thingStool1(&posStool1, 305);
    thingStool1.modelHash = p3dHash("stool1");
    thingStool1.blockNum = 0;
    // Simulate Pickup::UpdatePosition while dormant with newBlock != BLOCK_UNASSIGNED guard:
    {
        u16 newBlock = blockMgr.GetBlockNumber(posStool1);
        if (newBlock != BLOCK_UNASSIGNED) thingStool1.blockNum = newBlock;
    }
    thingStool1.Activate();

    // 2. club (Pickup, starts with blockNum=0, modelHash="club")
    LVector posClub = { -54080, 6720, -26432 };
    Thing thingClub(&posClub, 302);
    thingClub.modelHash = p3dHash("club");
    thingClub.blockNum = 0;
    {
        u16 newBlock = blockMgr.GetBlockNumber(posClub);
        if (newBlock != BLOCK_UNASSIGNED) thingClub.blockNum = newBlock;
    }
    thingClub.Activate();

    // 3. Lenny114 (Humanoid, type 21 Elite, modelHash="ELITELOHIER", blockNum=0)
    LVector posLenny = { -52608, 6784, -26752 };
    Thing thingLenny(&posLenny, 21);
    thingLenny.modelHash = p3dHash("ELITELOHIER");
    thingLenny.blockNum = 0;
    thingLenny.Activate();

    // 4. TablAdwo (Table, type 468, modelHash="TablAdwo", blockNum=0)
    LVector posTabl = { -52595, 6851, -27352 };
    Thing thingTabl(&posTabl, 468);
    thingTabl.modelHash = p3dHash("TablAdwo");
    thingTabl.blockNum = 0;
    thingTabl.Activate();

    // 5. drum_E_1 (KnockDown, type 424, modelHash="drum_E_1", no attrib 15 -> BLOCK_UNASSIGNED)
    LVector posDrum = { -55040, 6656, -28032 };
    Thing thingDrum(&posDrum, 424);
    thingDrum.modelHash = p3dHash("drum_E_1");
    thingDrum.blockNum = BLOCK_UNASSIGNED; // unassigned in WDB
    thingDrum.Activate();

    // 6. sldrs_B (Door, type 463, modelHash="sldrs_B", no attrib 15 -> BLOCK_UNASSIGNED)
    LVector posSldrsB = { -54528, 8192, -25152 };
    Thing thingSldrsB(&posSldrsB, 463);
    thingSldrsB.modelHash = p3dHash("sldrs_B");
    thingSldrsB.blockNum = BLOCK_UNASSIGNED; // unassigned in WDB
    thingSldrsB.Activate();

    // 7. sldrs (Door, type 463, modelHash="sldrs", no attrib 15 -> BLOCK_UNASSIGNED)
    LVector posSldrs = { -52480, 8192, -25152 };
    Thing thingSldrs(&posSldrs, 463);
    thingSldrs.modelHash = p3dHash("sldrs");
    thingSldrs.blockNum = BLOCK_UNASSIGNED; // unassigned in WDB
    thingSldrs.Activate();

    printf("\nActivation Results:\n");
    printf("  thingStool1: blockNum=%u flags=0x%08X (activated=%d, modelCreated=%d)\n",
           thingStool1.blockNum, thingStool1.flags,
           (thingStool1.flags & TF_ACTIVATED) != 0, (thingStool1.flags & TF_MODEL_CREATED) != 0);
    printf("  thingClub:   blockNum=%u flags=0x%08X (activated=%d, modelCreated=%d)\n",
           thingClub.blockNum, thingClub.flags,
           (thingClub.flags & TF_ACTIVATED) != 0, (thingClub.flags & TF_MODEL_CREATED) != 0);
    printf("  thingLenny:  blockNum=%u flags=0x%08X (activated=%d, modelCreated=%d)\n",
           thingLenny.blockNum, thingLenny.flags,
           (thingLenny.flags & TF_ACTIVATED) != 0, (thingLenny.flags & TF_MODEL_CREATED) != 0);
    printf("  thingTabl:   blockNum=%u flags=0x%08X (activated=%d, modelCreated=%d)\n",
           thingTabl.blockNum, thingTabl.flags,
           (thingTabl.flags & TF_ACTIVATED) != 0, (thingTabl.flags & TF_MODEL_CREATED) != 0);
    printf("  thingDrum:   blockNum=%u flags=0x%08X (activated=%d, modelCreated=%d)\n",
           thingDrum.blockNum, thingDrum.flags,
           (thingDrum.flags & TF_ACTIVATED) != 0, (thingDrum.flags & TF_MODEL_CREATED) != 0);
    printf("  thingSldrsB: blockNum=%u flags=0x%08X (activated=%d, modelCreated=%d)\n",
           thingSldrsB.blockNum, thingSldrsB.flags,
           (thingSldrsB.flags & TF_ACTIVATED) != 0, (thingSldrsB.flags & TF_MODEL_CREATED) != 0);
    printf("  thingSldrs:  blockNum=%u flags=0x%08X (activated=%d, modelCreated=%d)\n",
           thingSldrs.blockNum, thingSldrs.flags,
           (thingSldrs.flags & TF_ACTIVATED) != 0, (thingSldrs.flags & TF_MODEL_CREATED) != 0);

    // Verify all 7 succeeded
    assert(thingStool1.blockNum == 0 && (thingStool1.flags & TF_MODEL_CREATED));
    assert(thingClub.blockNum == 0 && (thingClub.flags & TF_MODEL_CREATED));
    assert(thingLenny.blockNum == 0 && (thingLenny.flags & TF_MODEL_CREATED));
    assert(thingTabl.blockNum == 0 && (thingTabl.flags & TF_MODEL_CREATED));
    assert(thingDrum.blockNum == 0 && (thingDrum.flags & TF_MODEL_CREATED));
    assert(thingSldrsB.blockNum == 0 && (thingSldrsB.flags & TF_MODEL_CREATED));
    assert(thingSldrs.blockNum == 0 && (thingSldrs.flags & TF_MODEL_CREATED));

    printf("\n>>> ALL 7 ENTITIES IN BLOCK 0 ACTIVATED AND MODEL_CREATED SUCCESSFULLY! <<<\n");

    return 0;
}
