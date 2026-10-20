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
#include "ai/fevolume.h"
#include "ai/teleporter.h"
#include "ai/ladder.h"
#include "ai/platform.h"
#include "fe/femenumgr.h"
#include "fe/hud.h"
#include "gen/ai.h"
#include "gen/director.h"
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
void AI::PopulateBlock() {}
void AI::UnpopulateBlock() {}

class Display {
public:
    Camera* GetCamera() { return nullptr; }
};

void WEffect_PopulateWEffects() {}
int RP_ZCullGClip(tGeometry*, const LVector*) { return 0; }
int RP_ZCullGMFog(tGeometry*, const LVector*, u16, u16, u32) { return 0; }
void UpdateUVPrimData(int, tPrimGeom*) {}
void UpdateCBVPrimData(int, tPrimGeom*) {}
void InvalidatePspPrimGeomCache(tPrimGeom*) {}
bool UpdatePspPrimGeomDynamicData(tPrimGeom*, bool, bool) { return true; }

void HUD::DisplayTake(int, int) {}
void hdDestSelect::ShowLevel(int) {}

// Geometry ownership stubs for Block parsing
const u8* tGeometry::GetVertexList() const { return vertexList; }
void tGeometry::SetVertexList(const u8* value) { vertexList = value; }
tPrimGeom::~tPrimGeom() { delete[] ownedRawData; }

CollisionSector::CollisionSector() {}
void CollisionSector::Load(unsigned int*) {}
void CollisionSector::Unload() {}
bool CollisionSector::LedgePrototype(const LVector&, const LVector&, s32, s32, LVector&, LVector&, u16&, s32) {
    return false;
}
CollisionSector g_collisionSectors[MAX_COLLISION_SECTORS];

// Minimal stubs for standalone test execution
Player* Player::s_player = nullptr;
BlockManager* g_blockManager = nullptr;
Game* g_game = nullptr;
feMenuMgr* g_feMenuMgr = nullptr;
HUD* g_hud = nullptr;
AI* g_ai = nullptr;
Display* g_display = nullptr;
Director* g_director = nullptr;
u8 g_arrowInside = 0;

static s32 s_dummyNISLadderScript = 0;
s32* Director::GetNISLadder1Script() {
    return &s_dummyNISLadderScript;
}
void Director::SetCodeSnip(s32* snip, Thing* /*thing*/) {
    codeSnipPtr = snip;
    scriptState = 537;
}

s32 Platform::AtEndOfPath() const {
    return 0;
}

static FrontEndVolume* s_lastShowLevelVolume = nullptr;
static Humanoid* s_lastShowLevelHumanoid = nullptr;
static World* s_mockWorld = nullptr;

void feMenuMgr::ShowLevel(FrontEndVolume* vol, Humanoid* hum) {
    s_lastShowLevelVolume = vol;
    s_lastShowLevelHumanoid = hum;
    if (g_game) {
        g_game->SetState(GameState::LocationMenu);
    }
}

// Game stubs
World* Game::GetWorld() const { return s_mockWorld; }
void Game::SetState(GameState st) { state = st; }

// Minimal Obstacle implementations needed for test
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

// Thing stubs
Thing::Thing(const LVector* initialPos, u16 type) {
    if (initialPos) pos = *initialPos;
    thingType = type;
}
Thing::~Thing() {}
void Thing::ClearFloorHeight() {}
void Thing::Deactivate() {}
void Thing::Activate() {}
void Thing::AnalyzeMesh(DBRoot*) {}
void Thing::CreateModel(char const*) {}
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

// Humanoid member stubs (called by Teleporter, FrontEndVolume, and Ladder)
void Humanoid::DeleteRightHandObj() {}
void Humanoid::DeleteLeftHandObj() {}
void Humanoid::FaceAngleY(s32, s32) {}
void Humanoid::SetActionState(u32 action, s32) { actionState = action; }
void Humanoid::PrepareLedgeLatch(const LVector&, const LVector&) {}
bool Humanoid::FacePointDesired(const LVector&) { return true; }
void Humanoid::FacePoint(const LVector&, s32) {}

static bool CheckXZStaticBoxCylinder(
    const LVector& obsPos, const tagCollisionBox& box, s32 orientY,
    const LVector& cylPos, const tagCollisionCylinder& cyl) {
    s32 minX = box.minX - cyl.radius;
    s32 maxX = box.maxX + cyl.radius;
    s32 maxZ = box.maxZ + cyl.radius;
    s32 minZ = box.minZ - cyl.radius;

    LVector delta;
    delta.x = cylPos.x - obsPos.x;
    delta.y = cylPos.y - obsPos.y;
    delta.z = cylPos.z - obsPos.z;

    s32 sinV = rmSin16(orientY);
    s32 cosV = rmSin16(orientY + 0x4000);

    s32 localX = (s32)(((s64)cosV * delta.x) >> 16) + (s32)((-(s64)sinV * delta.z) >> 16);
    s32 localZ = (s32)(((s64)sinV * delta.x) >> 16) + (s32)(((s64)cosV * delta.z) >> 16);

    return (localX >= minX && maxX >= localX && localZ >= minZ && maxZ >= localZ);
}

static bool CheckYPass(const LVector& obsPos, const tagCollisionBox& box,
                      const LVector& cylPos, const tagCollisionCylinder& cyl) {
    s32 dy = cylPos.y - obsPos.y;
    s32 yLow = (s32)box.minY - cyl.upperY;
    s32 yHigh = (s32)box.maxY - cyl.lowerY;
    return (dy >= yLow && yHigh >= dy);
}

static void require(bool condition, const char* msg) {
    if (!condition) {
        std::fprintf(stderr, "TEST FAILED: %s\n", msg);
        std::exit(1);
    }
}

int main() {
    std::printf("Running Hub Sewer Tunnel Trigger Autotest...\n");

    // 1. Verify FrontEndVolume constructor sets TF_MODEL_CREATED
    LVector dummyPos = { 0, 0, 0 };
    FrontEndVolume testVol(&dummyPos, AITypes::TT_BOSS);
    require((testVol.flags & TF_MODEL_CREATED) != 0,
            "FrontEndVolume must have TF_MODEL_CREATED set in constructor");

    // 2. Setup mock Level 7 World and Blocks
    BlockManager blockMgr;
    g_blockManager = &blockMgr;

    // Create 7 blocks (0 to 6). Block 6 is the Sewer hub area.
    std::vector<DBVolume*> blockVolumes;
    DBVolume bVol6;
    bVol6.pos = { -14720, 9664, 26176 };
    bVol6.bboxMin = { -25472, -576, 17792 };
    bVol6.bboxMax = { -3968, 19904, 34560 };
    bVol6.attribCount = 1;
    bVol6.attribs = new DBAttrib[1]{ { 15, 1, 6, nullptr } };
    blockVolumes.push_back(&bVol6);

    DBVolume bVol3;
    bVol3.pos = { -3072, 1536, -2816 };
    bVol3.bboxMin = { -4608, -8704, -8448 };
    bVol3.bboxMax = { -1536, 11776, 2816 };
    bVol3.attribCount = 1;
    bVol3.attribs = new DBAttrib[1]{ { 15, 1, 3, nullptr } };
    blockVolumes.push_back(&bVol3);

    blockMgr.LoadBlocksFunc(blockVolumes);
    require(blockMgr.GetNumBlocks() == 2, "BlockManager must have 2 blocks initialized");

    // Verify GetBlockNumberAllBlocks correctly identifies Block 6
    LVector sewerInPos = { -11904, 9984, 26496 };
    u16 resolvedBlock = blockMgr.GetBlockNumberAllBlocks(sewerInPos);
    require(resolvedBlock == 6, "GetBlockNumberAllBlocks must resolve sewerIN to Block 6");

    // Verify that GetBlockNumber (windowed) returns BLOCK_UNASSIGNED when block 6 is not in active window
    u16 windowedBlock = blockMgr.GetBlockNumber(sewerInPos);
    require(windowedBlock == BLOCK_UNASSIGNED, "GetBlockNumber must return BLOCK_UNASSIGNED before activation");

    // 3. Test Teleporter to Sewer Hub (Block 6)
    alignas(Game) static u8 s_mockGameBuf[sizeof(Game)] = {};
    g_game = reinterpret_cast<Game*>(s_mockGameBuf);

    alignas(World) static u8 s_mockWorldBuf[sizeof(World)] = {};
    World* mockWorld = reinterpret_cast<World*>(s_mockWorldBuf);
    s32 mockLevelList[2] = { 7, 1 };
    mockWorld->levelList = mockLevelList;
    mockWorld->levelCount = 1;
    mockWorld->currentLevelIndex = 0;
    s_mockWorld = mockWorld;

    alignas(feMenuMgr) static u8 s_mockFeMenuMgrBuf[sizeof(feMenuMgr)] = {};
    g_feMenuMgr = reinterpret_cast<feMenuMgr*>(s_mockFeMenuMgrBuf);

    alignas(Player) static u8 s_mockPlayerBuf[sizeof(Player)] = {};
    Player* mockPlayer = reinterpret_cast<Player*>(s_mockPlayerBuf);
    static void* s_mockPlayerVtable[64] = {};
    s_mockPlayerVtable[0xa0 / sizeof(void*)] = (void*)+[](Humanoid* h, u32 act, s32) {
        h->actionState = act;
    };
    *reinterpret_cast<void**>(mockPlayer) = s_mockPlayerVtable;
    mockPlayer->collBboxMin = { 175, 0, 768 };
    mockPlayer->pos = { -2560, -1408, -2304 }; // at manhole in block 3
    mockPlayer->homePos = mockPlayer->pos;
    mockPlayer->blockNum = 3;
    Player::s_player = mockPlayer;

    Teleporter sewerTeleporter(&dummyPos, AITypes::TT_TELEPORTER);
    sewerTeleporter.targetPos = sewerInPos;
    sewerTeleporter.targetAngle = 0;
    sewerTeleporter.blockNum = 3;

    blockMgr.flag3 = 1;
    blockMgr.currentBlockNum = 3;
    sewerTeleporter.HandleHumanoidCollision(mockPlayer);

    require(mockPlayer->blockNum == 6, "Teleporter must update mockPlayer blockNum to 6");
    require(mockPlayer->pos.x == sewerInPos.x && mockPlayer->pos.y == sewerInPos.y && mockPlayer->pos.z == sewerInPos.z,
            "Player position must be at targetPos after teleport");
    require(blockMgr.currentBlockNum == 6, "Teleporter must invoke DemandLoading() to activate Block 6");

    // 4. Test Sewer Tunnel 1 (Sewer 1, levelCode = 30)
    // Position: (-14354, 9472, 25447), size: (2048, 1536, 2304), f40: (0, 0, 0)
    DBVolume dbVol30;
    dbVol30.pos = { -14354, 9472, 25447 };
    dbVol30.bboxMin = { -14354 - 1024, 9472 - 768, 25447 - 1152 };
    dbVol30.bboxMax = { -14354 + 1024, 9472 + 768, 25447 + 1152 };
    dbVol30.attribCount = 1;
    dbVol30.attribs = new DBAttrib[1]{ { 8, 1, 30, nullptr } };

    FrontEndVolume tunnel1(&dbVol30.pos, AITypes::TT_BOSS);
    tunnel1.AnalyzeMesh(&dbVol30);
    require(tunnel1.levelCode == 30, "Tunnel 1 levelCode must be 30");
    require(tunnel1.collBox.minY <= -3000, "Tunnel 1 minY must extend downward to cover shaft");
    require(tunnel1.collBox.maxX >= 1400, "Tunnel 1 maxX must widen entrance toward hub corridor");

    // Test collision along approach, ramp edge, and falling into shaft for Tunnel 1
    tagCollisionCylinder humCyl = { 175, 0, 768 };
    LVector testPosT1_Approach = { -13100, 9472, 25447 }; // on corridor before ramp end
    LVector testPosT1_RampEdge = { -13696, 8960, 25447 }; // exactly at end of floor ramp
    LVector testPosT1_Falling  = { -14000, 7500, 25447 }; // falling down the shaft (-1972 from volume center)

    require(CheckXZStaticBoxCylinder(tunnel1.pos, tunnel1.collBox, tunnel1.orientation.y, testPosT1_Approach, humCyl),
            "Tunnel 1 must detect approach in corridor");
    require(CheckYPass(tunnel1.pos, tunnel1.collBox, testPosT1_Approach, humCyl),
            "Tunnel 1 must pass Y check on approach");

    require(CheckXZStaticBoxCylinder(tunnel1.pos, tunnel1.collBox, tunnel1.orientation.y, testPosT1_RampEdge, humCyl),
            "Tunnel 1 must detect player at ramp edge");
    require(CheckYPass(tunnel1.pos, tunnel1.collBox, testPosT1_RampEdge, humCyl),
            "Tunnel 1 must pass Y check at ramp edge");

    require(CheckXZStaticBoxCylinder(tunnel1.pos, tunnel1.collBox, tunnel1.orientation.y, testPosT1_Falling, humCyl),
            "Tunnel 1 must detect player falling into shaft");
    require(CheckYPass(tunnel1.pos, tunnel1.collBox, testPosT1_Falling, humCyl),
            "Tunnel 1 must pass Y check even when falling deep in shaft");

    // Test activation of Tunnel 1
    s_lastShowLevelVolume = nullptr;
    tunnel1.HandleHumanoidCollision(mockPlayer);
    require(s_lastShowLevelVolume == &tunnel1, "Tunnel 1 must trigger ShowLevel");
    require(g_game->GetState() == GameState::LocationMenu, "Game state must transition to LocationMenu");

    // 5. Test Sewer Tunnel 2 (Sewer 2, levelCode = 31)
    // Position: (-7932, 9472, 25470), size: (2048, 1536, 2304), f40: (0, 0, 0)
    DBVolume dbVol31;
    dbVol31.pos = { -7932, 9472, 25470 };
    dbVol31.bboxMin = { -7932 - 1024, 9472 - 768, 25470 - 1152 };
    dbVol31.bboxMax = { -7932 + 1024, 9472 + 768, 25470 + 1152 };
    dbVol31.attribCount = 1;
    dbVol31.attribs = new DBAttrib[1]{ { 8, 1, 31, nullptr } };

    FrontEndVolume tunnel2(&dbVol31.pos, AITypes::TT_BOSS);
    tunnel2.AnalyzeMesh(&dbVol31);
    require(tunnel2.levelCode == 31, "Tunnel 2 levelCode must be 31");
    require(tunnel2.collBox.minY <= -3000, "Tunnel 2 minY must extend downward to cover shaft");
    require(tunnel2.collBox.minX <= -1400, "Tunnel 2 minX must widen entrance toward hub corridor");

    LVector testPosT2_Approach = { -9100, 9472, 25470 };
    LVector testPosT2_RampEdge = { -8576, 8960, 25470 };
    LVector testPosT2_Falling  = { -8200, 7500, 25470 };

    require(CheckXZStaticBoxCylinder(tunnel2.pos, tunnel2.collBox, tunnel2.orientation.y, testPosT2_Approach, humCyl),
            "Tunnel 2 must detect approach in corridor");
    require(CheckYPass(tunnel2.pos, tunnel2.collBox, testPosT2_Approach, humCyl),
            "Tunnel 2 must pass Y check on approach");

    require(CheckXZStaticBoxCylinder(tunnel2.pos, tunnel2.collBox, tunnel2.orientation.y, testPosT2_RampEdge, humCyl),
            "Tunnel 2 must detect player at ramp edge");
    require(CheckYPass(tunnel2.pos, tunnel2.collBox, testPosT2_RampEdge, humCyl),
            "Tunnel 2 must pass Y check at ramp edge");

    require(CheckXZStaticBoxCylinder(tunnel2.pos, tunnel2.collBox, tunnel2.orientation.y, testPosT2_Falling, humCyl),
            "Tunnel 2 must detect player falling into shaft");
    require(CheckYPass(tunnel2.pos, tunnel2.collBox, testPosT2_Falling, humCyl),
            "Tunnel 2 must pass Y check even when falling deep in shaft");

    // Test activation of Tunnel 2
    s_lastShowLevelVolume = nullptr;
    tunnel2.HandleHumanoidCollision(mockPlayer);
    require(s_lastShowLevelVolume == &tunnel2, "Tunnel 2 must trigger ShowLevel");

    // 6. Test Sewer Tunnel 3 (Sewer 3, levelCode = 32)
    // Position: (-11006, 9472, 28932), size: (2048, 1536, 2304), f40: (0, -16384, 0)
    DBVolume dbVol32;
    dbVol32.pos = { -11006, 9472, 28932 };
    dbVol32.field44 = -16384;
    dbVol32.bboxMin = { -11006 - 1024, 9472 - 768, 28932 - 1152 };
    dbVol32.bboxMax = { -11006 + 1024, 9472 + 768, 28932 + 1152 };
    dbVol32.attribCount = 1;
    dbVol32.attribs = new DBAttrib[1]{ { 8, 1, 32, nullptr } };

    FrontEndVolume tunnel3(&dbVol32.pos, AITypes::TT_BOSS);
    tunnel3.AnalyzeMesh(&dbVol32);
    require(tunnel3.levelCode == 32, "Tunnel 3 levelCode must be 32");
    require(tunnel3.collBox.minY <= -3000, "Tunnel 3 minY must extend downward to cover shaft");
    require(tunnel3.collBox.minX <= -1400, "Tunnel 3 minX must widen entrance toward hub corridor");

    LVector testPosT3_Approach = { -11006, 9472, 27600 }; // approaching from corridor
    LVector testPosT3_FloorEnd = { -11006, 8960, 27776 }; // at end of floor 5
    LVector testPosT3_Falling  = { -11006, 7500, 28200 }; // falling into shaft

    require(CheckXZStaticBoxCylinder(tunnel3.pos, tunnel3.collBox, tunnel3.orientation.y, testPosT3_Approach, humCyl),
            "Tunnel 3 must detect approach in corridor");
    require(CheckYPass(tunnel3.pos, tunnel3.collBox, testPosT3_Approach, humCyl),
            "Tunnel 3 must pass Y check on approach");

    require(CheckXZStaticBoxCylinder(tunnel3.pos, tunnel3.collBox, tunnel3.orientation.y, testPosT3_FloorEnd, humCyl),
            "Tunnel 3 must detect player at floor end");
    require(CheckYPass(tunnel3.pos, tunnel3.collBox, testPosT3_FloorEnd, humCyl),
            "Tunnel 3 must pass Y check at floor end");

    require(CheckXZStaticBoxCylinder(tunnel3.pos, tunnel3.collBox, tunnel3.orientation.y, testPosT3_Falling, humCyl),
            "Tunnel 3 must detect player falling into shaft");
    require(CheckYPass(tunnel3.pos, tunnel3.collBox, testPosT3_Falling, humCyl),
            "Tunnel 3 must pass Y check even when falling deep in shaft");

    // Test activation of Tunnel 3
    s_lastShowLevelVolume = nullptr;
    tunnel3.HandleHumanoidCollision(mockPlayer);
    require(s_lastShowLevelVolume == &tunnel3, "Tunnel 3 must trigger ShowLevel");

    // 7. Test Hub Sewer Tunnel Exit Ladder (roof20 in Block 6)
    // Position: (-12416, 11392, 26496), size: (256, 2048, 256), hatchYTrigger = 11000, teleportTargetCRC = p3dHash("sOUT")
    DBVolume dbVolLadder;
    dbVolLadder.pos = { -12416, 11392, 26496 };
    dbVolLadder.bboxMin = { -12416 - 128, 11392 - 1024, 26496 - 128 };
    dbVolLadder.bboxMax = { -12416 + 128, 11392 + 1024, 26496 + 128 };
    dbVolLadder.attribCount = 5;
    dbVolLadder.attribs = new DBAttrib[5]{
        { 6, 1, 180, nullptr },          // faceAngle = 180
        { 7, 0, 0, "TRUE" },             // hatchEnabled = 1
        { 10, 0, 0, "sOUT" },            // teleportTargetCRC = p3dHash("sOUT")
        { 12, 1, 11000, nullptr },       // hatchYTrigger = 11000
        { 15, 1, 6, nullptr }            // block 6
    };

    Ladder ladder(&dbVolLadder.pos, AITypes::TT_LADDER);
    require((ladder.flags & TF_MODEL_CREATED) != 0, "Ladder must have TF_MODEL_CREATED set in constructor");

    ladder.AnalyzeMesh(&dbVolLadder);
    require(ladder.teleportTargetCRC == (s32)p3dHash("sOUT"), "Ladder must have teleportTargetCRC for sOUT");
    require(ladder.hatchYTrigger == 11000, "Ladder must have hatchYTrigger = 11000");
    require(ladder.collBox.minY <= -3000, "Ladder minY must extend down to cover floor level");
    require(ladder.collBox.minX <= -500 && ladder.collBox.maxX >= 500,
            "Ladder X bounds must cover approach corridor");

    // Verify broadphase / narrowphase intersection checks pass for humanoid on the sewer floor
    LVector testPosLadder_Floor1 = { -12100, 9984, 26496 }; // Floor 1 in front of ladder
    LVector testPosLadder_Floor2 = { -12416, 9472, 26496 }; // Floor 2 lower sewer level
    require(CheckXZStaticBoxCylinder(ladder.pos, ladder.collBox, ladder.orientation.y, testPosLadder_Floor1, humCyl),
            "Ladder must detect approach in corridor along X");
    require(CheckYPass(ladder.pos, ladder.collBox, testPosLadder_Floor1, humCyl),
            "Ladder must pass Y check on Floor 1");
    require(CheckXZStaticBoxCylinder(ladder.pos, ladder.collBox, ladder.orientation.y, testPosLadder_Floor2, humCyl),
            "Ladder must detect approach on Floor 2");
    require(CheckYPass(ladder.pos, ladder.collBox, testPosLadder_Floor2, humCyl),
            "Ladder must pass Y check on lower Floor 2");

    ladder.Reset();
    require(ladder.state == 0, "Ladder initially resets to state 0");

    alignas(Director) static u8 s_mockDirectorBuf[sizeof(Director)] = {};
    g_director = reinterpret_cast<Director*>(s_mockDirectorBuf);
    g_director->codeSnipPtr = nullptr;
    g_director->scriptState = 0;

    // Test 7a: Player on tunnel floor beneath ladder (Y=9984, while ladder mesh base is 10368)
    // The player's vertical reach must allow grabbing the bottom rung
    mockPlayer->pos = { -12100, 9984, 26496 };
    mockPlayer->homePos = mockPlayer->pos;
    mockPlayer->actionState = 0; // AS_STAND
    mockPlayer->commandBits = (1 << 7); // Grab button

    ladder.HandleHumanoidCollision(mockPlayer);
    require(ladder.state == 2, "Passive ladder must transition to state 2 upon collision check");
    require(mockPlayer->actionState == AS_LADDER_CLIMB_UP,
            "Jackie must grab ladder (roof20) from the sewer floor beneath minY");
    require(mockPlayer->homePos.x == ladder.pos.x && mockPlayer->homePos.z == ladder.pos.z,
            "Jackie XZ must align to ladder upon grab");

    // Test 7a2: Grab with D-pad UP walking into ladder
    mockPlayer->pos = { -12200, 9472, 26496 };
    mockPlayer->homePos = mockPlayer->pos;
    mockPlayer->actionState = 0;
    mockPlayer->commandBits = (1 << 2); // D-pad UP
    ladder.HandleHumanoidCollision(mockPlayer);
    require(mockPlayer->actionState == AS_LADDER_CLIMB_UP,
            "Jackie must grab ladder with D-pad UP");

    // Test 7a3: Grab during jumping into ladder
    mockPlayer->pos = { -12300, 9800, 26496 };
    mockPlayer->homePos = mockPlayer->pos;
    mockPlayer->actionState = AS_JUMP;
    mockPlayer->commandBits = 0;
    ladder.HandleHumanoidCollision(mockPlayer);
    require(mockPlayer->actionState == AS_LADDER_CLIMB_UP,
            "Jackie must grab ladder while jumping into it");

    // Test 7b: Player transitions to AS_LADDER_CLIMBING (27) at bottom (pos.y = 10368)
    mockPlayer->actionState = AS_LADDER_CLIMBING;
    mockPlayer->pos = { -12416, 10368, 26496 };
    mockPlayer->homePos = mockPlayer->pos;
    ladder.cutscenePending = 0;

    ladder.HandleHumanoidCollision(mockPlayer);
    require(ladder.cutscenePending == 0,
            "Ladder cutscene must NOT trigger at bottom (10368 < hatchYTrigger 11000)");

    // Test 7c: Player climbs up to hatchYTrigger (11000)
    mockPlayer->pos = { -12416, 11000, 26496 };
    mockPlayer->homePos = mockPlayer->pos;

    ladder.HandleHumanoidCollision(mockPlayer);
    require(ladder.cutscenePending == 1,
            "Ladder cutscene must trigger when player climbs to hatchYTrigger (11000)");
    require(g_director->codeSnipPtr == Director::GetNISLadder1Script(),
            "Director must receive NIS ladder script on cutscene trigger");

    std::printf("PASS: Hub sewer tunnel activation, teleporter, and ladder grab tests succeeded!\n");
    return 0;
}
